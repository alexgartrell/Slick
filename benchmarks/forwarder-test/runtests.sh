#!/bin/bash

NTHREADS=20

./init_tests.py 10.212.135.41 10.212.135.43 20

for P in 32 64 128 256 512 1024
do
    ../filesender/filesender ${NTHREADS} slick0 12345 1000 \
        payloads/payload${P}.tft
    echo "Sleeping 10"
    sleep 5
done