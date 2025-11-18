mkdir ?= mkdir
rm ?= rm
dd ?= dd
mkfs ?= mkfs
grub-install ?= grub-install
sudo ?= sudo
mount ?= mount
umount ?= umount
echo ?= echo
cd ?= cd
copy ?= cp
chmod ?= chmod
doxygen ?= doxygen
loop-setup ?= losetup
fdisk ?= fdisk

qemu ?= qemu-system-$(architecture)

comments-stat ?= $(path-tools)/comments_stat/stat.py
get-loop ?= $(path-tools)/get_loop_devices/get_loop.sh

loop-a ?= $(shell $(get-loop) 0)
loop-b ?= $(shell $(get-loop) $(loop-a))

ifeq ($(filesystem), fat32)
mkfs-fs := $(mkfs).msdos -F 32 -n $(os-name) -v -f 2
else
ifeq ($(filesystem), ext4)
mkfs-fs := $(mkfs).ext4 -m 5 -L $(os-name)
endif
endif
mkfs-fs ?= $(mkfs).msdos -F 32 -n $(os-name) -v -f 2

ifeq ($(architecture), x86_64)
grub-install-target := $(grub-install) --target=i386-pc
endif
grub-install-target ?= $(grub-install) --target=i386-pc

grub-modules ?= normal part_msdos ext2 fat multiboot all_video