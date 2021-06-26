#!/bin/sh

time ./flex/c.default ./test2.c | tail -n2
echo ""
time ./flex/c.fast ./test2.c | tail -n2
echo ""
time ./flex/c.full ./test2.c | tail -n2
