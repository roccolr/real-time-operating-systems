# real time operating systems
this is a wrapper for most interesting and useful shell scripts and  c code for real time operating systems programming.

## Linux Kernel download
You can download a linux kernel [here](https://cdn.kernel.org/pub/linux/kernel/). 

You can check your running kernel version using: 

``` bash 
uname -r
```

You can check your distro version using: 

``` bash 
lsb_release -a 
```

Extract the archive on Desktop. Import the configuration of the running system as a basic working configuration. 


``` bash 
cd ~/linux-6.12.79
cp /boot/config-$(uname -r) .config
make olddefconfig
```

