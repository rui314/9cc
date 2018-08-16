#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>
#include <string.h>

// Tokenizer

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

// Recursive-descendent parser

int pos = 0;

enum {
  ND_NUM = 256,     // Number literal
};

typedef struct Node {
  int ty;           // Node type
  struct Node *lhs; // left-hand side
  struct Node *rhs; // right-hand side
  int val;          // Number literal
} Node;

Node *new_node(int op, Node *lhs, Node *rhs) {
  Node *node = malloc(sizeof(Node));
  node->ty = op;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_node_num(int val) {
  Node *node = malloc(sizeof(Node));
  node->ty = ND_NUM;
  node->val = val;
  return node;
}

// An error reporting function.
noreturn void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

Node *number() {
  if (tokens[pos].ty == TK_NUM)
    return new_node_num(tokens[pos++].val);
  error("number expected, but got %s", tokens[pos].input);
}

Node *expr() {
  Node *lhs = number();
  for (;;) {
    int op = tokens[pos].ty;
    if (op != '+' && op != '-')
      break;
    pos++;
    lhs = new_node(op, lhs, number());
  }

  if (tokens[pos].ty != TK_EOF)
    error("stray token: %s", tokens[pos].input);
  return lhs;
}

// Intermediate representation

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

IR *new_ir(int op, int lhs, int rhs) {
  IR *ir = malloc(sizeof(IR));
  ir->op = op;
  ir->lhs = lhs;
  ir->rhs = rhs;
  return ir;
}

IR *ins[1000];
int inp;
int regno;

int gen_ir_sub(Node *node) {
  if (node->ty == ND_NUM) {
    int r = regno++;
    ins[inp++] = new_ir(IR_IMM, r, node->val);
    return r;
  }

  assert(node->ty == '+' || node->ty == '-');

  int lhs = gen_ir_sub(node->lhs);
  int rhs = gen_ir_sub(node->rhs);

  ins[inp++] = new_ir(node->ty, lhs, rhs);
  ins[inp++] = new_ir(IR_KILL, rhs, 0);
  return lhs;
}

void gen_ir(Node *node) {
  int r = gen_ir_sub(node);
  ins[inp++] = new_ir(IR_RETURN, r, 0);
}

// Register allocator

char *regs[] = {"rdi", "rsi", "r10", "r11", "r12", "r13", "r14", "r15"};
bool used[8];

int reg_map[1000];

int alloc(int ir_reg) {
  if (reg_map[ir_reg] != -1) {
    int r = reg_map[ir_reg];
    assert(used[r]);
    return r;
  }

  for (int i = 0; i < sizeof(regs) / sizeof(*regs); i++) {
    if (used[i])
      continue;
    used[i] = true;
    reg_map[ir_reg] = i;
    return i;
  }
  error("register exhausted");
}

void kill(int r) {
  assert(used[r]);
  used[r] = false;
}

void alloc_regs() {
  for (int i = 0; i < inp; i++) {
    IR *ir = ins[i];

    switch (ir->op) {
    case IR_IMM:
      ir->lhs = alloc(ir->lhs);
      break;
    case IR_MOV:
    case '+':
    case '-':
      ir->lhs = alloc(ir->lhs);
      ir->rhs = alloc(ir->rhs);
      break;
    case IR_RETURN:
      kill(reg_map[ir->lhs]);
      break;
    case IR_KILL:
      kill(reg_map[ir->lhs]);
      ir->op = IR_NOP;
      break;
    default:
      assert(0 && "unknown operator");
    }
  }
}

// Code generator

void gen_x86() {
  for (int i = 0; i < inp; i++) {
    IR *ir = ins[i];

    switch (ir->op) {
    case IR_IMM:
      printf("  mov %s, %d\n", regs[ir->lhs], ir->rhs);
      break;
    case IR_MOV:
      printf("  mov %s, %s\n", regs[ir->lhs], regs[ir->rhs]);
      break;
    case IR_RETURN:
      printf("  mov rax, %s\n", regs[ir->lhs]);
      printf("  ret\n");
      break;
    case '+':
      printf("  add %s, %s\n", regs[ir->lhs], regs[ir->rhs]);
      break;
    case '-':
      printf("  sub %s, %s\n", regs[ir->lhs], regs[ir->rhs]);
      break;
    case IR_NOP:
      break;
    default:
      assert(0 && "unknown operator");
    }
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: 9cc <code>\n");
    return 1;
  }

  for (int i = 0; i < sizeof(reg_map) / sizeof(*reg_map); i++)
    reg_map[i] = -1;

  // Tokenize and parse.
  tokenize(argv[1]);
  Node* node = expr();

  gen_ir(node);
  alloc_regs();

  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");
  gen_x86();
  return 0;
}
