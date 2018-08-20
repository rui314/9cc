extern int *stderr;

#define EXPECT(expected, expr)                                  \
  do {                                                          \
    int e1 = (expected);                                        \
    int e2 = (expr);                                            \
    if (e1 == e2) {                                             \
      fprintf(stderr, "%s => %d\n", #expr, e2);                 \
    } else {                                                    \
      fprintf(stderr, "%d: %s: %d expected, but got %d\n",      \
              __LINE__, #expr, e1, e2);                         \
      exit(1);                                                  \
    }                                                           \
  } while (0)

int one() { return 1; }
int two() { return 2; }
int plus(int x, int y) { return x + y; }
int mul(int x, int y) { return x * y; }
int add(int a, int b, int c, int d, int e, int f) { return a+b+c+d+e+f; }

int var1;
int var2[5];
extern int global_arr[1];

int main() {
  EXPECT(0, 0);
  EXPECT(1, 1);
  EXPECT(2, 1+1);
  EXPECT(10, 2*3+4);
  EXPECT(26, 2*3+4*5);
  EXPECT(5, 50/10);
  EXPECT(9, 6*3/2);
  EXPECT(45, (2+3)*(4+5));
  EXPECT(153, 1+2+3+4+5+6+7+8+9+10+11+12+13+14+15+16+17);

  EXPECT(2, ({ int a=2; return a; }));
  EXPECT(10, ({ int a=2; int b; b=3+2; return a*b; }));
  EXPECT(2, ({ if (1) return 2; return 3; }));
  EXPECT(3, ({ if (0) return 2; return 3; }));
  EXPECT(2, ({ if (1) return 2; else return 3; }));
  EXPECT(3, ({ if (0) return 2; else return 3; }));

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

  EXPECT(45, ({ int x=0; int y=0; do { y=y+x; x=x+1; } while (x < 10); return y; }));

  EXPECT(60, ({ int sum=0; int i; for (i=10; i<15; i=i+1) sum = sum + i; return sum;}));
  EXPECT(89, ({ int i=1; int j=1; for (int k=0; k<10; k=k+1) { int m=i+j; i=j; j=m; } return i;}));

  EXPECT(3, ({ int ary[2]; *ary=1; *(ary+1)=2; return *ary + *(ary+1);}));
  EXPECT(5, ({ int x; int *p = &x; x = 5; return *p;}));

  EXPECT(3, ({ int ary[2]; ary[0]=1; ary[1]=2; return ary[0] + ary[0+1];}));
  EXPECT(5, ({ int x; int *p = &x; x = 5; return p[0];}));

  EXPECT(1, ({ char x; return sizeof x; }));
  EXPECT(4, ({ int x; return sizeof(x); }));
  EXPECT(8, ({ int *x; return sizeof x; }));
  EXPECT(16, ({ int x[4]; return sizeof x; }));

  EXPECT(5, ({ char x = 5; return x; }));
  EXPECT(42, ({ int x = 0; char *p = &x; p[0] = 42; return x; }));

  EXPECT(97, ({ char *p = "abc"; return p[0]; }));
  EXPECT(98, ({ char *p = "abc"; return p[1]; }));
  EXPECT(99, ({ char *p = "abc"; return p[2]; }));
  EXPECT(0, ({ char *p = "abc"; return p[3]; }));

  EXPECT(1, ({ int x = 1; { int x = 2; } return x; }));

  EXPECT(0, var1);
  EXPECT(5, ({ var1 = 5; return var1; }));
  EXPECT(20, sizeof(var2));
  EXPECT(15, ({ var2[0] = 5; var2[4] = 10; return var2[0] + var2[4]; }));
  EXPECT(5, global_arr[0]);

  EXPECT(8, ({ return 3 + ({ return 5; }); }));

  printf("OK\n");
  return 0;
}
