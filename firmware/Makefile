CC      = riscv64-unknown-elf-gcc
OBJCOPY = riscv64-unknown-elf-objcopy

PICOLIBC     = /usr/lib/picolibc/riscv64-unknown-elf
PICOLIBC_INC = $(PICOLIBC)/include
PICOLIBC_LIB = $(PICOLIBC)/lib/rv32imac/ilp32

CFLAGS  = -march=rv32imac -mabi=ilp32 -mcmodel=medany \
          -O2 -nostdlib -fno-builtin \
          -I$(PICOLIBC_INC) \
          -Iwasm/wasm3/source \
          -Iwasm \
          \
          -Dd_m3MaxFunctionStackHeight=512 -Dd_m3FixedHeap=131072

SRCS = main.c stubs.c \
       wasm/wasm3/source/m3_env.c \
       wasm/wasm3/source/m3_parse.c \
       wasm/wasm3/source/m3_compile.c \
       wasm/wasm3/source/m3_exec.c \
       wasm/wasm3/source/m3_function.c \
       wasm/wasm3/source/m3_module.c \
       wasm/wasm3/source/m3_bind.c \
       wasm/wasm3/source/m3_code.c \
       wasm/wasm3/source/m3_core.c \
       wasm/wasm3/source/m3_info.c

all: heartbeat.elf

heartbeat.elf: $(SRCS) crt0.S linker.ld
	$(CC) $(CFLAGS) -T linker.ld -o $@ crt0.S $(SRCS) \
	    -L$(PICOLIBC_LIB) \
	    -Wl,--start-group -lc -lgcc -Wl,--end-group \
	    -Wl,--undefined=_exit

clean:
	rm -f heartbeat.elf
