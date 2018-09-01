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
void vec_pushi(Vector *v, int val);
void *vec_pop(Vector *v);
void *vec_last(Vector *v);
bool vec_contains(Vector *v, void *elem);
bool vec_union1(Vector *v, void *elem);

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

typedef struct Type Type;

typedef struct Type {
  int ty;
  int size;  // sizeof
  int align; // alignof

  // Pointer
  Type *ptr_to;

  // Array
  Type *ary_of;
  int len;

  // Struct
  Map *members;
  int offset;

  // Function
  Type *returning;
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
  TK_SWITCH,    // "switch"
  TK_CASE,      // "case"
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
int get_line_number(Token *t);
char *stringize(Vector *tokens);

/// preprocess.c

Vector *preprocess(Vector *tokens);

/// parse.c

extern int nlabel;

enum {
  ND_NUM = 256, // Number literal
  ND_STRUCT,    // Struct
  ND_DECL,      // declaration
  ND_VARDEF,    // Variable definition
  ND_VARREF,    // Variable reference
  ND_CAST,      // Cast
  ND_IF,        // "if"
  ND_FOR,       // "for"
  ND_DO_WHILE,  // do ... while
  ND_SWITCH,    // switch
  ND_CASE,      // case
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

typedef struct Reg Reg;

// Represents a variable.
typedef struct {
  Type *ty;
  char *name;
  bool is_local;

  // Local variables are compiled to offsets from RBP.
  int offset;

  // Global variables are compiled to labels with optional
  // initialized data.
  char *data;

  // For optimization passes.
  bool address_taken;
  Reg *promoted;
} Var;

typedef struct Node Node;
typedef struct BB BB;

// AST node
typedef struct Node {
  int op;        // Node type
  Type *ty;      // C type
  Node *lhs;     // left-hand side
  Node *rhs;     // right-hand side
  int val;       // Number literal
  Node *expr;    // "return" or expresson stmt
  Vector *stmts; // Compound statement

  char *name;

  // For ND_VARREF
  Var *var;

  // "if" ( cond ) then "else" els
  // "for" ( init; cond; inc ) body
  // "while" ( cond ) body
  // "do" body "while" ( cond )
  // "switch" ( cond ) body
  // "case" val ":" body
  Node *cond;
  Node *then;
  Node *els;
  Node *init;
  Node *inc;
  Node *body;

  // For switch and case
  Vector *cases;
  BB *bb;

  // For case, break and continue
  Node *target;
  BB *break_;
  BB *continue_;

  // Function definition
  Vector *params;

  // Function call
  Vector *args;

  // For error reporting
  Token *token;
} Node;

typedef struct {
  char *name;
  Node *node;
  Vector *lvars;
  Vector *bbs;
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

void dump_ir(Vector *irv);

/// gen_ir.c

enum {
  IR_ADD = 1,
  IR_SUB,
  IR_MUL,
  IR_DIV,
  IR_IMM,
  IR_BPREL,
  IR_MOV,
  IR_RETURN,
  IR_CALL,
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
  IR_BR,
  IR_LOAD,
  IR_LOAD_SPILL,
  IR_STORE,
  IR_STORE_ARG,
  IR_STORE_SPILL,
  IR_NOP,
};

typedef struct Reg {
  int vn; // virtual register number
  int rn; // real register number

  // For optimizer
  Reg *promoted;

  // For regalloc
  int def;
  int last_use;
  bool spill;
  Var *var;
} Reg;

typedef struct BB {
  int label;
  Vector *ir;
  Reg *param;

  // For liveness analysis
  Vector *succ;
  Vector *pred;
  Vector *def_regs;
  Vector *in_regs;
  Vector *out_regs;
} BB;

typedef struct {
  int op;

  Reg *r0;
  Reg *r1;
  Reg *r2;

  int imm;
  int label;
  Var *var;

  BB *bb1;
  BB *bb2;

  // Load/store size in bytes
  int size;

  // Function call
  char *name;
  int nargs;
  Reg *args[6];

  // For liveness tracking
  Vector *kill;

  // For SSA
  Reg *bbarg;
} IR;

void gen_ir(Program *prog);
Reg *new_reg();

/// opt.c

void optimize(Program *prog);

/// liveness.c

void liveness(Program *prog);

/// liveness.c

void liveness(Program *prog);

/// regalloc.c

void alloc_regs(Program *prog);

/// gen_x86.c

extern char *regs[];
extern char *regs8[];
extern char *regs32[];
extern int num_regs;

void gen_x86(Program *prog);
