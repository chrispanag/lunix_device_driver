################################################
# 
# Makefile
# for Lunix:TNG 
#
# Vangelis Koukis <vkoukis@cslab.ece.ntua.gr>
#
################################################

# C compiler to use for building userspace applications
CC = gcc

# Remove comment to enable verbose output from the kernel build system
KERNEL_VERBOSE = 'V=1'
DEBUG = y

# Add your debugging flag (or not) to CFLAGS
# Warnings are errors.
ifeq ($(DEBUG),y)
  EXTRA_CFLAGS += -g -DLUNIX_DEBUG=1
  # EXTRA_CFLAGS += -Werror
else
  EXTRA_CFLAGS += -DLUNIX_DEBUG=0
  # EXTRA_CFLAGS += -Werror
endif

#
# Ask the kernel build module to build Lunix:TNG as a module,
# satisfying the dependencies specified in lunix-objs.
#
obj-m	:= lunix.o
lunix-objs := lunix-module.o lunix-chrdev.o lunix-ldisc.o lunix-protocol.o lunix-sensors.o

# If KERNELDIR is not already set, set it to the build tree of the current kernel
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
# Uncomment the following, or set KERNEL_MAKE_ARGS in the environment if building for UML
# KERNEL_MAKE_ARGS ?= ARCH=um SUBARCH=i386
# Extra CFLAGS used to compile the userspace helpers
# e.g., -m32 if compiling in a 64-bit environment.
USER_CFLAGS = -Wall -Werror #-m32

PWD       := $(shell pwd)

all:	modules lunix-attach

modules: lunix-lookup.h
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(KERNEL_VERBOSE) $(KERNEL_MAKE_ARGS) modules

clean: 
	$(MAKE) -C $(KERNELDIR) M=$(PWD) $(KERNEL_VERBOSE) $(KERNEL_MAKE_ARGS) clean
	rm -f modules.order
	rm -f lunix-attach
	rm -f mk_lookup_tables
	rm -f lunix-lookup.h

lunix-attach: lunix.h lunix-attach.c
	$(CC) $(USER_CFLAGS) -o $@ lunix-attach.c

#
# Automagically generated lookup tables
# 
lunix-lookup.h: mk_lookup_tables
	./mk_lookup_tables >lunix-lookup.h

mk_lookup_tables: mk_lookup_tables.c
	$(CC) $(USER_CFLAGS) -o mk_lookup_tables mk_lookup_tables.c -lm

