#!/bin/bash

TEST_FNCALL="test_fncall"

cat <<EOF | gcc -xc - -c -o $TEST_FNCALL
#include <stdlib.h>
int ident(int a) { return a; }
int add2(int a, int b) { return a + b; }
int add3(int a, int b, int c) { return a + b + c; }
int add4(int a, int b, int c, int d) { return a + b + c + d; }
int add5(int a, int b, int c, int d, int e) { return a + b + c + d + e; }
int add6(int a, int b, int c, int d, int e, int f) { return a + b + c + d + e + f; }
void alloc4(int **p, int a, int b, int c, int d) {
    *p = malloc(4*sizeof(int));
    (*p)[0]=a;
    (*p)[1]=b;
    (*p)[2]=c;
    (*p)[3]=d;
}
EOF

assert() {
    input="$1"
    expected="$2"

    echo "$input" | ./kcc - > tmp.s
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
assert 'int div(int x, int y){return x/y;}int main(){return div(10,5);}' 2
assert 'int fib(int n) {
            if(n<=1) return n;
            else return fib(n-1) + fib(n-2);
        }
        int main(){return fib(10);}' 55
assert 'int sub(int a,int b,int c,int d,int e,int f){return a-b-c-d-e-f;}int main(){return sub(100,0,1,2,3,4);}' 90
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
assert 'int main(){int x;x=3;return *&x;}' 3
assert 'int main(){
    int a; a = 3;
    int *b; b = &a;
    int **c; c = &b;
    return **c;}' 3
assert 'int main(){int x;int *y;y=&x;*y=3;return x;}' 3
assert 'int main(){int *p; alloc4(&p, 1,2,4,8); int *q; q=p+2; return *q;}' 4
assert 'int main(){int *p; alloc4(&p, 1,2,4,8); int *q; q=3+p; return *q;}' 8
assert 'int main(){int *p; alloc4(&p, 1,2,4,8); int *q; q=4+p-1; return *q;}' 8
assert 'int main(){int *p; alloc4(&p, 1,2,4,8); int **pp; pp = &p; return *(*pp + 3);}' 8
assert 'int main(){int *p; alloc4(&p, 1,2,4,8); return ident(*(p+1));}' 2
assert 'int main(){int p; return sizeof(p+3);}' 4
assert 'int main(){int *p; return sizeof(p+5);}' 8
assert 'int main(){int **p; return sizeof(p);}' 8
assert 'int main(){int a[10]; *(a+3) = 1; return *(a+3);}' 1
assert 'int main(){int a[10]; a[5] = 1; return a[5];}' 1
assert 'int main(){int a[2]; a[0] = 1; a[a[0]] = 2; return a[1];}' 2
assert 'int main(){int a[2]; *(a+1)=4; return a[1];}' 4
assert 'int main(){int a; a=1; return a+=1;}' 2
assert 'int main(){int a; a=1; return a-=1;}' 0
assert 'int main(){int a; a=1; return a*=2;}' 2
assert 'int main(){int a; a=6; return a/=2;}' 3
assert 'int main(){int a; int b; a=b=3; return a+b;}' 6
assert 'int inc(int *x) {return *x=*x+1;} int main() {int p; p=0; inc(&p); return p;}' 1
assert 'int a;int main() {return a;}' 0
assert 'int a;int main() {a=0;return a;}' 0
assert 'int a;int add() {return a+=1;}int main() {add(); add(); return a;}' 2
assert 'int *a;int main() {alloc4(&a,0,1,2,3);return a[0];}' 0
assert 'int a[4];int main() {a[0]=1;return a[0];}' 1
assert 'int main() {int foo[8]; return &foo[7] - &foo[2];}' 5
# assert 'int main() {char foo[8]; return &foo[7] - &foo[2];}' 5
assert 'int main() {char x[3];x[0] = -1;x[1] = 2;int y;y = 4;return x[0] + y;}' 3
assert 'int c2i(char c){return c;}int main() {int a; a = c2i(256)/16; return a;}' 0
assert 'int main() { char *x; x = "abc"; return x[0]; }' 97
assert 'int main() { char *x; char *y; x = "abc"; y = "def"; return y[2]; }' 102
assert 'int main() { return "abc"[1]; }' 98
assert 'char chr() { return 0; } int main() { return sizeof(chr()); }' 1
# assert 'int *ptr() {int *p; alloc4(&p,1,2,4,8); return p;} int main() {return ptr()[3];}' 8
assert 'int *ptr() {int *p; alloc4(&p,1,2,4,8); return p;} int main() {return (ptr())[3];}' 8
assert 'int main() { return sizeof(char); }' 1
assert 'int main() { return sizeof(int); }' 4
assert 'int main() { return sizeof("123456789")/sizeof(char); }' 10
assert 'int main() { return sizeof(int*); }' 8
assert 'int main() { return sizeof(char***); }' 8
assert 'int main() {int a=1,b;b=2;return a+b;}' 3
assert 'int main() {int a=1,b=2; return a+b;}' 3
assert 'int main() {int a=1,b=2;int *x=&a,y=3; return b+*x+y;}' 6
assert 'int main() {int a[3][4]; return sizeof(a);}' 48
assert 'int main() {int a[3][4]; return sizeof(a[1]);}' 16
assert 'int main() {int *hoge[10]; return sizeof(hoge);}' 80
assert 'int main() {char a[1][2][3][4][5]; return sizeof(a);}' 120
assert 'int main() {int (*hoge)[10]; return sizeof(hoge);}' 8
assert 'int main() {int (*hoge)[10]; return sizeof(hoge);}' 8
assert 'int main() {int ((a))=3; return sizeof(a);}' 4
assert 'int main() {int ((a))=3; return a;}' 3
assert 'int main() {int *(hoge); return sizeof(hoge);}' 8
assert 'int main() {int a=0; ++a; return a;}' 1
assert 'int main() {int a=0; ++a; return ++a;}' 2
assert 'int main() {int a=10; --a; return a;}' 9
assert 'int main() {int a=10; --a; return --a;}' 8
assert 'int main() {int a[3]; a[1]=2; int *p = a; return *(++p);}' 2
assert 'int main() {int a[3]; a[1]=2; int *p = a + 2; return *(--p);}' 2
assert 'int main() {int a=0; a++; return a;}' 1
assert 'int main() {int a=0; a++; return a++;}' 1
assert 'int main() {int a=10; a--; return a;}' 9
assert 'int main() {int a=10; a--; return a--;}' 9
assert 'int main() {int a[3]; a[1]=2; int *p = a; p++; return *p;}' 2
assert 'int main() {int a[3]; a[1]=2; int *p = a + 2; p--; return *p;}' 2

echo "all tests passed"
