#!/bin/bash

# Compile .so
clang -shared -o umcgi/bus.so umcgi/bus.c

# Start PHP server
php -S $1
