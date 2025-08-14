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

    echo "$input" > tmp.c
    ./kcc2 tmp.c > tmp.s
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
assert '
int fib(int n) {
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
assert 'int main() {char foo[8]; return &foo[7] - &foo[2];}' 5
assert 'int main() {struct{int a;int b;int c;} foo[8]; return &foo[7] - &foo[2];}' 5
assert 'int main() {char x[3];x[0] = -1;x[1] = 2;int y;y = 4;return x[0] + y;}' 3
assert 'int c2i(char c){return c;}int main() {int a; a = c2i(256)/16; return a;}' 0
assert 'int main() { char *x; x = "abc"; return x[0]; }' 97
assert 'int main() { char *x; char *y; x = "abc"; y = "def"; return y[2]; }' 102
assert 'int main() { return "abc"[1]; }' 98
assert 'char chr() { return 0; } int main() { return sizeof(chr()); }' 1
assert 'int *ptr() {int *p; alloc4(&p,1,2,4,8); return p;} int main() {return ptr()[3];}' 8
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
assert 'int main() {int ((a))=3; return sizeof(a);}' 4
assert 'int main() {int ((a))=3; return a;}' 3
assert 'int main() {int *(hoge); return sizeof(hoge);}' 8
assert 'int main() {int a=0; ++a; return a;}' 1
assert 'int main() {int a=0; ++a; return ++a;}' 2
assert 'int main() {int a=10; --a; return a;}' 9
assert 'int main() {int a=10; --a; return --a;}' 8
assert 'int main(){int *p; alloc4(&p, 1,2,4,8); p++; p++; return *p++;}' 4
assert 'int main(){int *p; alloc4(&p, 1,2,4,8); ++p; return *++p;}' 4
assert 'int main(){int *p; alloc4(&p, 1,2,4,8); p+=3; p--; return *p--;}' 4
assert 'int main(){int *p; alloc4(&p, 1,2,4,8); p+=3; p--; return *--p;}' 2
assert 'int main() {int a=0; a++; return a;}' 1
assert 'int main() {int a=0; a++; return a++;}' 1
assert 'int main() {int a=10; a--; return a;}' 9
assert 'int main() {int a=10; a--; return a--;}' 9
assert 'int main() {int a[3]; a[1]=2; int *p = a; p++; return *p;}' 2
assert 'int main() {int a[3]; a[1]=2; int *p = a + 2; p--; return *p;}' 2
assert 'int main() { return 10%3; }' 1
assert 'int main() { return 7%10; }' 7
assert 'int main() {int a[3] = {1, 2+3, ident(4),}; return a[2];}' 4
assert 'int main() {char a[4] = {1, 2, 3, 4}; return a[2];}' 3
assert 'int main(){char *s[2]={"abc", "def"}; return s[1][1];}' 101
assert 'int main() { return sizeof(int[4]); }' 16
assert 'int main() { return sizeof(int*[4]); }' 32
assert 'int main() { return sizeof(int(*)[4]); }' 8
assert 'int main() { return sizeof(char(**)[4]); }' 8
assert 'int main() { return sizeof(int[4][5]); }' 80
assert 'int x = 1, y = 2; int main() { return x + y; }' 3
assert 'char x = 9; int main() { return x; }' 9
assert 'int x[3] = {1,2,3}; int main() { return x[2]; }' 3
assert 'int main() { int x = 0; return x == 0 ? 3 : 4; }' 3
assert 'int main() { int x = -1; return (0 < x ? 3 : 4) + 1; }' 5
assert 'int count = 0; void up() { count++; return; } int main() { up(); up(); return count; }' 2
assert 'int main() { return 1,2,3; }' 3
assert 'int main(){int a=0;for(int i=0;i<=10;i++)a+=i;return a;}' 55
assert 'int main(){int a=0;for(int i=10;i<0;i++)a+=i;return a;}' 0
assert 'int main(){int i;for(i=0;i<100;i++);return i;}' 100
assert 'int main(){if(1==1&&2==2) return 0; else return 1;}' 0
assert 'int main(){if(1==1&&2==2&&3==3) return 0; else return 1;}' 0
assert 'int main(){int a=0;1==1&&(a=1);return a;}' 1
assert 'int main(){int a=0;1==0&&(a=1);return a;}' 0
assert 'int main(){if(1==1||0==2) return 0; else return 1;}' 0
assert 'int main(){if(1==0||2==1||3==3) return 0; else return 1;}' 0
assert 'int main(){int a=0;1==1||(a=1);return a;}' 0
assert 'int main(){int a=0;1==0||(a=1);return a;}' 1
assert 'int main(){;;;;;; if(1);else {} return 0;}' 0
assert 'int main(){struct {int x; int y; char z;} s; return sizeof(s);}' 12
assert 'int main(){struct {char a; char b; int c;} s; return sizeof(s);}' 8
assert 'int main(){struct {int x; struct {char a; char b;} inner; int y;} s; return sizeof(s);}' 12
assert 'int main(){struct {char a[5]; int b;} s; return sizeof(s);}' 12
assert 'int main(){struct {int x; int y; char z;} s; s.x = 1; s.y = 2; s.z = 3; return s.x + s.y + s.z;}' 6
assert 'int main(){struct {int x; char y; char z;} s; s.x = 1; s.y = 2; s.z = 3; return s.x + s.y + s.z;}' 6
assert 'int main(){struct {char x; char y; char z;} s; s.x = 1; s.y = 2; s.z = 3; if (sizeof(s) != 3) return 0; return s.x + s.y + s.z;}' 6
assert 'int main(){struct s {int x; int y;}; struct s x; return sizeof(x);}' 8
assert 'int main(){struct s {int x; int y;}; struct s x; x.x=1; x.y=2;return x.x+x.y;}' 3
assert 'struct s {int x; int y; char z;} var; int main() {var.x = 1; var.y = 2; var.z = 3; return var.x + var.y + var.z;}' 6
assert 'struct {int a;} sa; struct {char b;} sb; int main() {sa.a=1; sb.b=2; return sa.a+sb.b; }' 3
assert 'struct {int *a;} pa; int x=5; int main() {pa.a = &x; return *pa.a;}' 5
assert 'struct test {int a; int b;}; int main() {struct test *var = malloc(8); var->a=1; var->b=2; return var->a + var->b;}' 3
assert '
struct cons { int car; struct cons *cdr; };
int main() {
    struct cons *second = malloc(16);
    second->car = 2;
    struct cons *first = malloc(16);
    first->cdr = second;
    first->car = 1;
    return first->car + first->cdr->car;
}
' 3
assert "int main() { return 'z' - 'a'; }" 25
assert "int main() { return '\\n'; }" 10
assert "int main() { return '\\0'; }" 0
assert "int main() { return '\\''; }" 39
assert "int main() { return '\\\"'; }" 34
assert "int main() { return '\\\\'; }" 92
assert "int zero(void) { return 0; } int main() { return zero(); }" 0
assert 'int main(){int *p; alloc4(&p, 1,2,4,8); p+=2; return *p;}' 4
assert 'int main(){int *p; alloc4(&p, 1,2,4,8); p+=2; p-=1; return *p;}' 2
assert 'int *p;int main(){ return !p; }' 1
assert 'struct test {char a; char b; int c;};int main() {return sizeof(struct test);}' 8
assert '
void *calloc();
int main() {
    int **tests = calloc(2, sizeof(int*));
    tests[1] = calloc(3, sizeof(int));
    tests[1][1] = 4;
    return tests[1][1] + 5;
}' 9
assert '
int main() {char a=1; char b=2; char c=3; char d=4; int x = 65535; x = a; return x + 5; }' 6
assert 'int main() { int x = 65535; /* 0x00 00 FF FF */ char *c = &x; return *c; }' 255
assert 'int main(){ int i = 0; while(1) { if (++i == 10) break; } return 3;}' 3
assert 'int main(){ int a = 0; for (int i = 0; i <= 8; i++) { if (i % 2 != 0) continue; a += i; } return a; }' 20
assert 'int main(){int s=0; for(int i=1;i<=10;i++){ if(i%2==0) continue; s+=i;} return s;}' 25
assert 'int main(){int i,k; for(i=0,k=0;i<10;i++,k++){ if(i==5) break; } return k*10+i;}' 55
assert "int main(){int i=0,s=0; while(i<10){ i++; if(i%3==0) continue; s+=i;} return s;}" 37
assert 'int main(){int cnt=0; for(int i=0;i<3;i++){ for(int j=0;j<5;j++){ if(j==2) break; cnt++; }} return cnt;}' 6
assert 'int main(){int cnt=0; for(int i=0;i<3;i++){ for(int j=0;j<5;j++){ if(j==2) continue; cnt++; }} return cnt;}' 12
assert 'int main(){int i,k; for(i=0,k=0;i<5;i++,k++){ if(1) continue; } return k;}' 5
assert 'int main(){int cnt=0,flag=0; for(int i=0;i<3;i++){ for(int j=0;j<3;j++){ if(i==1&&j==1){ flag=1; break;} cnt++; } if(flag) break; } return cnt;}' 4
assert 'int main() {int a = 0; do { a++; } while (a < 10); return a; }' 10
assert 'int main() {int a = 0, sum = 0; do { a++; if (a % 2 == 0) continue; sum+=a; } while (a < 10); return sum; }' 25
assert 'int main() {int a = 0; do { a++; if (a == 10) break; } while (1); return a; }' 10

assert 'int main() { int a = 2; switch (a) { case 0: return 1; case 1: return 2; case 2: return 3; default: return 4; }}' 3
assert 'int main() { int a = 1; switch (a) { case 0: break; case 1: a += 3; /* fallthrough */ case 2: a += 4; break; default: return 3; }}' 8
assert 'int main() { int a = 0; switch (a) { default: return 4; case 0: return 1; case 1: return 2; case 2: return 3; } }' 1

assert 'int main() { union { int x; char y; } u; u.x = 258; return u.y; }' 2
assert 'union test {int x; int y;}; int main() { union test u; u.x = 123; return u.y; }' 123
assert 'union test {int x; char y;}; int main() { union test u; u.x = 258; return u.y; }' 2
assert 'union test {int x; int y;} u; int main() { u.x = 123; return u.y; }' 123
assert 'struct test {int x; union {int y; int z;};} s; int main() { s.y = 123; return s.z; }' 123
assert 'int main(){ struct S{ union{ int a; int b; }; int c; } s; s.a=3; s.c=4; return s.a + s.c; }' 7
assert 'int main(){ struct S{ struct{ int x; }; }; struct S s; s.x=5; return s.x; }' 5
assert "int main(){ struct A{ union{ struct{ int x; int y; }; int arr[2]; }; }; struct A a; a.x=2; a.y=3; return a.arr[0]+a.arr[1]; }" 5
assert 'int main(){ struct S{ union{ int x; int y; }; }; struct S s; s.x=42; return s.y; }' 42
assert 'int main(){ struct S{ union{ int x; }; int y; }; struct S s; s.x=7; s.y=5; return s.x + s.y; }' 12
assert 'int main(){ struct S{ union{ int x; }; }; struct S s; s.x=3; int *p=&s.x; *p+=4; return s.x; }' 7
assert 'int main(){ struct T{ struct{ int a; }; struct{ int b; }; }; struct T t; t.a=2; t.b=4; return t.a*t.b; }' 8
assert 'int main(){ struct U{ union{ int a; int b; }; }; struct U u; u.a=10; u.b+=5; return u.a; }' 15
assert 'int main(){ struct S{ union{ int x; }; }; struct S arr[2]; arr[0].x=11; arr[1].x=31; return arr[0].x+arr[1].x; }' 42

assert 'int main(){ enum E{A,B,C}; return A+B+C; }' 3
assert 'int main(){ enum E{A,B}; enum E e=B; return e==B; }' 1
assert 'int main(){ enum E{X,}; return X; }' 0
assert 'int main(){ enum {A,B}; int n=B; switch(n){case A: return 1; case B: return 7;} return 0; }' 7
assert 'int main(){ enum {C0,C1}; return C1; }' 1
assert 'int main(){ enum T{Q}; struct P{ enum T t; }; struct P p; p.t=Q; return p.t; }' 0

assert 'int main(){ typedef int I; I x=40; return x+2; }' 42
assert 'int main(){ typedef int* IP; int x=41; IP p=&x; (*p)++; return x; }' 42
assert 'typedef int T; int main(){ T T=42; return T; }' 42
assert 'typedef int I; int f(I x){ return x+2; } int main(){ return f(40); }' 42
assert 'typedef struct {int a; int b;} S; int main(){ S s; s.a=20; s.b=22; return s.a+s.b; }' 42
assert 'struct S{int a;}; typedef int S; int main(){ struct S x; x.a=40; S y=2; return x.a+y; }' 42
assert 'typedef struct S{int a;} S; int main(){ S s; s.a=42; return s.a; }' 42
assert 'int main(){ typedef int T; return sizeof(T)==sizeof(int); }' 1
assert 'typedef int I; typedef I I2; int main(){ I2 x=42; return x; }' 42
assert 'typedef enum {A,B} E; int main(){ E e=A; return e+B; }' 1
assert 'typedef struct Node Node; struct Node{ int v; Node* next; }; int main(){ struct Node a; a.v=19; a.next=0; struct Node b; b.v=23; b.next=0; a.next=&b; return a.v + a.next->v; }' 42
assert 'typedef int* IP; void add_two(IP p){ *p+=2; } int main(){ int x=40; add_two(&x); return x; }' 42
assert '#define MACRO 5
int main() {return MACRO;}' 5
assert '#define MACRO "hello"
int main() {return MACRO[0];}' 104
assert 'typedef struct Node Node; struct Node{ int v; }; int main(){ Node *a; Node b; a = &b; a->v=42; return a->v; }' 42

echo "all tests passed"
