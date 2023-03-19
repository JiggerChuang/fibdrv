#!/bin/bash

sudo rmmod fibdrv
make clean
make
sudo insmod fibdrv.ko
sudo ./client
sudo dmesg --read-clear