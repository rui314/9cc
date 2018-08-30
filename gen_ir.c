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

static Function *fn;
static BB *out;
static int nreg = 1;

static BB *new_bb() {
  BB *bb = calloc(1, sizeof(BB));
  bb->label = nlabel++;
  bb->ir = new_vec();
  vec_push(fn->bbs, bb);
  return bb;
}

static IR *new_ir(int op) {
  IR *ir = calloc(1, sizeof(IR));
  ir->op = op;
  ir->kill = new_vec();
  vec_push(out->ir, ir);
  return ir;
}

static IR *emit(int op, int lhs, int rhs) {
  IR *ir = new_ir(op);
  ir->lhs = lhs;
  ir->rhs = rhs;
  return ir;
}

static IR *br(int r, BB *then, BB *els) {
  IR *ir = new_ir(IR_BR);
  ir->lhs = r;
  ir->bb1 = then;
  ir->bb2 = els;
  return ir;
}

static void kill(int r) {
  IR *ir = vec_last(out->ir);
  vec_pushi(ir->kill, r);
}

static void jmp(BB *bb) {
  IR *ir = new_ir(IR_JMP);
  ir->bb1 = bb;
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
    BB *bb1 = new_bb();
    BB *bb2 = new_bb();
    BB *last = new_bb();

    int r = gen_expr(node->lhs);
    br(r, bb1, last);

    out = bb1;
    int r2 = gen_expr(node->rhs);
    emit(IR_MOV, r, r2);
    kill(r2);
    br(r, bb2, last);

    out = bb2;
    emit(IR_IMM, r, 1);
    jmp(last);

    out = last;
    return r;
  }
  case ND_LOGOR: {
    BB *bb = new_bb();
    BB *set0 = new_bb();
    BB *set1 = new_bb();
    BB *last = new_bb();

    int r = gen_expr(node->lhs);
    br(r, set1, bb);

    out = set0;
    emit(IR_IMM, r, 0);
    jmp(last);

    out = set1;
    emit(IR_IMM, r, 1);
    jmp(last);

    out = bb;
    int r2 = gen_expr(node->rhs);
    emit(IR_MOV, r, r2);
    kill(r2);
    br(r, set1, set0);

    out = last;
    return r;
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
    BB *then = new_bb();
    BB *els = new_bb();
    BB *last = new_bb();

    int r = gen_expr(node->cond);
    br(r, then, els);

    out = then;
    int r2 = gen_expr(node->then);
    emit(IR_MOV, r, r2);
    kill(r2);
    jmp(last);

    out = els;
    int r3 = gen_expr(node->els);
    emit(IR_MOV, r, r3);
    kill(r2);
    jmp(last);

    out = last;
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
    BB *then = new_bb();
    BB *els = new_bb();
    BB *last = new_bb();

    int r = gen_expr(node->cond);
    br(r, then, els);
    kill(r);

    out = then;
    gen_stmt(node->then);
    jmp(last);

    out = els;
    if (node->els)
      gen_stmt(node->els);
    jmp(last);

    out = last;
    return;
  }
  case ND_FOR: {
    BB *cond = new_bb();
    BB *body = new_bb();
    node->break_ = new_bb();
    node->continue_ = new_bb();

    if (node->init)
      gen_stmt(node->init);
    jmp(cond);

    out = cond;
    if (node->cond) {
      int r = gen_expr(node->cond);
      br(r, body, node->break_);
      kill(r);
    } else {
      jmp(body);
    }

    out = body;
    gen_stmt(node->body);
    jmp(node->continue_);

    out = node->continue_;
    if (node->inc)
      kill(gen_expr(node->inc));
    jmp(cond);

    out = node->break_;
    return;
  }
  case ND_DO_WHILE: {
    BB *body = new_bb();
    node->continue_ = new_bb();
    node->break_ = new_bb();

    jmp(body);

    out = body;
    gen_stmt(node->body);
    jmp(node->continue_);

    out = node->continue_;
    int r = gen_expr(node->cond);
    br(r, body, node->break_);
    kill(r);

    out = node->break_;
    return;
  }
  case ND_SWITCH: {
    node->break_ = new_bb();
    node->continue_ = new_bb();

    int r = gen_expr(node->cond);
    for (int i = 0; i < node->cases->len; i++) {
      Node *case_ = node->cases->data[i];
      case_->bb = new_bb();

      BB *next = new_bb();
      int r2 = nreg++;

      emit(IR_IMM, r2, case_->val);
      emit(IR_EQ, r2, r);
      br(r2, case_->bb, next);
      kill(r2);
      out = next;
    }
    jmp(node->break_);
    kill(r);

    gen_stmt(node->body);
    jmp(node->break_);

    out = node->break_;
    return;
  }
  case ND_CASE:
    jmp(node->bb);
    out = node->bb;
    gen_stmt(node->body);
    break;
  case ND_BREAK:
    jmp(node->target->break_);
    break;
  case ND_CONTINUE:
    jmp(node->target->continue_);
    break;
  case ND_RETURN: {
    int r = gen_expr(node->expr);
    emit(IR_RETURN, r, -1);
    kill(r);

    BB *bb = new_bb();
    jmp(bb);
    out = bb;
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
    fn = prog->funcs->data[i];
    out = new_bb();

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

    // Later passes shouldn't need the following members,
    // so make it explicit.
    fn->lvars = NULL;
    fn->node = NULL;
  }
}
