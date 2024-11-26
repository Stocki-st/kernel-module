# is18 kernel module

[TOC]

## how to build
you can build the following targets:

- default: kernel module itselt
```
make 
```
- testapp: test application for the kernel module
```
make testapp
```
- all: mean both: testapp and kernel module
```
make all
```
clean with
```
make clean
```

build everything
```
make all
```

## how to (un)install:
uninstalling the kernel module:
```
make remove
```

installing the kernel module:
```
make install
```

## how to run the test:
```
./testapp <device-file> <mode>
```
e.g.:
```
./testapp /dev/is18dev1 ioctl
```
the following modes/testcases are supported:
 - 'ioctl': - is testing all the ioctl functionality
 - 'rw_blocking': - tests reading and writing in blocking mode (multi threaded)
 - 'rw_nonblocking': - tests reading and writing in non-blocking mode
 - 'all': - executes all the above mentioned tests

It's also supported to start the test with multiple testmodes, e.g.: 
```
./testapp /dev/is18dev1 ioctl rw_blocking
```


## proc file

a file containing process information can be found here:

```
cat /proc/is18/info
```
