override flags-c += -DBITS=64
override flags-asm +=

prefix-compiler ?= riscv64-unknown-elf-

$(warning "ARCH=riscv64")