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
static int nreg = 1;

static IR *emit(int op, int lhs, int rhs) {
  IR *ir = calloc(1, sizeof(IR));
  ir->op = op;
  ir->lhs = lhs;
  ir->rhs = rhs;
  ir->kill = new_vec();
  vec_push(code, ir);
  return ir;
}

static void kill(int r) {
  IR *ir = vec_last(code);
  vec_pushi(ir->kill, r);
}

static void label(int x) {
  emit(IR_LABEL, x, -1);
}

static void jmp(int x) {
  emit(IR_JMP, x, -1);
}

static int gen_expr(Node *node);

static void load(Node *node, int dst, int src) {
  IR *ir = emit(IR_LOAD, dst, src);
  ir->size = node->ty->size;
}

static void store(Node *node, int dst, int src) {
  IR *ir = emit(IR_STORE, dst, src);
  ir->size = node->ty->size;
}

static void gen_imm(int op, int r, int imm) {
  int r2 = nreg++;
  emit(IR_IMM, r2, imm);
  emit(op, r, r2);
  kill(r2);
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

  if (node->op == ND_DOT) {
    int r = gen_lval(node->expr);
    gen_imm(IR_ADD, r, node->ty->offset);
    return r;
  }

  assert(node->op == ND_VARREF);
  Var *var = node->var;

  int r = nreg++;
  if (var->is_local) {
    emit(IR_BPREL, r, var->offset);
  } else {
    IR *ir = emit(IR_LABEL_ADDR, r, -1);
    ir->name = var->name;
  }
  return r;
}

static int gen_binop(int ty, Node *node) {
  int lhs = gen_expr(node->lhs);
  int rhs = gen_expr(node->rhs);
  emit(ty, lhs, rhs);
  kill(rhs);
  return lhs;
}

static void gen_stmt(Node *node);

static int gen_expr(Node *node) {
  switch (node->op) {
  case ND_NUM: {
    int r = nreg++;
    emit(IR_IMM, r, node->val);
    return r;
  }
  case ND_EQ:
    return gen_binop(IR_EQ, node);
  case ND_NE:
    return gen_binop(IR_NE, node);
  case ND_LOGAND: {
    int x = nlabel++;

    int r1 = gen_expr(node->lhs);
    emit(IR_UNLESS, r1, x);
    int r2 = gen_expr(node->rhs);
    emit(IR_MOV, r1, r2);
    kill(r2);
    emit(IR_UNLESS, r1, x);
    emit(IR_IMM, r1, 1);
    label(x);
    return r1;
  }
  case ND_LOGOR: {
    int x = nlabel++;
    int y = nlabel++;

    int r1 = gen_expr(node->lhs);
    emit(IR_UNLESS, r1, x);
    emit(IR_IMM, r1, 1);
    jmp(y);
    label(x);

    int r2 = gen_expr(node->rhs);
    emit(IR_MOV, r1, r2);
    kill(r2);
    emit(IR_UNLESS, r1, y);
    emit(IR_IMM, r1, 1);
    label(y);
    return r1;
  }
  case ND_VARREF:
  case ND_DOT: {
    int r = gen_lval(node);
    load(node, r, r);
    return r;
  }
  case ND_CALL: {
    int args[6];
    for (int i = 0; i < node->args->len; i++)
      args[i] = gen_expr(node->args->data[i]);

    int r = nreg++;

    IR *ir = emit(IR_CALL, r, -1);
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
    load(node, r, r);
    return r;
  }
  case ND_CAST: {
    int r = gen_expr(node->expr);
    if (node->ty->ty != BOOL)
      return r;
    int r2 = nreg++;
    emit(IR_IMM, r2, 0);
    emit(IR_NE, r, r2);
    kill(r2);
    return r;
  }
  case ND_STMT_EXPR:
    for (int i = 0; i < node->stmts->len; i++)
      gen_stmt(node->stmts->data[i]);
    return gen_expr(node->expr);
  case '=': {
    int rhs = gen_expr(node->rhs);
    int lhs = gen_lval(node->lhs);
    store(node, lhs, rhs);
    kill(lhs);
    return rhs;
  }
  case '+':
    return gen_binop(IR_ADD, node);
  case '-':
    return gen_binop(IR_SUB, node);
  case '*':
    return gen_binop(IR_MUL, node);
  case '/':
    return gen_binop(IR_DIV, node);
  case '%':
    return gen_binop(IR_MOD, node);
  case '<':
    return gen_binop(IR_LT, node);
  case ND_LE:
    return gen_binop(IR_LE, node);
  case '&':
    return gen_binop(IR_AND, node);
  case '|':
    return gen_binop(IR_OR, node);
  case '^':
    return gen_binop(IR_XOR, node);
  case ND_SHL:
    return gen_binop(IR_SHL, node);
  case ND_SHR:
    return gen_binop(IR_SHR, node);
  case '~': {
    int r = gen_expr(node->expr);
    gen_imm(IR_XOR, r, -1);
    return r;
  }
  case ',':
    kill(gen_expr(node->lhs));
    return gen_expr(node->rhs);
  case '?': {
    int x = nlabel++;
    int y = nlabel++;
    int r = gen_expr(node->cond);

    emit(IR_UNLESS, r, x);
    int r2 = gen_expr(node->then);
    emit(IR_MOV, r, r2);
    kill(r2);
    jmp(y);

    label(x);
    int r3 = gen_expr(node->els);
    emit(IR_MOV, r, r3);
    kill(r2);
    label(y);
    return r;
  }
  case '!': {
    int lhs = gen_expr(node->expr);
    int rhs = nreg++;
    emit(IR_IMM, rhs, 0);
    emit(IR_EQ, lhs, rhs);
    kill(rhs);
    return lhs;
  }
  default:
    assert(0 && "unknown AST type");
  }
}

static void gen_stmt(Node *node) {
  switch (node->op) {
  case ND_NULL:
    return;
  case ND_IF: {
    int x = nlabel++;
    int y = nlabel++;
    int r = gen_expr(node->cond);
    emit(IR_UNLESS, r, x);
    kill(r);
    gen_stmt(node->then);
    jmp(y);
    label(x);
    if (node->els)
      gen_stmt(node->els);
    label(y);
    return;
  }
  case ND_FOR: {
    int x = nlabel++;
    if (node->init)
      gen_stmt(node->init);
    label(x);
    if (node->cond) {
      int r = gen_expr(node->cond);
      emit(IR_UNLESS, r, node->break_label);
      kill(r);
    }
    gen_stmt(node->body);
    label(node->continue_label);
    if (node->inc)
      kill(gen_expr(node->inc));
    jmp(x);
    label(node->break_label);
    return;
  }
  case ND_DO_WHILE: {
    int x = nlabel++;
    label(x);
    gen_stmt(node->body);
    label(node->continue_label);
    int r = gen_expr(node->cond);
    emit(IR_IF, r, x);
    kill(r);
    label(node->break_label);
    return;
  }
  case ND_SWITCH: {
    int r = gen_expr(node->cond);
    for (int i = 0; i < node->cases->len; i++) {
      Node *case_ = node->cases->data[i];
      int r2 = nreg++;
      emit(IR_IMM, r2, case_->val);
      emit(IR_EQ, r2, r);
      emit(IR_IF, r2, case_->case_label);
      kill(r2);
    }
    kill(r);
    jmp(node->break_label);
    gen_stmt(node->body);
    label(node->break_label);
    return;
  }
  case ND_CASE:
    label(node->case_label);
    gen_stmt(node->body);
    break;
  case ND_BREAK:
    jmp(node->target->break_label);
    break;
  case ND_CONTINUE:
    jmp(node->target->continue_label);
    break;
  case ND_RETURN: {
    int r = gen_expr(node->expr);
    emit(IR_RETURN, r, -1);
    kill(r);
    return;
  }
  case ND_EXPR_STMT:
    kill(gen_expr(node->expr));
    return;
  case ND_COMP_STMT:
    for (int i = 0; i < node->stmts->len; i++)
      gen_stmt(node->stmts->data[i]);
    return;
  default:
    error("unknown node: %d", node->op);
  }
}

static void gen_param(Var *var, int i) {
  IR *ir = emit(IR_STORE_ARG, var->offset, i);
  ir->size = var->ty->size;
}

void gen_ir(Program *prog) {
  for (int i = 0; i < prog->funcs->len; i++) {
    Function *fn = prog->funcs->data[i];
    code = new_vec();

    assert(fn->node->op == ND_FUNC);

    // Assign an offset from RBP to each local variable.
    int off = 0;
    for (int i = 0; i < fn->lvars->len; i++) {
      Var *var = fn->lvars->data[i];
      off += var->ty->size;
      off = roundup(off, var->ty->align);
      var->offset = -off;
    }
    fn->stacksize = off;

    // Emit IR.
    Vector *params = fn->node->params;
    for (int i = 0; i < params->len; i++)
      gen_param(params->data[i], i);

    gen_stmt(fn->node->body);
    fn->ir = code;

    // Later passes shouldn't need the following members,
    // so make it explicit.
    fn->lvars = NULL;
    fn->node = NULL;
  }
}
