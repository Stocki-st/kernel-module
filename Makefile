DRIVER = is18drv
# If KERNELRELEASE is defined, we've been invoked from the
# kernel build system and can use its language.
ifneq ($(KERNELRELEASE),)
	obj-m := $(DRIVER).o
# Otherwise we were called directly from the command
# line; invoke the kernel build system.
else
	KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif

testapp: testapp.c
	gcc -Wall -o testapp testapp.c -lpthread
install:
	sudo insmod $(DRIVER).ko
	sleep 1
	sudo chmod 666 /dev/is18dev*
	ls -l /dev/is18dev*
	
remove:
	sudo rmmod $(DRIVER)
	
clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
	rm -f testapp
	rm *.orig

all: default testapp
