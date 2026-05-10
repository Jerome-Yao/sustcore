include $(global-env)
include $(path-script)/env/local.mk
-include $(path-e)/config.mk

include $(path-e)/flags.mk
include $(path-script)/helper.mk
include $(path-script)/tool.mk
include $(path-script)/build/libc.mk

component-root ?= $(path-d)
component-name ?= $(notdir $(component-root))
component-kind ?=
component-variants ?= default

sources :=
objects :=
deps :=
attachments :=

include $(component-root)/options.mk

component-include-mks := $(shell find $(component-root) -name include.mk | sort)

prefix-include-source = $(if $(filter /%,$(1)),$(1),$(if $(include-dir),$(include-dir)/$(1),$(1)))

define include-component-include
include-file := $(1)
include-dir := $(patsubst %/,%,$(patsubst $(component-root)/%,%,$(dir $(1))))
sources-before-include := $$(sources)
sources :=
include $(1)
sources-added := $$(sources)
sources := $$(sources-before-include) $$(foreach source,$$(sources-added),$$(call prefix-include-source,$$(source)))
endef

$(foreach include-mk,$(component-include-mks),$(eval $(call include-component-include,$(include-mk))))

source-objects := $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(patsubst %.S,%.o,$(sources))))
source-deps := $(patsubst %.c,%.d,$(filter %.c,$(sources))) \
	$(patsubst %.cpp,%.d,$(filter %.cpp,$(sources)))

objects += $(source-objects)
deps += $(source-deps)

include $(path-script)/helper.mk

component-args-c = defs-c="$(defs-c)" include-c="$(include-c)" flags-c="$(flags-c) $(variant.$(1).flags-c)"
component-args-cpp = defs-cpp="$(defs-cpp)" include-cpp="$(include-cpp)" flags-cpp="$(flags-cpp) $(variant.$(1).flags-cpp)"
component-args-asm = defs-asm="$(defs-asm)" include-asm="$(include-asm)" flags-asm="$(flags-asm) $(variant.$(1).flags-asm)"
component-args-ld = flags-ld="$(flags-ld) $(variant.$(1).flags-ld)" script-ld="$(variant.$(1).script-ld)"
component-args = $(call component-args-ld,$(1)) $(call component-args-c,$(1)) $(call component-args-cpp,$(1)) $(call component-args-asm,$(1)) libraries="$(variant.$(1).libraries)"

variant.default.target ?= $(component-target)
variant.default.dir-obj ?= $(component-objdir)
variant.default.flags-c ?=
variant.default.flags-cpp ?=
variant.default.flags-asm ?=
variant.default.flags-ld ?=
variant.default.script-ld ?= $(script-ld)
variant.default.libraries ?= $(libraries)

ifeq ($(component-kind), static-library)

ifeq ($(library-is-libc), true)
component-libc-args := obj-crt0="$(libc-crt0)" obj-crti="$(libc-crti)" obj-crtn="$(libc-crtn)" libc-mode=true
else
component-libc-args :=
endif

define component_static_variant
.PHONY: build-$(component-name)-$(1)
build-$(component-name)-$(1):
	$$(q)$$(MAKE) $$(builder-s)="$$(variant.$(1).target)" architecture="$$(architecture)" deps="$$(deps)" objects="$$(objects)" $$(component-libc-args) dir-obj="$$(variant.$(1).dir-obj)" dir-src="$$(component-root)" $$(call component-args,$(1)) "$$(variant.$(1).target)"
endef

$(foreach variant,$(component-variants),$(eval $(call component_static_variant,$(variant))))

.PHONY: build
build: $(foreach variant,$(component-variants),build-$(component-name)-$(variant))

endif

ifeq ($(component-kind), module)

module-libc ?= $(default-libc)
module-linker-script-path := $(if $(module-linker-script),$(component-root)/$(module-linker-script),$(libc.$(module-libc).module-linker-script))
module-crt-head-objs := $(libc.$(module-libc).crt-head)
module-crt-tail-objs := $(libc.$(module-libc).crt-tail)

variant.default.target := $(path-bin)/mods/$(architecture)/$(module-output)
variant.default.dir-obj := $(path-objects)/module/$(component-name)
variant.default.script-ld := $(module-linker-script-path)
variant.default.libraries := $(module-libraries)

.PHONY: build
build:
	$(q)$(MAKE) $(builder-m)="$(variant.default.target)" architecture="$(architecture)" attachments="$(attachments)" deps="$(deps)" objects="$(objects)" module-crt-head-objs="$(module-crt-head-objs)" module-crt-tail-objs="$(module-crt-tail-objs)" dir-obj="$(variant.default.dir-obj)" dir-src="$(component-root)" $(call component-args,default) "$(variant.default.target)"
	$(q)$(copy) $(variant.default.target) $(path-initrd)/$(module-output)

endif

ifeq ($(component-kind), kernel)

define component_kernel_variant
.PHONY: build-$(component-name)-$(1)
build-$(component-name)-$(1):
	$$(q)$$(MAKE) $$(builder-k)="$$(variant.$(1).target)" architecture="$$(architecture)" attachments="$$(attachments)" deps="$$(deps)" objects="$$(objects)" dir-obj="$$(variant.$(1).dir-obj)" dir-src="$$(component-root)" $$(call component-args,$(1)) "$$(variant.$(1).target)"
endef

$(foreach variant,$(component-variants),$(eval $(call component_kernel_variant,$(variant))))

.PHONY: build
build: $(foreach variant,$(component-variants),build-$(component-name)-$(variant))

endif
