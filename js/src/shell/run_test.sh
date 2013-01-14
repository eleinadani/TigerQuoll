#!/bin/bash

TQ=../_build/js/shell/js
THREADS=1

FAILED=0

for file in "$@"; do
    echo "Testing $file...";
    "$TQ" -T "$THREADS" "$file" > $file.out 2> $file.err
    if ! diff "$file.out" "$file.exp_out" > /dev/null; then
        echo "Error: stdout of test $file differs"
        FAILED=1
    else
        rm "$file.out"
    fi
    if ! diff "$file.err" "$file.exp_err" > /dev/null; then
        echo "Error: stderr of test $file differs"
        FAILED=1
    else
        rm "$file.err"
    fi
done

if [ $FAILED != "0" ]; then
    echo "Tests failed."
else
    echo "Tests passed."
fi
exit $FAILED
