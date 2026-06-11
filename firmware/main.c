#include <stdint.h>
#include <string.h>

/* ── UART registers (SiFive FE310) ─────────────────────────── */
#define UART0_TXDATA  (*(volatile unsigned int*)0x10013000)
#define UART0_TXCTRL  (*(volatile unsigned int*)0x10013008)

void uart_init()  { UART0_TXCTRL = 1; }

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

/* ── Wasm3 includes ─────────────────────────────────────────── */
#include "wasm/wasm3/source/wasm3.h"
#include "wasm/wasm3/source/m3_env.h"

/* ── Embedded WASM module ────────────────────────────────────── */
#include "wasm/heartbeat_wasm.c"

/* ── WASM stack size ─────────────────────────────────────────── */
#define WASM_STACK_SLOTS  1024

/* ── Host function: uart_write exposed to WASM module ────────── */
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
#define WASM_UPDATE_DATA  ((const uint8_t*)0x80002008)
#define WASM_UPDATE_MAGIC 0xDEADBEEF

void main() {
    uart_init();
    uart_print("BOOT: Wasm3 heartbeat starting\r\n");

    /* Select WASM source: update region or embedded */
    const uint8_t *wasm_data = heartbeat_wasm;
    uint32_t       wasm_len  = heartbeat_wasm_len;

    if (WASM_UPDATE_FLAG == WASM_UPDATE_MAGIC) {
        uart_print("UPDATE: new WASM module detected\r\n");
        wasm_data = WASM_UPDATE_DATA;
        wasm_len  = WASM_UPDATE_LEN;
        WASM_UPDATE_FLAG = 0;
    }

    uart_print("WASM len: ");
    uart_print_hex(wasm_len);
    uart_print("\r\n");

    uart_print("WASM magic: ");
    uart_print_hex(((uint32_t)wasm_data[0] << 24) |
                   ((uint32_t)wasm_data[1] << 16) |
                   ((uint32_t)wasm_data[2] << 8)  |
                    (uint32_t)wasm_data[3]);
    uart_print("\r\n");

    /* ── Initialise Wasm3 ──────────────────────────────────── */
    uart_print("Creating environment...\r\n");
    IM3Environment env = m3_NewEnvironment();
    if (!env) { uart_print("ERR: env\r\n"); while(1); }

    uart_print("Creating runtime...\r\n");
    IM3Runtime runtime = m3_NewRuntime(env, WASM_STACK_SLOTS, NULL);
    if (!runtime) { uart_print("ERR: runtime\r\n"); while(1); }

    uart_print("Parsing module...\r\n");
    /* Print first 10 bytes of WASM */
    uart_print("WASM bytes: ");
    for(int i=0;i<10;i++){
        const char *hex="0123456789ABCDEF";
        uart_putc(hex[(wasm_data[i]>>4)&0xF]);
        uart_putc(hex[wasm_data[i]&0xF]);
        uart_putc(' ');
    }
    uart_print("\r\n");

    IM3Module module;
    M3Result result = m3_ParseModule(env, &module, wasm_data, wasm_len);
    if (result) {
        uart_print("ERR: ParseModule: ");
        uart_print(result);
        uart_print("\r\n");
        while(1);
    }

    uart_print("Loading module...\r\n");
    result = m3_LoadModule(runtime, module);
    if (result) {
        uart_print("ERR: LoadModule: ");
        uart_print(result);
        uart_print("\r\n");
        while(1);
    }

    uart_print("Linking uart_write...\r\n");
    m3_LinkRawFunction(module, "env", "uart_write", "v(*)", &m3_uart_write);

    uart_print("Finding heartbeat fn...\r\n");
    IM3Function f_heartbeat;
    result = m3_FindFunction(&f_heartbeat, runtime, "heartbeat");
    if (result) {
        uart_print("ERR: FindFunction: ");
        uart_print(result);
        uart_print("\r\n");
        while(1);
    }

    uart_print("WASM: runtime ready!\r\n");

    while (1) {
        result = m3_CallV(f_heartbeat);
        if (result) {
            uart_print("ERR: call: ");
            uart_print(result);
            uart_print("\r\n");
        }
        delay();
    }
}
