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
  bb->succ = new_vec();
  bb->pred = new_vec();
  bb->def_regs = new_vec();
  bb->in_regs = new_vec();
  bb->out_regs = new_vec();
  vec_push(fn->bbs, bb);
  return bb;
}

static IR *new_ir(int op) {
  IR *ir = calloc(1, sizeof(IR));
  ir->op = op;
  vec_push(out->ir, ir);
  return ir;
}

Reg *new_reg() {
  Reg *r = calloc(1, sizeof(Reg));
  r->vn = nreg++;
  r->rn = -1;
  return r;
}

static IR *emit(int op, Reg *r0, Reg *r1, Reg *r2) {
  IR *ir = new_ir(op);
  ir->r0 = r0;
  ir->r1 = r1;
  ir->r2 = r2;
  return ir;
}

static IR *br(Reg *r, BB *then, BB *els) {
  IR *ir = new_ir(IR_BR);
  ir->r2 = r;
  ir->bb1 = then;
  ir->bb2 = els;
  return ir;
}

static IR *jmp(BB *bb) {
  IR *ir = new_ir(IR_JMP);
  ir->bb1 = bb;
  return ir;
}

static IR *jmp_arg(BB *bb, Reg *r) {
  IR *ir = new_ir(IR_JMP);
  ir->bb1 = bb;
  ir->bbarg = r;
  return ir;
}

static Reg *imm(int imm) {
  Reg *r = new_reg();
  IR *ir = new_ir(IR_IMM);
  ir->r0 = r;
  ir->imm = imm;
  return r;
}

static Reg *gen_expr(Node *node);

static void load(Node *node, Reg *dst, Reg *src) {
  IR *ir = emit(IR_LOAD, dst, NULL, src);
  ir->size = node->ty->size;
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
static Reg *gen_lval(Node *node) {
  if (node->op == ND_DEREF)
    return gen_expr(node->expr);

  if (node->op == ND_DOT) {
    Reg *r1 = new_reg();
    Reg *r2 = gen_lval(node->expr);
    Reg *r3 = imm(node->ty->offset);
    emit(IR_ADD, r1, r2, r3);
    return r1;
  }

  assert(node->op == ND_VARREF);
  Var *var = node->var;

  IR *ir;
  if (var->is_local) {
    ir = new_ir(IR_BPREL);
    ir->r0 = new_reg();
    ir->var = var;
  } else {
    ir = new_ir(IR_LABEL_ADDR);
    ir->r0 = new_reg();
    ir->name = var->name;
  }
  return ir->r0;
}

static Reg *gen_binop(int op, Node *node) {
  Reg *r1 = new_reg();
  Reg *r2 = gen_expr(node->lhs);
  Reg *r3 = gen_expr(node->rhs);
  emit(op, r1, r2, r3);
  return r1;
}

static void gen_stmt(Node *node);

static Reg *gen_expr(Node *node) {
  switch (node->op) {
  case ND_NUM:
    return imm(node->val);
  case ND_EQ:
    return gen_binop(IR_EQ, node);
  case ND_NE:
    return gen_binop(IR_NE, node);
  case ND_LOGAND: {
    BB *bb = new_bb();
    BB *set0 = new_bb();
    BB *set1 = new_bb();
    BB *last = new_bb();

    br(gen_expr(node->lhs), bb, set0);

    out = bb;
    br(gen_expr(node->rhs), set1, set0);

    out = set0;
    jmp_arg(last, imm(0));

    out = set1;
    jmp_arg(last, imm(1));

    out = last;
    out->param = new_reg();
    return out->param;
  }
  case ND_LOGOR: {
    BB *bb = new_bb();
    BB *set0 = new_bb();
    BB *set1 = new_bb();
    BB *last = new_bb();

    Reg *r1 = gen_expr(node->lhs);
    br(r1, set1, bb);

    out = bb;
    Reg *r2 = gen_expr(node->rhs);
    br(r2, set1, set0);

    out = set0;
    jmp_arg(last, imm(0));

    out = set1;
    jmp_arg(last, imm(1));

    out = last;
    out->param = new_reg();
    return out->param;
  }
  case ND_VARREF:
  case ND_DOT: {
    Reg *r = new_reg();
    load(node, r, gen_lval(node));
    return r;
  }
  case ND_CALL: {
    Reg *args[6];
    for (int i = 0; i < node->args->len; i++)
      args[i] = gen_expr(node->args->data[i]);

    IR *ir = new_ir(IR_CALL);
    ir->r0 = new_reg();
    ir->name = node->name;
    ir->nargs = node->args->len;
    memcpy(ir->args, args, sizeof(args));
    return ir->r0;
  }
  case ND_ADDR:
    return gen_lval(node->expr);
  case ND_DEREF: {
    Reg *r = new_reg();
    load(node, r, gen_expr(node->expr));
    return r;
  }
  case ND_CAST: {
    Reg *r1 = gen_expr(node->expr);
    if (node->ty->ty != BOOL)
      return r1;
    Reg *r2 = new_reg();
    emit(IR_NE, r2, r1, imm(0));
    return r2;
  }
  case ND_STMT_EXPR:
    for (int i = 0; i < node->stmts->len; i++)
      gen_stmt(node->stmts->data[i]);
    return gen_expr(node->expr);
  case '=': {
    Reg *r1 = gen_expr(node->rhs);
    Reg *r2 = gen_lval(node->lhs);

    IR *ir = emit(IR_STORE, NULL, r2, r1);
    ir->size = node->ty->size;
    return r1;
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
    Reg *r1 = new_reg();
    Reg *r2 = gen_expr(node->expr);
    emit(IR_XOR, r1, r2, imm(-1));
    return r1;
  }
  case ',':
    gen_expr(node->lhs);
    return gen_expr(node->rhs);
  case '?': {
    BB *then = new_bb();
    BB *els = new_bb();
    BB *last = new_bb();

    br(gen_expr(node->cond), then, els);

    out = then;
    jmp_arg(last, gen_expr(node->then));

    out = els;
    jmp_arg(last, gen_expr(node->els));

    out = last;
    out->param = new_reg();
    return out->param;
  }
  case '!': {
    Reg *r1 = new_reg();
    Reg *r2 = gen_expr(node->expr);
    emit(IR_EQ, r1, r2, imm(0));
    return r1;
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

    br(gen_expr(node->cond), then, els);

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
    node->continue_ = new_bb();
    BB *body = new_bb();
    node->break_ = new_bb();

    if (node->init)
      gen_stmt(node->init);
    jmp(cond);

    out = cond;
    if (node->cond) {
      Reg *r = gen_expr(node->cond);
      br(r, body, node->break_);
    } else {
      jmp(body);
    }

    out = body;
    gen_stmt(node->body);
    jmp(node->continue_);

    out = node->continue_;
    if (node->inc)
      gen_expr(node->inc);
    jmp(cond);

    out = node->break_;
    return;
  }
  case ND_DO_WHILE: {
    node->continue_ = new_bb();
    BB *body = new_bb();
    node->break_ = new_bb();

    jmp(body);

    out = body;
    gen_stmt(node->body);
    jmp(node->continue_);

    out = node->continue_;
    Reg *r = gen_expr(node->cond);
    br(r, body, node->break_);

    out = node->break_;
    return;
  }
  case ND_SWITCH: {
    node->break_ = new_bb();
    node->continue_ = new_bb();

    Reg *r = gen_expr(node->cond);
    for (int i = 0; i < node->cases->len; i++) {
      Node *case_ = node->cases->data[i];
      case_->bb = new_bb();

      BB *next = new_bb();
      Reg *r2 = new_reg();
      emit(IR_EQ, r2, r, imm(case_->val));
      br(r2, case_->bb, next);
      out = next;
    }
    jmp(node->break_);

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
    out = new_bb();
    break;
  case ND_CONTINUE:
    jmp(node->target->continue_);
    out = new_bb();
    break;
  case ND_RETURN: {
    Reg *r = gen_expr(node->expr);
    IR *ir = new_ir(IR_RETURN);
    ir->r2 = r;
    out = new_bb();
    return;
  }
  case ND_EXPR_STMT:
    gen_expr(node->expr);
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
  IR *ir = new_ir(IR_STORE_ARG);
  ir->var = var;
  ir->imm = i;
  ir->size = var->ty->size;
  var->address_taken = true;
}

void gen_ir(Program *prog) {
  for (int i = 0; i < prog->funcs->len; i++) {
    fn = prog->funcs->data[i];

    assert(fn->node->op == ND_FUNC);

    // Add an empty entry BB to make later analysis easy.
    out = new_bb();
    BB *bb = new_bb();
    jmp(bb);
    out = bb;

    // Emit IR.
    Vector *params = fn->node->params;
    for (int i = 0; i < params->len; i++)
      gen_param(params->data[i], i);

    gen_stmt(fn->node->body);

    // Make it always ends with a return to make later analysis easy.
    new_ir(IR_RETURN)->r2 = imm(0);

    // Later passes shouldn't need the AST, so make it explicit.
    fn->node = NULL;
  }
}
