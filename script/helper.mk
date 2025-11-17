builder := -f $(path-script)/build/Makefile.build global-env=$(global-env) target

builder-k := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=k target
builder-m := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=m target
builder-s := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=s target
builder-l := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=l target

builder-k-x86 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=k architecture=x86 target
builder-m-x86 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=m architecture=x86 target
builder-s-x86 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=s architecture=x86 target
builder-l-x86 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=l architecture=x86 target

builder-k-x86_64 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=k architecture=x86_64 target
builder-m-x86_64 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=m architecture=x86_64 target
builder-s-x86_64 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=s architecture=x86_64 target
builder-l-x86_64 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=l architecture=x86_64 target

builder-k-riscv64 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=k architecture=riscv64 target
builder-m-riscv64 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=m architecture=riscv64 target
builder-s-riscv64 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=s architecture=riscv64 target
builder-l-riscv64 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=l architecture=riscv64 target

builder-k-loongarch64 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=k architecture=loongarch64 target
builder-m-loongarch64 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=m architecture=loongarch64 target
builder-s-loongarch64 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=s architecture=loongarch64 target
builder-l-loongarch64 := -f $(path-script)/build/Makefile.build global-env=$(global-env) mode=l architecture=loongarch64 target

imager := -f $(path-script)/image/Makefile.image global-env=$(global-env) target

doc-generator := -f $(path-script)/gendoc/Makefile.doc global-env=$(global-env) config