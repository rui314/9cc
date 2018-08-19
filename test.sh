#!/bin/bash
try() {
  expected="$1"
  input="$2"

  ./9cc "$input" > tmp.s
  gcc -static -o tmp tmp.s tmp-plus.o
  ./tmp
  actual="$?"

  if [ "$actual" == "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input: $expected expected, but got $actual"
    exit 1
  fi
}

echo 'int plus(int x, int y) { return x + y; }' | gcc -xc -c -o tmp-plus.o -

try 1 'int main() { return 1; }'
try 10 'int main() { return 2*3+4; }'
try 14 'int main() { return 2+3*4; }'
try 26 'int main() { return 2*3+4*5; }'
try 5 'int main() { return 50/10; }'
try 9 'int main() { return 6*3/2; }'

try 0 'int main() { return 0; }'
try 42 'int main() { return 42; }'
try 21 'int main() { 1+2; return 5+20-4; }'
try 41 'int main() { return  12 + 34 - 5 ; }'
try 45 'int main() { return (2+3)*(4+5); }'
try 153 'int main() { return 1+2+3+4+5+6+7+8+9+10+11+12+13+14+15+16+17; }'

try 2 'int main() { int a=2; return a; }'
try 10 'int main() { int a=2; int b; b=3+2; return a*b; }'
try 2 'int main() { if (1) return 2; return 3; }'
try 3 'int main() { if (0) return 2; return 3; }'
try 2 'int main() { if (1) return 2; else return 3; }'
try 3 'int main() { if (0) return 2; else return 3; }'

try 5 'int main() { return plus(2, 3); }'
try 1 'int one() { return 1; } int main() { return one(); }'
try 3 'int one() { return 1; } int two() { return 2; } int main() { return one()+two(); }'
try 6 'int mul(a, b) { return a * b; } int main() { return mul(2, 3); }'
try 21 'int add(a,b,c,d,e,f) { return a+b+c+d+e+f; } int main() { return add(1,2,3,4,5,6); }'

try 0 'int main() { return 0||0; }'
try 1 'int main() { return 1||0; }'
try 1 'int main() { return 0||1; }'
try 1 'int main() { return 1||1; }'

try 0 'int main() { return 0&&0; }'
try 0 'int main() { return 1&&0; }'
try 0 'int main() { return 0&&1; }'
try 1 'int main() { return 1&&1; }'

try 0 'int main() { return 0<0; }'
try 0 'int main() { return 1<0; }'
try 1 'int main() { return 0<1; }'
try 0 'int main() { return 0>0; }'
try 0 'int main() { return 0>1; }'
try 1 'int main() { return 1>0; }'

try 60 'int main() { int sum=0; int i; for (i=10; i<15; i=i+1) sum = sum + i; return sum;}'
try 89 'int main() { int i=1; int j=1; int k; for (k=0; k<10; k=k+1) { int m=i+j; i=j; j=m; } return i;}'

echo OK
