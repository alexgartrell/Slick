#!/bin/bash

for P in 32 64 128 256 512 1024
do
    echo $P
    nc -l 12345 > payloads/payload${P}.dat &
    sleep 2
    python sender.py ${P}
    wait
done