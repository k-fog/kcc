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

assert 'int main(){return 0;}' 0 
assert 'int main(){return 42;}' 42
assert 'int main(){return 1+2+3;}' 6
assert 'int main(){return 1*2+3;}' 5
assert 'int main(){return 10/2-3;}' 2
assert 'int main(){return 10-2*3+1;}' 5
assert 'int main(){return  12 + 34 - 5 ;}' 41
assert 'int main(){return 5+6*7;}' 47
assert 'int main(){return 5*(9-6);}' 15
assert 'int main(){return (3+5)/2;}' 4
assert 'int main(){return 4+((1+2)*3);}' 13
assert 'int main(){return +1;}' 1
assert 'int main(){return -3+10;}' 7
assert 'int main(){return -10+20;}' 10
assert 'int main(){return - -10;}' 10
assert 'int main(){return - - +10;}' 10
assert 'int main(){return 1<2+3;}' 1
assert 'int main(){return 1>(2*4)+3;}' 0
assert 'int main(){return 1*3<1;}' 0
assert 'int main(){return 1<=1;}' 1
assert 'int main(){return 0>=3;}' 0
assert 'int main(){return 1==1;}' 1
assert 'int main(){return 1!=2;}' 1
assert 'int main(){return 3>2==3<5;}' 1
assert 'int main(){return !1;}' 0
assert 'int main(){return !0;}' 1
assert 'int main(){return !(2==2);}' 0
assert 'int main(){int a;int b;a=1;b=2;return a+b;}' 3
assert 'int main(){int a; int b; a = 3;
b = 5 * 6 - 8;
return a + b / 2;}' 14
assert 'int main(){int var; var=123; return var;}' 123
assert 'int main(){int var; int var_2; int var_3; var=123; var_2=23; var_3=var-var_2; return var_3;}' 100
assert 'int main(){1;2;3; return 4; 5;6;7;}' 4
assert 'int main(){return ident(123);}' 123
assert 'int main(){return add2(1, 2);}' 3
assert 'int main(){return add3(1,2,4-3);}' 4
assert 'int main(){return add4(1+2,2,3,4);}' 12
assert 'int main(){return add5(1,2,3,4,5);}' 15
assert 'int main(){return add6(1,2,3,4,5,6);}' 21
assert 'int main(){int a;a=0; if(a==0) return 3; return 1;}' 3
assert 'int main(){int a;a=0; if(a==2) return 3; else return 1;}' 1
assert 'int main(){int a;a=0; if(a==0) a = a + 2; else a = a + 3; return a;}' 2
assert 'int main(){int a;a=0; if(a==2) a = a + 2; else a = a + 3; return a;}' 3
assert 'int main(){int a;a=1; if(a==0) return 0; else if(a==1) return 1; else return 2;}' 1
assert 'int main(){int a;a=2; if(a==0) return 0; else if(a==1) return 1; else return 2;}' 2
assert 'int main(){int a;int b;a=2; b=0; if(a==2) if(b==1) return 1; else return 2; else return 3;}' 2
assert 'int main(){{ 1+2; 2+3; return 4; }}' 4
assert 'int main(){int a;a=2; if(a==2) { a = a + 2; return a;} else { return 1; }}' 4
assert 'int main(){int a;a=0;while(a<=10)a=a+1;return a;}' 11
assert 'int main(){int a;int i;a=0;for(i=0;i<=10;i=i+1)a=a+i;return a;}' 55
assert 'int main(){int a;int b;int i;a=0;for(i=0;i<=10;i=i+1)a=a+i;b=0;for(i=0;i<=2;i=i+1)b=i;return a+b;}' 57
assert 'int div(int x, int y){return x/y;}int main(){div(10,5);}' 2
assert 'int fib(int n) {
            if(n<=1) return n;
            else return fib(n-1) + fib(n-2);
        }
        int main(){return fib(10);}' 55
assert 'int sub(int a,int b,int c,int d,int e,int f){return a-b-c-d-e-f;}int main(){sub(100,0,1,2,3,4);}' 90
assert 'int main(){int x;x=1;x+=5;return x;}' 6
assert 'int main(){int x;int y;x=1;y=x+=5;return y;}' 6
assert 'int main(){int x;x=5;x-=3;return x;}' 2
assert 'int main(){int x;x=3;x*=5;return x;}' 15
assert 'int main(){int x;x=15;x/=3;return x;}' 5
assert 'int main(){
    // comment
    return 1+2 /* comment */ +3;} //comment
    /* comment */' 6
assert 'int main(){int x;int *y;x=5; y=&x; return *y;}' 5
assert 'int main(){
    int a; a = 3;
    int *b; b = &a;
    int **c; c = &b;
    return **c;}' 3
assert 'int main(){int x;int *y;y=&x;*y=3;return x;}' 3

echo "all tests passed"
