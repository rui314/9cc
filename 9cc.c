#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  TK_NUM = 256, // Number literal
  TK_EOF,       // End marker
};

// Token type
typedef struct {
  int ty;      // Token type
  int val;     // Number literal
  char *input; // Token string (for error reporting)
} Token;

// Tokenized input is stored to this array.
Token tokens[100];

void tokenize(char *p) {
  int i = 0;
  while (*p) {
    // Skip whitespace
    if (isspace(*p)) {
      p++;
      continue;
    }

    // + or -
    if (*p == '+' || *p == '-') {
      tokens[i].ty = *p;
      tokens[i].input = p;
      i++;
      p++;
      continue;
    }

    // Number
    if (isdigit(*p)) {
      tokens[i].ty = TK_NUM;
      tokens[i].input = p;
      tokens[i].val = strtol(p, &p, 10);
      i++;
      continue;
    }

    fprintf(stderr, "cannot tokenize: %s", p);
    exit(1);
  }

  tokens[i].ty = TK_EOF;
}

// An error reporting function.
void fail(int i) {
  fprintf(stderr, "unexpected token: %s\n",
	  tokens[i].input);
  exit(1);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: 9cc <code>\n");
    return 1;
  }

  tokenize(argv[1]);

  // Print the prologue
  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");

  // Verify that the given expression starts with a number,
  // and then emit the first `mov` instruction.
  if (tokens[0].ty != TK_NUM)
    fail(0);
  printf("  mov rax, %d\n", tokens[0].val);

  // Emit assembly as we consume the sequence of `+ <number>`
  // or `- <number>`.
  int i = 1;
  while (tokens[i].ty != TK_EOF) {
    if (tokens[i].ty == '+') {
      i++;
      if (tokens[i].ty != TK_NUM)
	fail(i);
      printf("  add rax, %d\n", tokens[i].val);
      i++;
      continue;
    }

    if (tokens[i].ty == '-') {
      i++;
      if (tokens[i].ty != TK_NUM)
	fail(i);
      printf("  sub rax, %d\n", tokens[i].val);
      i++;
      continue;
    }

    fail(i);
  }

  printf("  ret\n");
  return 0;
}
