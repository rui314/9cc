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
  TK_LOGOR,     // ||
  TK_LOGAND,    // &&
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
  ND_NUM = 256, // Number literal
  ND_IDENT,     // Identifier
  ND_IF,        // "if"
  ND_LOGAND,    // &&
  ND_LOGOR,     // ||
  ND_RETURN,    // "return"
  ND_CALL,      // Function call
  ND_FUNC,      // Function definition
  ND_COMP_STMT, // Compound statement
  ND_EXPR_STMT, // Expressions tatement
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

  // Function definition
  struct Node *body;

  // Function call
  Vector *args;
} Node;

Vector *parse(Vector *tokens);

/// gen_ir.c

enum {
  IR_ADD,
  IR_SUB,
  IR_MUL,
  IR_DIV,
  IR_IMM,
  IR_SUB_IMM,
  IR_MOV,
  IR_RETURN,
  IR_CALL,
  IR_LABEL,
  IR_LT,
  IR_JMP,
  IR_UNLESS,
  IR_LOAD,
  IR_STORE,
  IR_KILL,
  IR_SAVE_ARGS,
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
  IR_TY_IMM,
  IR_TY_JMP,
  IR_TY_LABEL,
  IR_TY_REG_REG,
  IR_TY_REG_IMM,
  IR_TY_REG_LABEL,
  IR_TY_CALL,
};

typedef struct {
  char *name;
  int ty;
} IRInfo;

typedef struct {
  char *name;
  int stacksize;
  Vector *ir;
} Function;

extern IRInfo irinfo[];

Vector *gen_ir(Vector *nodes);
void dump_ir(Vector *irv);

/// regalloc.c

extern char *regs[];
extern char *regs8[];
void alloc_regs(Vector *irv);

/// gen_x86.c
void gen_x86(Vector *fns);
