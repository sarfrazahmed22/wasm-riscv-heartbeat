#include <stdint.h>

// Declare uart_write as an imported function
__attribute__((import_module("env"), import_name("uart_write")))
extern void uart_write(const char* msg);

static uint32_t uptime = 0;

// Exported WASM function
__attribute__((export_name("heartbeat")))
void heartbeat(void)
{
    uptime++;
    uart_write("heartbeat from WASM\n");
}
