#!/bin/bash

idf.py fullclean
idf.py build
idf.py -p /dev/ttyUSB1 flash monitor

