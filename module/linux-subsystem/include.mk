sources += main.cpp basic.cpp clone.cpp thread.cpp fdtable.cpp \
	file.cpp pipe.cpp
riscv64-sources += arch/riscv64/clone_return.S arch/riscv64/signal_return.S
loongarch64-sources += arch/loongarch64/clone_return.S arch/loongarch64/signal_return.S
