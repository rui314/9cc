extern void *stderr;

int printf();
int fprintf();
int exit();

#define EXPECT(expected, expr)                                  \
  do {                                                          \
    int e1 = (expected);                                        \
    int e2 = (expr);                                            \
    if (e1 == e2) {                                             \
      fprintf(stderr, "%s => %d\n", #expr, e2);                 \
    } else {                                                    \
      fprintf(stderr, "line %d: %s: %d expected, but got %d\n", \
              __LINE__, #expr, e1, e2);                         \
      exit(1);                                                  \
    }                                                           \
  } while (0)

int one() { return 1; }
int two() { return 2; }
int plus(int x, int y) { return x + y; }
int mul(int x, int y) { return x * y; }
int add(int a, int b, int c, int d, int e, int f) { return a+b+c+d+e+f; }
int add2(int (*a)[2]) { return a[0][0] + a[1][0]; }
int add3(int a[][2]) { return a[0][0] + a[1][0]; }
int add4(int a[2][2]) { return a[0][0] + a[1][0]; }
void nop() {}

int var1;
int var2[5];
extern int global_arr[1];
typedef int myint;

// Single-line comment test

/***************************
 * Multi-line comment test *
 ***************************/

int main() {
  EXPECT(0, 0);
  EXPECT(1, 1);
  EXPECT(493, 0755);
  EXPECT(48879, 0xBEEF);
  EXPECT(255, 0Xff);
  EXPECT(2, 1+1);
  EXPECT(10, 2*3+4);
  EXPECT(26, 2*3+4*5);
  EXPECT(5, 50/10);
  EXPECT(9, 6*3/2);
  EXPECT(45, (2+3)*(4+5));
  EXPECT(153, 1+2+3+4+5+6+7+8+9+10+11+12+13+14+15+16+17);

  EXPECT(2, ({ int a=2; a; }));
  EXPECT(10, ({ int a=2; int b; b=3+2; a*b; }));
  EXPECT(2, ({ int i=3; if (1) i=2; i; }));
  EXPECT(3, ({ int i=3; if (0) i=2; i; }));
  EXPECT(2, ({ int i=0; if (1) i=2; else i=3; i; }));
  EXPECT(3, ({ int i=0; if (0) i=2; else i=3; i; }));

  EXPECT(5, plus(2, 3));
  EXPECT(1, one());
  EXPECT(3, one()+two());
  EXPECT(6, mul(2, 3));
  EXPECT(21, add(1,2,3,4,5,6));

  EXPECT(0, 0 || 0);
  EXPECT(1, 1 || 0);
  EXPECT(1, 0 || 1);
  EXPECT(1, 1 || 1);

  EXPECT(0, 0 && 0);
  EXPECT(0, 1 && 0);
  EXPECT(0, 0 && 1);
  EXPECT(1, 1 && 1);

  EXPECT(0, 0 < 0);
  EXPECT(0, 1 < 0);
  EXPECT(1, 0 < 1);
  EXPECT(0, 0 > 0);
  EXPECT(0, 0 > 1);
  EXPECT(1, 1 > 0);

  EXPECT(0, 4 == 5);
  EXPECT(1, 5 == 5);
  EXPECT(1, 4 != 5);
  EXPECT(0, 5 != 5);

  EXPECT(1, 4 <= 5);
  EXPECT(1, 5 <= 5);
  EXPECT(0, 6 <= 5);

  EXPECT(0, 4 >= 5);
  EXPECT(1, 5 >= 5);
  EXPECT(1, 6 >= 5);

  EXPECT(8, 1 << 3);
  EXPECT(4, 16 >> 2);

  EXPECT(4, 19 % 5);
  EXPECT(0, 9 % 3);

  EXPECT(0-3, -3);

  EXPECT(0, !1);
  EXPECT(1, !0);

  EXPECT(-1, ~0);
  EXPECT(-4, ~3);

  EXPECT(3, ({ int i = 3; i++; }));
  EXPECT(4, ({ int i = 3; ++i; }));
  EXPECT(3, ({ int i = 3; i--; }));
  EXPECT(2, ({ int i = 3; --i; }));

  EXPECT(5, 0 ? 3 : 5);
  EXPECT(3, 1 ? 3 : 5);

  EXPECT(3, (1, 2, 3));

  EXPECT(11, 9 | 2);
  EXPECT(11, 9 | 3);
  EXPECT(5, 6 ^ 3);
  EXPECT(2, 6 & 3);
  EXPECT(0, 6 & 0);

  EXPECT(3, ({ int x; int y; x=y=3; x; }));
  EXPECT(3, ({ int x; int y; x=y=3; y; }));

  EXPECT(45, ({ int x=0; int y=0; do { y=y+x; x=x+1; } while (x < 10); y; }));
  EXPECT(1, ({ int x=0; do {x++; break;} while (1); x; }));
  EXPECT(1, ({ int x=0; do {x++; continue;} while (0); x; }));

  EXPECT(60, ({ int sum=0; int i; for (i=10; i<15; i=i+1) sum = sum + i; sum;}));
  EXPECT(89, ({ int i=1; int j=1; for (int k=0; k<10; k=k+1) { int m=i+j; i=j; j=m; } i;}));
  EXPECT(1, ({ int i=1; for (int i = 5; i < 10; i++); i; }));
  EXPECT(5, ({ int i=0; for (; i < 10; i++) if (i==5) break; i; }));
  EXPECT(10, ({ int i=0; for (;;) { i++; if (i==10) break; } i; }));

  EXPECT(7, ({ int i=0; for (int j=0; j < 10; j++) { if (j<3) continue; i++; } i; }));

  EXPECT(45, ({ int i=0; int j=0; while (i<10) { j=j+i; i=i+1; } j;}));

  EXPECT(6, ({ int x=0; switch(3) { case 2: x=5; break; case 3: x=6; break; case 4: x=7; break; } x; }));
  EXPECT(7, ({ int x=0; switch(3) { case 2: x=5; case 3: x=6; case 4: x=7; } x; }));
  EXPECT(0, ({ int x=0; switch(3) case 1: x=5; x; }));

  EXPECT(3, ({ int ary[2]; *ary=1; *(ary+1)=2; *ary + *(ary+1);}));
  EXPECT(5, ({ int x; int *p = &x; x = 5; *p;}));
  EXPECT(4, ({ int *p; (p+5)-(p+1); }));

  EXPECT(40, ({ int ary[2][5]; sizeof(ary);}));
  EXPECT(8, ({ int ary[2][2]; ary[0][0]=3; ary[1][0]=5; add2(ary);}));
  EXPECT(8, ({ int ary[2][2]; ary[0][0]=3; ary[1][0]=5; add3(ary);}));
  EXPECT(8, ({ int ary[2][2]; ary[0][0]=3; ary[1][0]=5; add4(ary);}));

  EXPECT(3, ({ int ary[2]; ary[0]=1; ary[1]=2; ary[0] + ary[0+1];}));
  EXPECT(5, ({ int x; int *p = &x; x = 5; p[0];}));
  EXPECT(1, ({ int ary[2]; ary[0]=1; ary[1]=2; int *p=ary; *p++;}));
  EXPECT(2, ({ int ary[2]; ary[0]=1; ary[1]=2; int *p=ary; *++p;}));

  EXPECT(1, ({ char x; sizeof x; }));
  EXPECT(4, ({ int x; sizeof(x); }));
  EXPECT(8, ({ int *x; sizeof x; }));
  EXPECT(16, ({ int x[4]; sizeof x; }));
  EXPECT(4, sizeof("abc"));
  EXPECT(7, sizeof("abc" "def"));
  EXPECT(9, sizeof("ab\0c" "\0def"));

  EXPECT(1, ({ char x; _Alignof x; }));
  EXPECT(4, ({ int x; _Alignof(x); }));
  EXPECT(8, ({ int *x; _Alignof x; }));
  EXPECT(4, ({ int x[4]; _Alignof x; }));
  EXPECT(8, ({ int *x[4]; _Alignof x; }));

  EXPECT(5, ({ char x = 5; x; }));
  EXPECT(42, ({ int x = 0; char *p = &x; p[0] = 42; x; }));

  EXPECT(0, '\0');
  EXPECT(0, '\00');
  EXPECT(0, '\000');
  EXPECT(1, '\1');
  EXPECT(7, '\7');
  EXPECT(64, '\100');

  EXPECT(64, "\10000"[0]);
  EXPECT('0', "\10000"[1]);
  EXPECT('0', "\10000"[2]);
  EXPECT(0, "\10000"[3]);
  EXPECT(255, "\xffxyz"[0]);
  EXPECT('x', "\xffxyz"[1]);

  EXPECT('a', ({ char *p = "abc"; p[0]; }));
  EXPECT('b', ({ char *p = "abc"; p[1]; }));
  EXPECT('c', ({ char *p = "abc"; p[2]; }));
  EXPECT(0, ({ char *p = "abc"; p[3]; }));

  EXPECT(1, ({ int x = 1; { int x = 2; } x; }));

  EXPECT(0, var1);
  EXPECT(5, ({ var1 = 5; var1; }));
  EXPECT(20, sizeof(var2));
  EXPECT(15, ({ var2[0] = 5; var2[4] = 10; var2[0] + var2[4]; }));
  EXPECT(5, global_arr[0]);

  EXPECT(4, ({ struct { int a; } x; sizeof(x); }));
  EXPECT(8, ({ struct { char a; int b; } x; sizeof(x); }));
  EXPECT(12, ({ struct { char a; char b; int c; char d; } x; sizeof(x); }));
  EXPECT(3, ({ struct { int a; } x; x.a=3; x.a; }));
  EXPECT(8, ({ struct { char a; int b; } x; x.a=3; x.b=5; x.a+x.b; }));
  EXPECT(8, ({ struct { char a; int b; } x; struct { char a; int b; } *p = &x; x.a=3; x.b=5; p->a+p->b; }));
  EXPECT(8, ({ struct tag { char a; int b; } x; struct tag *p = &x; x.a=3; x.b=5; p->a+p->b; }));
  EXPECT(48, ({ struct { struct { int b; int c[5]; } a[2]; } x; sizeof(x); }));

  EXPECT(8, ({
	struct {
	  struct {
	    int b;
	    int c[5];
	  } a[2];
	} x;
	x.a[0].b = 3;
	x.a[0].c[1] = 5;
	x.a[0].b + x.a[0].c[1];
      }));

  EXPECT(3, ({ typedef int foo; foo x = 3; x; }));
  EXPECT(4, ({ myint foo = 3; sizeof(foo); }));

  EXPECT(1, ({ typedef struct foo_ foo; 1; }));

  EXPECT(15, ({ int i=5; i*=3; i; }));
  EXPECT(1, ({ int i=5; i/=3; i; }));
  EXPECT(2, ({ int i=5; i%=3; i; }));
  EXPECT(8, ({ int i=5; i+=3; i; }));
  EXPECT(2, ({ int i=5; i-=3; i; }));
  EXPECT(40, ({ int i=5; i<<=3; i; }));
  EXPECT(0, ({ int i=5; i>>=3; i; }));
  EXPECT(1, ({ int i=5; i&=3; i; }));
  EXPECT(6, ({ int i=5; i^=3; i; }));
  EXPECT(7, ({ int i=5; i|=3; i; }));

  EXPECT(5, ({ int x; typeof(x) y = 5; y; }));
  EXPECT(1, ({ char x; typeof(x) y = 257; y; }));
  EXPECT(2, ({ char x; typeof(x) y[2]; y[0]=257; y[1]=1; y[0]+y[1]; }));

  EXPECT(0, ({ _Bool x = 0; x; }));
  EXPECT(1, ({ _Bool x = 1; x; }));
  EXPECT(0, ({ _Bool x; x = 0; x; }));
  EXPECT(1, ({ _Bool x; x = 2; x; }));
  EXPECT(0, ({ _Bool x; int y = 0; x = y; x; }));
  EXPECT(1, ({ _Bool x; int y = -1; x = y; x; }));
  EXPECT(0, ({ _Bool x; _Bool y = 0; x = y; x; }));
  EXPECT(1, ({ _Bool x; _Bool y = 1; x = y; x; }));
  EXPECT(1, ({ _Bool x = 0; !x; }));
  EXPECT(0, ({ _Bool x = 1; !x; }));
  EXPECT(-1, ({ _Bool x = 0; ~x; }));
  EXPECT(-2, ({ _Bool x = 1; ~x; }));

  EXPECT(128, ((((((1+1)+(1+1))+(1+1)+(1+1))+(((1+1)+(1+1))+(1+1)+(1+1)))+((((1+1)+(1+1))+(1+1)+(1+1))+(((1+1)+(1+1))+(1+1)+(1+1))))+(((((1+1)+(1+1))+(1+1)+(1+1))+(((1+1)+(1+1))+(1+1)+(1+1)))+((((1+1)+(1+1))+(1+1)+(1+1))+(((1+1)+(1+1))+(1+1)+(1+1)))))+((((((1+1)+(1+1))+(1+1)+(1+1))+(((1+1)+(1+1))+(1+1)+(1+1)))+((((1+1)+(1+1))+(1+1)+(1+1))+(((1+1)+(1+1))+(1+1)+(1+1))))+(((((1+1)+(1+1))+(1+1)+(1+1))+(((1+1)+(1+1))+(1+1)+(1+1)))+((((1+1)+(1+1))+(1+1)+(1+1))+(((1+1)+(1+1))+(1+1)+(1+1))))));

  printf("OK\n");
  return 0;
}
