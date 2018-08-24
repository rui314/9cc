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

// Input file
typedef struct {
  char *fname;
  char *text;
} SrcFile;

// Diagnostic location
typedef struct {
  SrcFile *file;
  // A pointer into the file->text;
  char *pos;
} SrcLoc;

/// util.c

noreturn void error(char *fmt, ...);
noreturn void errorloc(SrcLoc loc, char *fmt, ...);
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
  int size;
  int align;

  // Pointer
  struct Type *ptr_to;

  // Array
  struct Type *ary_of;
  int len;

  // Struct
  Vector *members;
  int offset;
} Type;

Type *ptr_to(Type *base);
Type *ary_of(Type *base, int len);
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
  TK_IF,        // "if"
  TK_ELSE,      // "else"
  TK_FOR,       // "for"
  TK_DO,        // "do"
  TK_WHILE,     // "while"
  TK_BREAK,     // "break"
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
  TK_BITAND_EQ, // &=
  TK_XOR_EQ,    // ^=
  TK_BITOR_EQ,  // |=
  TK_RETURN,    // "return"
  TK_SIZEOF,    // "sizeof"
  TK_ALIGNOF,   // "_Alignof"
  TK_EOF,       // End marker
};

// Token type
typedef struct {
  int ty;      // Token type
  int val;     // Number literal
  char *name;  // Identifier
  SrcLoc loc;  // Token location (for error reporting)

  // String literal
  char *str;
  char len;
} Token;

Vector *tokenize(SrcFile *src);

/// parse.c

enum {
  ND_NUM = 256, // Number literal
  ND_STR,       // String literal
  ND_IDENT,     // Identifier
  ND_STRUCT,    // Struct
  ND_VARDEF,    // Variable definition
  ND_LVAR,      // Local variable reference
  ND_GVAR,      // Global variable reference
  ND_IF,        // "if"
  ND_FOR,       // "for"
  ND_DO_WHILE,  // do ... while
  ND_BREAK,     // break
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
  ND_NEG,       // -
  ND_POST_INC,  // post ++
  ND_POST_DEC,  // post --
  ND_MUL_EQ,    // *=
  ND_DIV_EQ,    // /=
  ND_MOD_EQ,    // %=
  ND_ADD_EQ,    // +=
  ND_SUB_EQ,    // -=
  ND_SHL_EQ,    // <<=
  ND_SHR_EQ,    // >>=
  ND_BITAND_EQ, // &=
  ND_XOR_EQ,    // ^=
  ND_BITOR_EQ,  // |=
  ND_RETURN,    // "return"
  ND_SIZEOF,    // "sizeof"
  ND_ALIGNOF,   // "_Alignof"
  ND_CALL,      // Function call
  ND_FUNC,      // Function definition
  ND_COMP_STMT, // Compound statement
  ND_EXPR_STMT, // Expression statement
  ND_STMT_EXPR, // Statement expression (GNU extn.)
  ND_NULL,      // Null statement
};

enum {
  INT,
  CHAR,
  VOID,
  PTR,
  ARY,
  STRUCT,
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

  // Global variable
  bool is_extern;
  char *data;
  int len;

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
  Vector *globals;

  // Offset from BP or begining of a struct
  int offset;

  // Function call
  Vector *args;
} Node;

Vector *parse(Vector *tokens);

/// sema.c

typedef struct {
  Type *ty;
  bool is_local;

  // local
  int offset;

  // global
  char *name;
  bool is_extern;
  char *data;
  int len;
} Var;

Vector *sema(Vector *nodes);

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
  IR_NEG,
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

typedef struct {
  char *name;
  int stacksize;
  Vector *ir;
  Vector *globals;
} Function;

Vector *gen_ir(Vector *fns);

/// regalloc.c

void alloc_regs(Vector *irv);

/// gen_x86.c

extern char *regs[];
extern char *regs8[];
extern char *regs32[];
extern int num_regs;

void gen_x86(Vector *globals, Vector *fns);
