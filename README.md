# WASM-Based Heartbeat Monitoring from Emulated RISC-V MCU in Renode

A simulated Industrial IoT system where a RISC-V MCU (SiFive FE310) emulated in Renode runs the Wasm3 WebAssembly runtime and executes a WASM module that sends periodic heartbeat messages over UART to a host monitoring server.

---

# Key Fixes (Per Feedback)

## Fix 1 - WASM Runtime Now Runs ON the MCU

Previously the heartbeat was generated directly by C code in `main.c`, while the WASM module executed separately on the host using Wasmtime.

Now:

* `main.c` acts as a lightweight Wasm3 host loader
* Wasm3 runtime is cross-compiled for RISC-V
* `heartbeat.wasm` runs INSIDE the emulated MCU in Renode
* The WASM module generates heartbeat messages through imported UART functions

---

## Fix 2 - Dynamic WASM Module Replacement

Previously there was no way to update the WASM module without rebuilding firmware.

Now:

* RAM region at `0x80002000` acts as WASM staging memory
* Firmware checks for update magic value `0xDEADBEEF`
* If present, firmware loads new WASM module from RAM
* Host tools can update WASM dynamically over UART

---

# Project Structure

```text
riscv-heartbeat/
├── firmware/
│   ├── main.c
│   ├── crt0.S
│   ├── linker.ld
│   ├── heartbeat.elf
│   └── wasm/
│       ├── heartbeat.c
│       ├── heartbeat.wasm
│       ├── heartbeat_wasm.c
│       └── wasm3/
│
├── renode/
│   └── fe310_bigram.repl
│
└── README.md
```

---

# Technologies Used

* RISC-V RV32IMAC
* Renode Emulator
* SiFive FE310 MCU
* Wasm3 Runtime
* WebAssembly (WASM)
* Bare-metal C
* UART Communication
* GCC RISC-V Toolchain

---

# Build Instructions

## 1. Compile WASM Module

```bash
clang \
  --target=wasm32 \
  -nostdlib \
  -Wl,--no-entry \
  -Wl,--export-all \
  -o heartbeat.wasm \
  heartbeat.c
```

---

## 2. Convert WASM Binary to C Array

```bash
xxd -i heartbeat.wasm > heartbeat_wasm.c
```

---

## 3. Build Firmware

```bash
riscv64-unknown-elf-gcc \
  -march=rv32imac \
  -mabi=ilp32 \
  -nostdlib \
  -ffreestanding \
  crt0.S \
  main.c \
  wasm/heartbeat_wasm.c \
  wasm/wasm3/source/m3_*.c \
  -Iwasm/wasm3/source \
  -T linker.ld \
  -o heartbeat.elf
```

---

# Running in Renode

Start Renode:

```bash
renode
```

Inside Renode:

```text
mach create
machine LoadPlatformDescription @/home/sarfraz/riscv-heartbeat/renode/fe310_bigram.repl
showAnalyzer sysbus.uart
sysbus LoadELF @/home/sarfraz/riscv-heartbeat/firmware/heartbeat.elf
start
```

---

# Expected Output

```text
heartbeat from WASM
heartbeat from WASM
heartbeat from WASM
```

---

# Educational Goals

This project demonstrates:

* Running WebAssembly on embedded RISC-V systems
* Bare-metal firmware development
* Emulated MCU debugging using Renode
* UART-based communication
* Dynamic firmware extensibility using WASM
* Lightweight sandboxed execution on constrained systems

---

# Future Improvements

* Live WASM hot-swapping over UART
* MQTT heartbeat forwarding
* Memory protection for WASM sandbox
* RTOS integration
* Real hardware deployment on FE310 boards
* Web dashboard visualization

---

# Author

Sarfraz Ahmed

GitHub Repository:

https://github.com/sarfrazahmed22/wasm-riscv-heartbeat
