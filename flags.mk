# Common build flags shared by kernel, modules, and libraries.

flags-common-c := -std=gnu23 -nostdlib -fno-builtin -ffreestanding \
	-Wall -Wno-int-conversion -Wstrict-prototypes -Werror=implicit-function-declaration \
	-fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
	-fno-stack-protector -Wno-int-to-pointer-cast \
	-fno-toplevel-reorder -fno-tree-scev-cprop -mcmodel=medany

flags-common-cpp := -std=gnu++26 -nostdlib -fno-builtin -ffreestanding \
	-Wall -Wno-sign-compare -Wno-literal-suffix -Wno-int-to-pointer-cast \
	-fno-strict-aliasing -fomit-frame-pointer -fno-pic -fno-asynchronous-unwind-tables \
	-fno-stack-protector -fno-toplevel-reorder -fno-tree-scev-cprop \
	-mcmodel=medany

flags-no-rtti-cpp := -D__SUS_NO_RTTI__ -fno-rtti
flags-no-exceptions-cpp := -D__SUS_NO_EXCEPTIONS__ -fno-exceptions

flags-common-ld := -nostdlib
flags-kernel-ld := -z max-page-size=0x100
flags-module-ld := -z max-page-size=0x1000 -e_start

flags-mode-c :=
flags-mode-cpp :=
flags-mode-ld :=
defs-mode-c :=
defs-mode-cpp :=

ifeq ($(build-mode), debug)
flags-mode-c += -O0 -g
flags-mode-cpp += -O0 -g
else
flags-mode-c += -Ofast
flags-mode-cpp += -Ofast
flags-mode-ld += --strip-all
defs-mode-c += -DNDEBUG
defs-mode-cpp += -DNDEBUG
endif
