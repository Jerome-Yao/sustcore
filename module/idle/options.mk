component-kind := module
component-name := idle
module-output := idle.mod
module-libc := placeholder-libc
module-libraries :=
module-linker-script := idle.ld

flags-ld := $(flags-module-ld) $(flags-common-ld) $(flags-mode-ld)

include-asm := -I$(path-include) -I$(path-include)/std \
	-I$(path-third_party)/include -I$(path-third_party)/include/std
