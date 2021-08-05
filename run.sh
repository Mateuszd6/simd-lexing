#!/bin/sh

TESTFILE=./test2.c

./flex/c.default $TESTFILE &> /dev/null
time ./flex/c.default $TESTFILE | tail -n2
echo ""
./flex/c.fast $TESTFILE &> /dev/null
time ./flex/c.fast $TESTFILE | tail -n2
echo ""
./flex/c.full $TESTFILE &> /dev/null
time ./flex/c.full $TESTFILE | tail -n2
echo ""
./main $TESTFILE &> /dev/null
time ./main $TESTFILE
