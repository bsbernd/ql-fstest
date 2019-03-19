#!/bin/bash

dir=$1

if [ -z "$dir" ]; then
    echo
    echo "Usage: $(basename $0) <path-to-test-for-ql-fstest-errors>"
    echo
    exit 1
fi

is_error=0
for i in $dir/fstest-*.err; do
    if [ -s $i ]; then
        echo "File with error: $i"
        is_error=1
    fi
done

exit ${is_error}
