#!/bin/bash

testNo=0

assert() {
    expected="$1"
    input="$2"

    ./kcc "$input" > tmp.s
    cc tmp.s -o tmp
    ./tmp
    actual="$?"
    testNo=$(($testNo+1))
    if [ "$actual" = "$expected" ]; then
        echo "$testNo: ($input) => $actual"
    else
        echo "$testNo: ($input) => $expected expected, but got $actual"
        exit 1
    fi
}

assert 0 "main(){return 0;}"
assert 42 "main(){42;}"
assert 21 "main(){5+20-4;}"
assert 41 "main(){ 12 + 34 - 5 ;}"
assert 47 "main(){5 + 6 * 7;}"
assert 15 "main(){5 * (10 - 7);}"
assert 3 "main(){ (3 + 6) / 3;}"
assert 10 "main(){-10+20;}"

assert 0 "main(){return 0==1;}"
assert 1 "main(){return 23 == 23;}"
assert 1 "main(){return 2 != 4;}"
assert 0 "main(){return 2 != 2;}"
assert 1 "main(){return 0 < 1;}"
assert 1 "main(){return 0 <= 1;}"
assert 1 "main(){return 10 <= 10;}"
assert 1 "main(){return -20 < 0;}"
assert 1 "main(){return 1 < (3 + 2);}"
assert 0 "main(){return 0 < -1;}"
assert 0 "main(){return 2 <= 1;}"
assert 1 "main(){return 2 > 1;}"
assert 1 "main(){return -1 > -4;}"
assert 1 "main(){return 64 >= 64;}"
assert 0 "main(){return 10 > 11;}"
assert 0 "main(){return 10 >= 11;}"

assert 12 "main(){a=12;return a;}"
assert 12 "main(){b=4;b=b+8;return b;}"
assert 1 "main(){3;2;return 1;}"

assert 5 "main(){foo = 5; return foo;}"
assert 12 "main(){hoge = 2; fuga = 6; return hoge * fuga;}"
assert 1 "main(){return 1;}"
assert 8 "main(){a = 10; b = 2; return a + b - b * 2;}"
assert 1 "main(){x = 0; if(x == 0) return 1;}"
assert 2 "main(){x = 0; if(x != 0) return 1; else return 2;}"
assert 1 "main(){hoge = 12; hoge = hoge - 2; if(hoge>5) return 1; return 123;}"

assert 11 "main(){i=0;while(i<=10)i=i+1;return i;}"
assert 55 "main(){s=0;for(i=0;i<=10;i=i+1) s=s+i; return s;}"
assert 3 "main(){return 3;}"

assert 0 "fn(){return 123;} main(){return fn()-123;}"
assert 55 "main(){s=0;i=0;while(i<=10){s=s+i;i=i+1;}return s;}"
assert 0 "fn(x){return x;} main(){return fn(0);}"
assert 3 "add(x,y){return x+y;} main(){return add(1,2);}"
assert 1 "fib(i){if(i==0)return 0;if(i==1)return 1;return fib(i-1)+fib(i-2);} main(){return fib(1);}"

echo -e "\033[32m OK \033[m"
