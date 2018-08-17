#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

/// util.c

noreturn void error(char *fmt, ...);
char *format(char *fmt, ...);

typedef struct {
  void **data;
  int capacity;
  int len;
} Vector;

Vector *new_vec(void);
void vec_push(Vector *v, void *elem);

typedef struct {
  Vector *keys;
  Vector *vals;
} Map;

Map *new_map(void);
void map_put(Map *map, char *key, void *val);
void *map_get(Map *map, char *key);
bool map_exists(Map *map, char *key);

typedef struct {
  char *data;
  int capacity;
  int len;
} StringBuilder;

StringBuilder *new_sb(void);
void sb_append(StringBuilder *sb, char *s);
char *sb_get(StringBuilder *sb);

/// util_test.c

void util_test();

/// token.c

enum {
  TK_NUM = 256, // Number literal
  TK_IDENT,     // Identifier
  TK_IF,        // "if"
  TK_ELSE,      // "else"
  TK_RETURN,    // "return"
  TK_EOF,       // End marker
};

// Token type
typedef struct {
  int ty;      // Token type
  int val;     // Number literal
  char *name;  // Identifier
  char *input; // Token string (for error reporting)
} Token;

Vector *tokenize(char *p);

/// parse.c

enum {
  ND_NUM = 256,     // Number literal
  ND_IDENT,         // Identifier
  ND_IF,            // "if"
  ND_RETURN,        // "return"
  ND_CALL,       // Function call
  ND_COMP_STMT,     // Compound statement
  ND_EXPR_STMT,     // Expressions tatement
};

typedef struct Node {
  int ty;            // Node type
  struct Node *lhs;  // left-hand side
  struct Node *rhs;  // right-hand side
  int val;           // Number literal
  struct Node *expr; // "return" or expresson stmt
  Vector *stmts;     // Compound statement

  char *name;

  // "if"
  struct Node *cond;
  struct Node *then;
  struct Node *els;

  // Function call
  Vector *args;
} Node;

Node *parse(Vector *tokens);

/// ir.c

enum {
  IR_IMM = 256,
  IR_ADD_IMM,
  IR_MOV,
  IR_RETURN,
  IR_CALL,
  IR_LABEL,
  IR_JMP,
  IR_UNLESS,
  IR_ALLOCA,
  IR_LOAD,
  IR_STORE,
  IR_KILL,
  IR_NOP,
};

typedef struct {
  int op;
  int lhs;
  int rhs;

  // Function call
  char *name;
  int nargs;
  int args[6];
} IR;

enum {
  IR_TY_NOARG,
  IR_TY_REG,
  IR_TY_LABEL,
  IR_TY_REG_REG,
  IR_TY_REG_IMM,
  IR_TY_REG_LABEL,
  IR_TY_CALL,
};

typedef struct {
  int op;
  char *name;
  int ty;
} IRInfo;

extern IRInfo irinfo[];
IRInfo *get_irinfo(IR *ir);

Vector *gen_ir(Node *node);
void dump_ir(Vector *irv);

/// regalloc.c

extern char *regs[];
void alloc_regs(Vector *irv);

/// codegen.c
void gen_x86(Vector *irv);

/// main.c
char **argv;
