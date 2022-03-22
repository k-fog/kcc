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

assert 0 "int main(){return 0;}"
assert 42 "int main(){return 42;}"
assert 21 "int main(){return 5+20-4;}"
assert 41 "int main(){return 12 + 34 - 5 ;}"
assert 47 "int main(){return 5 + 6 * 7;}"
assert 15 "int main(){return 5 * (10 - 7);}"
assert 3 "int main(){return (3 + 6) / 3;}"
assert 10 "int main(){return -10+20;}"

assert 0 "int main(){return 0==1;}"
assert 1 "int main(){return 23 == 23;}"
assert 1 "int main(){return 2 != 4;}"
assert 0 "int main(){return 2 != 2;}"
assert 1 "int main(){return 0 < 1;}"
assert 1 "int main(){return 0 <= 1;}"
assert 1 "int main(){return 10 <= 10;}"
assert 1 "int main(){return -20 < 0;}"
assert 1 "int main(){return 1 < (3 + 2);}"
assert 0 "int main(){return 0 < -1;}"
assert 0 "int main(){return 2 <= 1;}"
assert 1 "int main(){return 2 > 1;}"
assert 1 "int main(){return -1 > -4;}"
assert 1 "int main(){return 64 >= 64;}"
assert 0 "int main(){return 10 > 11;}"
assert 0 "int main(){return 10 >= 11;}"

assert 12 "int main(){int a;a=12;return a;}"
assert 12 "int main(){int b;b=4;b=b+8;return b;}"
assert 1 "int main(){3;2;return 1;}"

assert 5 "int main(){int foo;foo = 5; return foo;}"
assert 12 "int main(){int hoge;int fuga;hoge = 2; fuga = 6; return hoge * fuga;}"
assert 1 "int main(){return 1;}"
assert 8 "int main(){int a;int b;a = 10; b = 2; return a + b - b * 2;}"
assert 1 "int main(){int x;x = 0; if(x == 0) return 1;}"
assert 2 "int main(){int x;x = 0; if(x != 0) return 1; else return 2;}"
assert 1 "int main(){int hoge;hoge = 12; hoge = hoge - 2; if(hoge>5) return 1; return 123;}"

assert 11 "int main(){int i;i=0;while(i<=10)i=i+1;return i;}"
assert 55 "int main(){int s;int i;s=0;for(i=0;i<=10;i=i+1) s=s+i; return s;}"
assert 3 "int main(){return 3;}"

assert 0 "int fn(){return 123;} int main(){return fn()-123;}"
assert 55 "int main(){int i;int s;s=0;i=0;while(i<=10){s=s+i;i=i+1;}return s;}"
assert 0 "int fn(int x){return x;} int main(){return fn(0);}"
assert 3 "int add(int x,int y){return x+y;} int main(){return add(1,2);}"
assert 1 "int main(){int n;n=1;if(n==0)return n;return n;}"
assert 1 "int main(){int n;n=1;if(0)return 0;return n;}"
assert 55 "int fib(int n){if(n==0)return 0; if(n==1)return 1; return fib(n-1)+fib(n-2);} int main(){return fib(10);}"

assert 0 "int main(){int s;int p;s=0;p=&s;return *p;}"
assert 3 "int main(){int x;int y;int z;x=3;y=5;z=&y+8;return *z;}" # 今のところOK

echo -e "\033[32m OK \033[m"
