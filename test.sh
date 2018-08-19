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

try 1 'main() { return 1; }'
try 10 'main() { return 2*3+4; }'
try 14 'main() { return 2+3*4; }'
try 26 'main() { return 2*3+4*5; }'
try 5 'main() { return 50/10; }'
try 9 'main() { return 6*3/2; }'

try 0 'main() { return 0; }'
try 42 'main() { return 42; }'
try 21 'main() { 1+2; return 5+20-4; }'
try 41 'main() { return  12 + 34 - 5 ; }'
try 45 'main() { return (2+3)*(4+5); }'
try 153 'main() { return 1+2+3+4+5+6+7+8+9+10+11+12+13+14+15+16+17; }'

try 2 'main() { a=2; return a; }'
try 10 'main() { a=2; b=3+2; return a*b; }'
try 2 'main() { if (1) return 2; return 3; }'
try 3 'main() { if (0) return 2; return 3; }'
try 2 'main() { if (1) return 2; else return 3; }'
try 3 'main() { if (0) return 2; else return 3; }'

try 5 'main() { return plus(2, 3); }'
try 1 'one() { return 1; } main() { return one(); }'
try 3 'one() { return 1; } two() { return 2; } main() { return one()+two(); }'
try 6 'mul(a, b) { return a * b; } main() { return mul(2, 3); }'
try 21 'add(a,b,c,d,e,f) { return a+b+c+d+e+f; } main() { return add(1,2,3,4,5,6); }'

try 0 'main() { return 0||0; }'
try 1 'main() { return 1||0; }'
try 1 'main() { return 0||1; }'
try 1 'main() { return 1||1; }'

try 0 'main() { return 0&&0; }'
try 0 'main() { return 1&&0; }'
try 0 'main() { return 0&&1; }'
try 1 'main() { return 1&&1; }'

try 0 'main() { return 0<0; }'
try 0 'main() { return 1<0; }'
try 1 'main() { return 0<1; }'
try 0 'main() { return 0>0; }'
try 0 'main() { return 0>1; }'
try 1 'main() { return 1>0; }'

try 60 'main() { sum=0; for (i=10; i<15; i=i+1) sum = sum + i; return sum;}'

echo OK
