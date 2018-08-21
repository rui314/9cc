#include "9cc.h"

// 9cc's code generation is two-pass. In the first pass, abstract
// syntax trees are compiled to IR (intermediate representation).
//
// IR resembles the real x86-64 instruction set, but it has infinite
// number of registers. We don't try too hard to reuse registers in
// this pass. Instead, we "kill" registers to mark them as dead when
// we are done with them and use new registers.
//
// Such infinite number of registers are mapped to a finite registers
// in a later pass.

static Vector *code;
static int nreg;
static int nlabel;
static int return_label;
static int return_reg;

static IR *add(int op, int lhs, int rhs) {
  IR *ir = calloc(1, sizeof(IR));
  ir->op = op;
  ir->lhs = lhs;
  ir->rhs = rhs;
  vec_push(code, ir);
  return ir;
}

static void kill(int r) { add(IR_KILL, r, -1); }
static void label(int x) { add(IR_LABEL, x, -1); }
static int gen_expr(Node *node);

static int choose_insn(Node *node, int op8, int op32, int op64) {
  if (node->ty->size == 1)
    return op8;
  if (node->ty->size == 4)
    return op32;
  assert(node->ty->size == 8);
  return op64;
}

static int load_insn(Node *node) {
  return choose_insn(node, IR_LOAD8, IR_LOAD32, IR_LOAD64);
}

static int store_insn(Node *node) {
  return choose_insn(node, IR_STORE8, IR_STORE32, IR_STORE64);
}

static int store_arg_insn(Node *node) {
  return choose_insn(node, IR_STORE8_ARG, IR_STORE32_ARG, IR_STORE64_ARG);
}

// In C, all expressions that can be written on the left-hand side of
// the '=' operator must have an address in memory. In other words, if
// you can apply the '&' operator to take an address of some
// expression E, you can assign E to a new value.
//
// Other expressions, such as `1+2`, cannot be written on the lhs of
// '=', since they are just temporary values that don't have an address.
//
// The stuff that can be written on the lhs of '=' is called lvalue.
// Other values are called rvalue. An lvalue is essentially an address.
//
// When lvalues appear on the rvalue context, they are converted to
// rvalues by loading their values from their addresses. You can think
// '&' as an operator that suppresses such automatic lvalue-to-rvalue
// conversion.
//
// This function evaluates a given node as an lvalue.
static int gen_lval(Node *node) {
  if (node->op == ND_DEREF)
    return gen_expr(node->expr);

  if (node->op == ND_LVAR) {
    int r = nreg++;
    add(IR_BPREL, r, node->offset);
    return r;
  }

  assert(node->op == ND_GVAR);
  int r = nreg++;
  IR *ir = add(IR_LABEL_ADDR, r, -1);
  ir->name = node->name;
  return r;
}

static int gen_binop(int ty, Node *node) {
  int lhs = gen_expr(node->lhs);
  int rhs = gen_expr(node->rhs);
  add(ty, lhs, rhs);
  kill(rhs);
  return lhs;
}

static void gen_stmt(Node *node);

static int gen_expr(Node *node) {
  switch (node->op) {
  case ND_NUM: {
    int r = nreg++;
    add(IR_IMM, r, node->val);
    return r;
  }
  case ND_EQ:
    return gen_binop(IR_EQ, node);
  case ND_NE:
    return gen_binop(IR_NE, node);
  case ND_LOGAND: {
    int x = nlabel++;

    int r1 = gen_expr(node->lhs);
    add(IR_UNLESS, r1, x);
    int r2 = gen_expr(node->rhs);
    add(IR_MOV, r1, r2);
    kill(r2);
    add(IR_UNLESS, r1, x);
    add(IR_IMM, r1, 1);
    label(x);
    return r1;
  }
  case ND_LOGOR: {
    int x = nlabel++;
    int y = nlabel++;

    int r1 = gen_expr(node->lhs);
    add(IR_UNLESS, r1, x);
    add(IR_IMM, r1, 1);
    add(IR_JMP, y, -1);
    label(x);

    int r2 = gen_expr(node->rhs);
    add(IR_MOV, r1, r2);
    kill(r2);
    add(IR_UNLESS, r1, y);
    add(IR_IMM, r1, 1);
    label(y);
    return r1;
  }
  case ND_GVAR:
  case ND_LVAR: {
    int r = gen_lval(node);
    add(load_insn(node), r, r);
    return r;
  }
  case ND_CALL: {
    int args[6];
    for (int i = 0; i < node->args->len; i++)
      args[i] = gen_expr(node->args->data[i]);

    int r = nreg++;

    IR *ir = add(IR_CALL, r, -1);
    ir->name = node->name;
    ir->nargs = node->args->len;
    memcpy(ir->args, args, sizeof(args));

    for (int i = 0; i < ir->nargs; i++)
      kill(ir->args[i]);
    return r;
  }
  case ND_ADDR:
    return gen_lval(node->expr);
  case ND_DEREF: {
    int r = gen_expr(node->expr);
    add(load_insn(node), r, r);
    return r;
  }
  case ND_STMT_EXPR: {
    int orig_label = return_label;
    int orig_reg = return_reg;
    return_label = nlabel++;
    int r = nreg++;
    return_reg = r;

    gen_stmt(node->body);
    label(return_label);

    return_label = orig_label;
    return_reg = orig_reg;
    return r;
  }
  case '=': {
    int rhs = gen_expr(node->rhs);
    int lhs = gen_lval(node->lhs);
    add(store_insn(node), lhs, rhs);
    kill(rhs);
    return lhs;
  }
  case '+':
  case '-': {
    int insn = (node->op == '+') ? IR_ADD : IR_SUB;

    if (node->lhs->ty->ty != PTR)
      return gen_binop(insn, node);

    int rhs = gen_expr(node->rhs);
    int r = nreg++;
    add(IR_IMM, r, node->lhs->ty->ptr_to->size);
    add(IR_MUL, rhs, r);
    kill(r);

    int lhs = gen_expr(node->lhs);
    add(insn, lhs, rhs);
    kill(rhs);
    return lhs;
  }
  case '*':
    return gen_binop(IR_MUL, node);
  case '/':
    return gen_binop(IR_DIV, node);
  case '<':
    return gen_binop(IR_LT, node);
  default:
    assert(0 && "unknown AST type");
  }
}

static void gen_stmt(Node *node) {
  if (node->op == ND_NULL)
    return;

  if (node->op == ND_VARDEF) {
    if (!node->init)
      return;
    int rhs = gen_expr(node->init);
    int lhs = nreg++;
    add(IR_BPREL, lhs, node->offset);
    add(store_insn(node), lhs, rhs);
    kill(lhs);
    kill(rhs);
    return;
  }

  if (node->op == ND_IF) {
    if (node->els) {
      int x = nlabel++;
      int y = nlabel++;
      int r = gen_expr(node->cond);
      add(IR_UNLESS, r, x);
      kill(r);
      gen_stmt(node->then);
      add(IR_JMP, y, -1);
      label(x);
      gen_stmt(node->els);
      label(y);
      return;
    }

    int x = nlabel++;
    int r = gen_expr(node->cond);
    add(IR_UNLESS, r, x);
    kill(r);
    gen_stmt(node->then);
    label(x);
    return;
  }

  if (node->op == ND_FOR) {
    int x = nlabel++;
    int y = nlabel++;

    gen_stmt(node->init);
    label(x);
    int r = gen_expr(node->cond);
    add(IR_UNLESS, r, y);
    kill(r);
    gen_stmt(node->body);
    gen_stmt(node->inc);
    add(IR_JMP, x, -1);
    label(y);
    return;
  }

  if (node->op == ND_DO_WHILE) {
    int x = nlabel++;
    label(x);
    gen_stmt(node->body);
    int r = gen_expr(node->cond);
    add(IR_IF, r, x);
    kill(r);
    return;
  }

  if (node->op == ND_RETURN) {
    int r = gen_expr(node->expr);

    // Statement expression (GNU extension)
    if (return_label) {
      add(IR_MOV, return_reg, r);
      kill(r);
      add(IR_JMP, return_label, -1);
      return;
    }

    add(IR_RETURN, r, -1);
    kill(r);
    return;
  }

  if (node->op == ND_EXPR_STMT) {
    kill(gen_expr(node->expr));
    return;
  }

  if (node->op == ND_COMP_STMT) {
    for (int i = 0; i < node->stmts->len; i++)
      gen_stmt(node->stmts->data[i]);
    return;
  }

  error("unknown node: %d", node->op);
}

Vector *gen_ir(Vector *nodes) {
  Vector *v = new_vec();
  nlabel = 1;

  for (int i = 0; i < nodes->len; i++) {
    Node *node = nodes->data[i];

    if (node->op == ND_VARDEF)
      continue;

    assert(node->op == ND_FUNC);
    code = new_vec();

    for (int i = 0; i < node->args->len; i++) {
      Node *arg = node->args->data[i];
      add(store_arg_insn(arg), arg->offset, i);
    }

    gen_stmt(node->body);

    Function *fn = malloc(sizeof(Function));
    fn->name = node->name;
    fn->stacksize = node->stacksize;
    fn->ir = code;
    fn->globals = node->globals;
    vec_push(v, fn);
  }
  return v;
}
