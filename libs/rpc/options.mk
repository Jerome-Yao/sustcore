component-kind := static-library
component-name := rpc
component-target := $(path-bin)/libs/$(architecture)/librpc.a
component-objdir := $(path-objects)/rpc/$(architecture)

flags-c := $(flags-common-c) -nostdinc $(flags-mode-c)
include-c := -I$(path-include) -I$(path-include)/std -I$(path-include)/libs \
	-I$(path-third_party)/include -I$(path-third_party)/include/std
defs-c := $(defs-mode-c)

flags-cpp := $(flags-common-cpp) -nostdinc $(flags-no-rtti-cpp) -DUSE_SUSTCORE_FEATURES $(flags-no-exceptions-cpp) \
	$(flags-mode-cpp)
include-cpp := -I$(path-include) -I$(path-include)/std -I$(path-include)/std/c++ \
	-I$(path-third_party)/include -I$(path-third_party)/include/libfdt \
	-I$(path-third_party)/include/std -I$(component-root) -I$(path-include)/arch
defs-cpp := $(defs-mode-cpp)