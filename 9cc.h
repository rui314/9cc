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

noreturn void error(char *fmt, ...) __attribute__((format(printf, 1, 2)));
char *format(char *fmt, ...) __attribute__((format(printf, 1, 2)));

typedef struct {
  void **data;
  int capacity;
  int len;
} Vector;

Vector *new_vec(void);
void vec_push(Vector *v, void *elem);
void *vec_pop(Vector *v);

typedef struct {
  Vector *keys;
  Vector *vals;
} Map;

Map *new_map(void);
void map_put(Map *map, char *key, void *val);
void map_puti(Map *map, char *key, int val);
void *map_get(Map *map, char *key);
int map_geti(Map *map, char *key, int default_);
bool map_exists(Map *map, char *key);

typedef struct {
  char *data;
  int capacity;
  int len;
} StringBuilder;

StringBuilder *new_sb(void);
void sb_add(StringBuilder *sb, char c);
void sb_append(StringBuilder *sb, char *s);
void sb_append_n(StringBuilder *sb, char *s, int len);
char *sb_get(StringBuilder *sb);

typedef struct Type {
  int ty;
  int size;  // sizeof
  int align; // alignof
  bool is_extern;

  // Pointer
  struct Type *ptr_to;

  // Array
  struct Type *ary_of;
  int len;

  // Struct
  Map *members;
  int offset;

  // Function
  struct Type *returning;
} Type;

Type *ptr_to(Type *base);
Type *ary_of(Type *base, int len);
Type *void_ty();
Type *bool_ty();
Type *char_ty();
Type *int_ty();
Type *func_ty(Type *returning);
bool same_type(Type *x, Type *y);
int roundup(int x, int align);

/// util_test.c

void util_test();

/// token.c

enum {
  TK_NUM = 256, // Number literal
  TK_STR,       // String literal
  TK_IDENT,     // Identifier
  TK_ARROW,     // ->
  TK_EXTERN,    // "extern"
  TK_TYPEDEF,   // "typedef"
  TK_INT,       // "int"
  TK_CHAR,      // "char"
  TK_VOID,      // "void"
  TK_STRUCT,    // "struct"
  TK_BOOL,      // "_Bool"
  TK_IF,        // "if"
  TK_ELSE,      // "else"
  TK_FOR,       // "for"
  TK_DO,        // "do"
  TK_WHILE,     // "while"
  TK_BREAK,     // "break"
  TK_CONTINUE,  // "continue"
  TK_EQ,        // ==
  TK_NE,        // !=
  TK_LE,        // <=
  TK_GE,        // >=
  TK_LOGOR,     // ||
  TK_LOGAND,    // &&
  TK_SHL,       // <<
  TK_SHR,       // >>
  TK_INC,       // ++
  TK_DEC,       // --
  TK_MUL_EQ,    // *=
  TK_DIV_EQ,    // /=
  TK_MOD_EQ,    // %=
  TK_ADD_EQ,    // +=
  TK_SUB_EQ,    // -=
  TK_SHL_EQ,    // <<=
  TK_SHR_EQ,    // >>=
  TK_AND_EQ,    // &=
  TK_XOR_EQ,    // ^=
  TK_OR_EQ,     // |=
  TK_RETURN,    // "return"
  TK_SIZEOF,    // "sizeof"
  TK_ALIGNOF,   // "_Alignof"
  TK_TYPEOF,    // "typeof"
  TK_PARAM,     // Function-like macro parameter
  TK_EOF,       // End marker
};

// Token type
typedef struct {
  int ty;     // Token type
  int val;    // Number literal
  char *name; // Identifier

  // String literal
  char *str;
  char len;

  // For preprocessor
  bool stringize;

  // For error reporting
  char *buf;
  char *path;
  char *start;
  char *end;
} Token;

Vector *tokenize(char *path, bool add_eof);
noreturn void bad_token(Token *t, char *msg);
void warn_token(Token *t, char *msg);
char *tokstr(Token *t);
int get_line_number(Token *t);

/// preprocess.c

Vector *preprocess(Vector *tokens);

/// parse.c

extern int nlabel;

enum {
  ND_NUM = 256, // Number literal
  ND_STR,       // String literal
  ND_STRUCT,    // Struct
  ND_DECL,      // declaration
  ND_VARDEF,    // Variable definition
  ND_VARREF,    // Variable reference
  ND_CAST,      // Cast
  ND_IF,        // "if"
  ND_FOR,       // "for"
  ND_DO_WHILE,  // do ... while
  ND_BREAK,     // break
  ND_CONTINUE,  // continue
  ND_ADDR,      // address-of operator ("&")
  ND_DEREF,     // pointer dereference ("*")
  ND_DOT,       // Struct member access
  ND_EQ,        // ==
  ND_NE,        // !=
  ND_LE,        // <=
  ND_LOGAND,    // &&
  ND_LOGOR,     // ||
  ND_SHL,       // <<
  ND_SHR,       // >>
  ND_MOD,       // %
  ND_POST_INC,  // post ++
  ND_POST_DEC,  // post --
  ND_MUL_EQ,    // *=
  ND_DIV_EQ,    // /=
  ND_MOD_EQ,    // %=
  ND_ADD_EQ,    // +=
  ND_SUB_EQ,    // -=
  ND_SHL_EQ,    // <<=
  ND_SHR_EQ,    // >>=
  ND_AND_EQ,    // &=
  ND_XOR_EQ,    // ^=
  ND_OR_EQ,     // |=
  ND_RETURN,    // "return"
  ND_CALL,      // Function call
  ND_FUNC,      // Function definition
  ND_COMP_STMT, // Compound statement
  ND_EXPR_STMT, // Expression statement
  ND_STMT_EXPR, // Statement expression (GNU extn.)
  ND_NULL,      // Null statement
};

enum {
  VOID = 1,
  BOOL,
  CHAR,
  INT,
  PTR,
  ARY,
  STRUCT,
  FUNC,
};

typedef struct {
  Type *ty;
  bool is_local;

  // local
  int offset;

  // global
  char *name;
  char *data;
  int len;
} Var;

typedef struct Node {
  int op;            // Node type
  Type *ty;          // C type
  struct Node *lhs;  // left-hand side
  struct Node *rhs;  // right-hand side
  int val;           // Number literal
  struct Node *expr; // "return" or expresson stmt
  Vector *stmts;     // Compound statement

  char *name;

  // For ND_VARREF
  Var *var;

  // "if" ( cond ) then "else" els
  // "for" ( init; cond; inc ) body
  // "while" ( cond ) body
  // "do" body "while" ( cond )
  struct Node *cond;
  struct Node *then;
  struct Node *els;
  struct Node *init;
  struct Node *inc;
  struct Node *body;

  // For break and continue
  int break_label;
  int continue_label;
  struct Node *target;

  // Function definition and function call
  Vector *args;

  // For error reporting
  Token *token;
} Node;

typedef struct {
  char *name;
  Node *node;
  Vector *lvars;
  Vector *ir;
  int stacksize;
} Function;

// Represents toplevel constructs.
typedef struct {
  Vector *gvars;
  Vector *funcs;
} Program;

Program *parse(Vector *tokens);

Node *new_int_node(int val, Token *t);

/// sema.c

Type *get_type(Node *node);
void sema(Program *prog);

/// ir_dump.c

typedef struct {
  char *name;
  int ty;
} IRInfo;

extern IRInfo irinfo[];

void dump_ir(Vector *irv);

/// gen_ir.c

enum {
  IR_ADD,
  IR_SUB,
  IR_MUL,
  IR_DIV,
  IR_IMM,
  IR_BPREL,
  IR_MOV,
  IR_RETURN,
  IR_CALL,
  IR_LABEL,
  IR_LABEL_ADDR,
  IR_EQ,
  IR_NE,
  IR_LE,
  IR_LT,
  IR_AND,
  IR_OR,
  IR_XOR,
  IR_SHL,
  IR_SHR,
  IR_MOD,
  IR_JMP,
  IR_IF,
  IR_UNLESS,
  IR_LOAD,
  IR_STORE,
  IR_STORE_ARG,
  IR_KILL,
  IR_NOP,
};

typedef struct {
  int op;
  int lhs;
  int rhs;

  // Load/store size in bytes
  int size;

  // For binary operator. If true, rhs is an immediate.
  bool is_imm;

  // Function call
  char *name;
  int nargs;
  int args[6];
} IR;

enum {
  IR_TY_NOARG,
  IR_TY_BINARY,
  IR_TY_REG,
  IR_TY_IMM,
  IR_TY_MEM,
  IR_TY_JMP,
  IR_TY_LABEL,
  IR_TY_LABEL_ADDR,
  IR_TY_REG_REG,
  IR_TY_REG_IMM,
  IR_TY_STORE_ARG,
  IR_TY_REG_LABEL,
  IR_TY_CALL,
};

void gen_ir(Program *prog);

/// regalloc.c

void alloc_regs(Program *prog);

/// gen_x86.c

extern char *regs[];
extern char *regs8[];
extern char *regs32[];
extern int num_regs;

void gen_x86(Program *prog);
