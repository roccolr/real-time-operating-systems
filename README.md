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

## Dependencies
Extract the archive on Desktop. Import the configuration of the running system as a basic working configuration. 

``` bash 
sudo apt install flex 
sudo apt install bison
cd ~/linux-6.12.79
cp /boot/config-$(uname -r) .config
make olddefconfig

sudo apt install gcc g++ make 
sudo apt install libncurses5-dev
sudo apt install -y qtcreator qtbase5-dev qt5-qmake cmake
sudo apt install build-essential bc bison flex libssl-dev libelf-dev dwarves zstd pkg-config gawk
```

## Kernel patch ~ PREEMPT-RT

You can download the patch matching exactly your kernel version [here](https://wiki.linuxfoundation.org/realtime/start) and [here](https://wiki.linuxfoundation.org/realtime/preempt_rt_versions).

Then run 
``` bash 
make menuconfig
```

Since we are using kernel version 6.*, we can use built in real time support.
Using the GUI, General Setup/Preemption model/fully preemptible. 

Otherwise, we could have done:

``` bash 
patch -p1 --dry-run < ../nome_patch.patch
patch -p1 < ../nome_patch.patch
```

## Configuration 
The aim is to remove _latency killers_ and every bit of useless code. 
In order to reduce bloat from a general purpose distro, run localmodconfig to put in .config only the modules found with lsmod. 

![alt text](/pics/Screenshot%20from%202026-06-01%2018-08-47.png "Configuration")
![alt text](/pics/Screenshot%20from%202026-06-01%2018-55-11.png "Configuration")

## Build the kernel 
To build  the kernell run 

``` bash 
make -j9
```
To install the kernel

``` bash 
sudo make install 
sudo make modules_install
```

If there are too many modules

``` bash 
cd /lib/modules/6.12.79-rt17
sudo find ./ -iname "*.ko" -exec strip --strip-unneeded {} \;
```

## GRUB setup 
``` bash 
sudo update-grub
sudo nano /etc/default/grub
```

comment out # GRUB_TIMEOUT. 
REBOOT and choose your new kernel!

# Tools for Real time Linux
## Measurement 
1. [cyclictest](https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/cyclictest/start) accurately and repeatedly measures the difference between a thread's intended wake-up time and the time at which it actually wakes up from clock_nanosleep in order to provide statistics about the system's latencies. It can measure latencies in real-time systems caused by the hardware, the firmware, and the operating system. 

2. [RT-Tests](https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/rt-tests) is a test suite, that contains programs to test various real time Linux features.

3. [RTLA](https://docs.kernel.org/tools/rtla/index.html) provides a set of tools for the analysis of the kernel’s realtime behavior on specific hardware.

4. [LTP](https://wiki.linuxfoundation.org/realtime/documentation/howto/tools/ltp)'s goal is to deliver tests to the open source community that validate the reliability, robustness, and stability of Linux. 

5. [RT-APP](https://github.com/scheduler-tools/rt-app) is a test application that starts multiple periodic threads in order to simulate a real-time periodic load.

## Load 
1. iperf (network)
2. fio (disk)
3. hackbench (scheduler)
4. stress-ng
