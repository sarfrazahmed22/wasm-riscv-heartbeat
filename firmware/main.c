#include <stdint.h>
#include <string.h>

/* ── UART registers (SiFive FE310) ─────────────────────────── */
#define UART0_TXDATA  (*(volatile unsigned int*)0x10013000)
#define UART0_RXDATA  (*(volatile unsigned int*)0x10013004)
#define UART0_TXCTRL  (*(volatile unsigned int*)0x10013008)
#define UART0_RXCTRL  (*(volatile unsigned int*)0x1001300C)

void uart_init() {
    UART0_TXCTRL = 1;  /* enable transmitter */
    UART0_RXCTRL = 1;  /* enable receiver */
}

void uart_putc(char c) {
    while (UART0_TXDATA & 0x80000000);
    UART0_TXDATA = c;
}

void uart_print(const char *s) {
    while (*s) uart_putc(*s++);
}

void uart_print_hex(uint32_t v) {
    const char *h = "0123456789ABCDEF";
    uart_print("0x");
    for (int i = 28; i >= 0; i -= 4)
        uart_putc(h[(v >> i) & 0xF]);
}

/* Non-blocking UART receive: returns 1 and sets *out if a byte was
   available, returns 0 otherwise (bit 31 = empty flag). */
int uart_try_getc(char *out) {
    uint32_t val = UART0_RXDATA;
    if (val & 0x80000000) return 0;
    *out = (char)(val & 0xFF);
    return 1;
}

/* ── Wasm3 includes ─────────────────────────────────────────── */
#include "wasm/wasm3/source/wasm3.h"
#include "wasm/wasm3/source/m3_env.h"

/* ── Embedded WASM module ────────────────────────────────────── */
#include "wasm/heartbeat_wasm.c"

#define WASM_STACK_SLOTS  1024

m3ApiRawFunction(m3_uart_write) {
    m3ApiGetArgMem(const char*, msg);
    uart_print(msg);
    m3ApiSuccess();
}

void delay() {
    for (volatile int i = 0; i < 500000; i++);
}

/* ── WASM update region in RAM ──────────────────────────────── */
#define WASM_UPDATE_FLAG  (*(volatile uint32_t*)0x80002000)
#define WASM_UPDATE_LEN   (*(volatile uint32_t*)0x80002004)
#define WASM_UPDATE_DATA  ((uint8_t*)0x80002008)
#define WASM_UPDATE_MAGIC 0xDEADBEEF
#define WASM_UPDATE_MAX   8192

/* ── UART receive state machine for dynamic WASM updates ──────
   Protocol from server.py:
     "WASMUPDATE" (10 bytes) + size (4 bytes LE) + <size> bytes of .wasm
   On completion, payload is stored at WASM_UPDATE_DATA and
   WASM_UPDATE_FLAG is set to WASM_UPDATE_MAGIC.                  */

static const char UPD_MARKER[] = "WASMUPDATE";
#define UPD_MARKER_LEN 10

typedef enum {
    UPD_WAIT_MARKER,
    UPD_READ_SIZE,
    UPD_READ_DATA
} update_state_t;

static update_state_t upd_state      = UPD_WAIT_MARKER;
static uint32_t       upd_marker_pos = 0;
static uint32_t       upd_size       = 0;
static uint32_t       upd_size_bytes = 0;
static uint32_t       upd_data_pos   = 0;

/* Returns 1 the moment a complete WASM image has been received.
   Call repeatedly from the main loop (non-blocking).            */
int check_uart_update(void) {
    char c;
    while (uart_try_getc(&c)) {
        switch (upd_state) {
        case UPD_WAIT_MARKER:
            if (c == UPD_MARKER[upd_marker_pos]) {
                upd_marker_pos++;
                if (upd_marker_pos == UPD_MARKER_LEN) {
                    upd_state      = UPD_READ_SIZE;
                    upd_size       = 0;
                    upd_size_bytes = 0;
                }
            } else {
                upd_marker_pos = (c == UPD_MARKER[0]) ? 1 : 0;
            }
            break;

        case UPD_READ_SIZE:
            upd_size |= ((uint32_t)(uint8_t)c) << (8 * upd_size_bytes);
            upd_size_bytes++;
            if (upd_size_bytes == 4) {
                if (upd_size == 0 || upd_size > WASM_UPDATE_MAX) {
                    upd_state      = UPD_WAIT_MARKER;
                    upd_marker_pos = 0;
                } else {
                    upd_state    = UPD_READ_DATA;
                    upd_data_pos = 0;
                }
            }
            break;

        case UPD_READ_DATA:
            WASM_UPDATE_DATA[upd_data_pos++] = (uint8_t)c;
            if (upd_data_pos == upd_size) {
                WASM_UPDATE_LEN  = upd_size;
                WASM_UPDATE_FLAG = WASM_UPDATE_MAGIC;
                upd_state      = UPD_WAIT_MARKER;
                upd_marker_pos = 0;
                return 1;
            }
            break;
        }
    }
    return 0;
}

/* m3_ResetHeap (in m3_core.c) rewinds Wasm3's internal fixed heap so
   a fresh module can be loaded without resetting the CPU.         */
extern void m3_ResetHeap(void);

void main() {
    uart_init();
    uart_print("BOOT: Wasm3 heartbeat starting\r\n");

    while (1) {
        const uint8_t *wasm_data = heartbeat_wasm;
        uint32_t       wasm_len  = heartbeat_wasm_len;

        if (WASM_UPDATE_FLAG == WASM_UPDATE_MAGIC) {
            uart_print("UPDATE: loading new WASM module from RAM\r\n");
            wasm_data = WASM_UPDATE_DATA;
            wasm_len  = WASM_UPDATE_LEN;
            WASM_UPDATE_FLAG = 0;
        }

        uart_print("WASM len: ");
        uart_print_hex(wasm_len);
        uart_print("\r\n");

        IM3Environment env = m3_NewEnvironment();
        if (!env) { uart_print("ERR: env\r\n"); while(1); }

        IM3Runtime runtime = m3_NewRuntime(env, WASM_STACK_SLOTS, NULL);
        if (!runtime) { uart_print("ERR: runtime\r\n"); while(1); }

        IM3Module module;
        M3Result result = m3_ParseModule(env, &module, wasm_data, wasm_len);
        if (result) {
            uart_print("ERR: ParseModule: "); uart_print(result); uart_print("\r\n");
            while(1);
        }

        result = m3_LoadModule(runtime, module);
        if (result) {
            uart_print("ERR: LoadModule: "); uart_print(result); uart_print("\r\n");
            while(1);
        }

        m3_LinkRawFunction(module, "env", "uart_write", "v(*)", &m3_uart_write);

        IM3Function f_heartbeat;
        result = m3_FindFunction(&f_heartbeat, runtime, "heartbeat");
        if (result) {
            uart_print("ERR: FindFunction: "); uart_print(result); uart_print("\r\n");
            while(1);
        }

        uart_print("WASM: runtime ready!\r\n");

        /* ── Run loop: execute heartbeat() and watch for updates ── */
        int got_update = 0;
        while (!got_update) {
            result = m3_CallV(f_heartbeat);
            if (result) {
                uart_print("ERR: call: "); uart_print(result); uart_print("\r\n");
            }
            delay();
            got_update = check_uart_update();
        }

        uart_print("UPDATE: received, restarting WASM runtime...\r\n");
        m3_ResetHeap();
    }
}
