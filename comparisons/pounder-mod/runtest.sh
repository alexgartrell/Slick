#!/bin/bash

if lsmod | grep -q pounder
then
    sudo rmmod pounder.ko
fi

sudo insmod pounder.ko