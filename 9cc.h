#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

/// util.c

noreturn void error(char *fmt, ...);

typedef struct {
  void **data;
  int capacity;
  int len;
} Vector;

Vector *new_vec(void);
void vec_push(Vector *v, void *elem);

/// token.c

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

Vector *tokenize(char *p);

/// parse.c

enum {
  ND_NUM = 256,     // Number literal
};

typedef struct Node {
  int ty;           // Node type
  struct Node *lhs; // left-hand side
  struct Node *rhs; // right-hand side
  int val;          // Number literal
} Node;

Node *parse(Vector *tokens);

/// ir.c

enum {
  IR_IMM,
  IR_MOV,
  IR_RETURN,
  IR_KILL,
  IR_NOP,
};

typedef struct {
  int op;
  int lhs;
  int rhs;
} IR;

Vector *gen_ir(Node *node);

/// regalloc.c

extern char *regs[];
void alloc_regs(Vector *irv);

/// codegen.c
void gen_x86(Vector *irv);
