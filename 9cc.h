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

typedef struct Type {
  int ty;

  // Pointer
  struct Type *ptr_of;

  // Array
  struct Type *ary_of;
  int len;
} Type;

Type *ptr_of(Type *base);
Type *ary_of(Type *base, int len);
int size_of(Type *ty);

/// util_test.c

void util_test();

/// token.c

enum {
  TK_NUM = 256, // Number literal
  TK_IDENT,     // Identifier
  TK_INT,       // "int"
  TK_CHAR,      // "char"
  TK_IF,        // "if"
  TK_ELSE,      // "else"
  TK_FOR,       // "for"
  TK_LOGOR,     // ||
  TK_LOGAND,    // &&
  TK_RETURN,    // "return"
  TK_SIZEOF,    // "sizeof"
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
  ND_VARDEF,    // Variable definition
  ND_LVAR,      // Variable reference
  ND_IF,        // "if"
  ND_FOR,       // "for"
  ND_ADDR,      // address-of operator ("&")
  ND_DEREF,     // pointer dereference ("*")
  ND_LOGAND,    // &&
  ND_LOGOR,     // ||
  ND_RETURN,    // "return"
  ND_SIZEOF,    // "sizeof"
  ND_CALL,      // Function call
  ND_FUNC,      // Function definition
  ND_COMP_STMT, // Compound statement
  ND_EXPR_STMT, // Expressions tatement
};

enum {
  INT,
  CHAR,
  PTR,
  ARY,
};

typedef struct Node {
  int op;            // Node type
  Type *ty;          // C type
  struct Node *lhs;  // left-hand side
  struct Node *rhs;  // right-hand side
  int val;           // Number literal
  struct Node *expr; // "return" or expresson stmt
  Vector *stmts;     // Compound statement

  char *name;

  // "if" ( cond ) then "else" els
  // "for" ( init; cond; inc ) body
  struct Node *cond;
  struct Node *then;
  struct Node *els;
  struct Node *init;
  struct Node *inc;
  struct Node *body;

  // Function definition
  int stacksize;

  // Local variable
  int offset;

  // Function call
  Vector *args;
} Node;

Vector *parse(Vector *tokens);
int size_of(Type *ty);

/// sema.c

void sema(Vector *nodes);

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
  IR_LOAD8,
  IR_LOAD32,
  IR_LOAD64,
  IR_STORE8,
  IR_STORE32,
  IR_STORE64,
  IR_STORE8_ARG,
  IR_STORE32_ARG,
  IR_STORE64_ARG,
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
  IR_TY_IMM,
  IR_TY_JMP,
  IR_TY_LABEL,
  IR_TY_REG_REG,
  IR_TY_REG_IMM,
  IR_TY_IMM_IMM,
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
extern char *regs32[];
void alloc_regs(Vector *irv);

/// gen_x86.c
void gen_x86(Vector *fns);
