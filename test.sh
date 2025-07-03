#!/bin/bash

assert() {
    input="$1"
    expected="$2"

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
assert '1+2+3' 6
assert '1*2+3' 5
assert '10/2-3' 2
assert '10-2*3+1' 5
assert ' 12 + 34 - 5 ' 41
assert '5+6*7' 47
assert '5*(9-6)' 15
assert '(3+5)/2' 4
assert '4+((1+2)*3)' 13
assert '+1' 1
assert '-3+10' 7
assert '-10+20' 10
assert '- -10' 10
assert '- - +10' 10
assert '1<2+3' 1
assert '1>(2*4)+3' 0
assert '1*3<1' 0
assert '1<=1' 1
assert '0>=3' 0
assert '1==1' 1
assert '1!=2' 1
assert '3>2==3<5' 1
assert '!1' 0
assert '!0' 1
assert '!(2==2)' 0
