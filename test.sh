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

assert 'return 0;' 0 
assert 'return 42;' 42
assert 'return 1+2+3;' 6
assert 'return 1*2+3;' 5
assert 'return 10/2-3;' 2
assert 'return 10-2*3+1;' 5
assert 'return  12 + 34 - 5 ;' 41
assert 'return 5+6*7;' 47
assert 'return 5*(9-6);' 15
assert 'return (3+5)/2;' 4
assert 'return 4+((1+2)*3);' 13
assert 'return +1;' 1
assert 'return -3+10;' 7
assert 'return -10+20;' 10
assert 'return - -10;' 10
assert 'return - - +10;' 10
assert 'return 1<2+3;' 1
assert 'return 1>(2*4)+3;' 0
assert 'return 1*3<1;' 0
assert 'return 1<=1;' 1
assert 'return 0>=3;' 0
assert 'return 1==1;' 1
assert 'return 1!=2;' 1
assert 'return 3>2==3<5;' 1
assert 'return !1;' 0
assert 'return !0;' 1
assert 'return !(2==2);' 0
assert 'a=1;b=2;return a+b;' 3
assert 'a = 3;
b = 5 * 6 - 8;
return a + b / 2;' 14
assert 'var=123; return var;' 123
assert 'var=123; var_2=23; var_3=var-var_2; return var_3;' 100
assert '1;2;3; return 4; 5;6;7;' 4
