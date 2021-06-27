#!/bin/sh

./flex/c.default ./test2.c &> /dev/null
time ./flex/c.default ./test2.c | tail -n2
echo ""
./flex/c.fast ./test2.c &> /dev/null
time ./flex/c.fast ./test2.c | tail -n2
echo ""
./flex/c.full ./test2.c &> /dev/null
time ./flex/c.full ./test2.c | tail -n2
echo ""
./main ./test2.c &> /dev/null
time ./main ./test2.c
