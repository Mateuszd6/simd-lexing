#!/bin/sh

SNAPSHOT=0
COV_IDX=0
if [ "$1" == "--snapshot" ]; then
    SNAPSHOT=1
fi

rm -rf ./cov/
mkdir -p ./cov/

for f in ./tests/*.c; do
    ./main $f > "mine.out" 2> "mine.err"
    ERROR_CODE=$?

    KOVNAME="./cov/cov_${COV_IDX}"
    let "COV_IDX += 1";
    kcov $KOVNAME ./main $f &> /dev/null

    NAME=$(basename -- "$f")
    echo -ne "${NAME}: "

    if [ -s "mine.err" ] && [ $ERROR_CODE -eq 0 ]; then
        echo "FAILED"
        echo "${f}:1:1: error: Lexing has error, but finished with code 0"
        continue
    elif ! [ -s "mine.err" ] && [ $ERROR_CODE -ne 0 ] ; then
        echo "FAILED"
        echo "${f}:1:1: erorr: Lexing didn't finish with error code 0"
        continue
    fi

    if [ $SNAPSHOT -eq 1 ]; then
        echo "SNAPSHOTTED"

        mkdir -p "./tests/"
        mv ./mine.out "${f}.out"
        mv ./mine.err "${f}.err"
    else
        if ! diff -q ./mine.out "${f}.out" &> /dev/null || ! diff -q ./mine.err "${f}.err" &> /dev/null; then
            echo "FAILED"
            diff --color ./mine.out "${f}.out"
            diff --color ./mine.err "${f}.err"
        else
            echo "OK"
        fi
    fi
done

kcov --merge ./cov ./cov/cov_*
