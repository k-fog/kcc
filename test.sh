#!/bin/bash

assert() {
    expected="$1"
    input="$2"

    ./kcc "$input" > tmp.s
    cc -o tmp tmp.s
    ./tmp
    actual="$?"

    if [ "$actual" = "$expected" ]; then
        echo "$input => $actual"
    else
        echo "$input => $expected expected, but got $actual"
        exit 1
    fi
}

assert 0 0 
assert 42 42
assert 6 '1+2+3'
assert 5 '1*2+3'
assert 2 '10/2-3'
assert 5 '10-2*3+1'
