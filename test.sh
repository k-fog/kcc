#!/bin/bash

TEST_FNCALL="test_fncall"

cat <<EOF | gcc -xc - -c -o $TEST_FNCALL
int ident(int a) { return a; }
int add2(int a, int b) { return a + b; }
int add3(int a, int b, int c) { return a + b + c; }
int add4(int a, int b, int c, int d) { return a + b + c + d; }
int add5(int a, int b, int c, int d, int e) { return a + b + c + d + e; }
int add6(int a, int b, int c, int d, int e, int f) { return a + b + c + d + e + f; }
EOF

assert() {
    input="$1"
    expected="$2"

    ./kcc "$input" > tmp.s
    cc -o tmp tmp.s $TEST_FNCALL
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
assert 'return ident(123);' 123
assert 'return add2(1, 2);' 3
assert 'return add3(1,2,4-3);' 4
assert 'return add4(1+2,2,3,4);' 12
assert 'return add5(1,2,3,4,5);' 15
assert 'return add6(1,2,3,4,5,6);' 21
assert 'a=0; if(a==0) return 3; return 1;' 3
assert 'a=0; if(a==2) return 3; else return 1;' 1
assert 'a=0; if(a==0) a = a + 2; else a = a + 3; return a;' 2
assert 'a=0; if(a==2) a = a + 2; else a = a + 3; return a;' 3
assert '{ 1+2; 2+3; return 4; }' 4
assert 'a=2; if(a==2) { a = a + 2; return a;} else { return 1; }' 4
assert 'a=0;while(a<=10)a=a+1;return a;' 11
assert 'a=0;for(i=0;i<=10;i=i+1)a=a+i;return a;' 55
assert 'a=0;for(i=0;i<=10;i=i+1)a=a+i;b=0;for(i=0;i<=2;i=i+1)b=i;return a+b;' 57

echo "all tests passed"
