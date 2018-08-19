#include "9cc.h"

// Compile AST to intermediate code that has infinite number of registers.
// Base pointer is always assigned to r0.

IRInfo irinfo[] = {
  {IR_ADD, "ADD", IR_TY_REG_REG},
  {IR_SUB, "SUB", IR_TY_REG_REG},
  {IR_MUL, "MUL", IR_TY_REG_REG},
  {IR_DIV, "DIV", IR_TY_REG_REG},
  {IR_IMM, "MOV", IR_TY_REG_IMM},
  {IR_SUB_IMM, "SUB", IR_TY_REG_IMM},
  {IR_MOV, "MOV", IR_TY_REG_REG},
  {IR_LABEL, "", IR_TY_LABEL},
  {IR_JMP, "JMP", IR_TY_LABEL},
  {IR_UNLESS, "UNLESS", IR_TY_REG_LABEL},
  {IR_CALL, "CALL", IR_TY_CALL},
  {IR_RETURN, "RET", IR_TY_REG},
  {IR_LOAD, "LOAD", IR_TY_REG_REG},
  {IR_STORE, "STORE", IR_TY_REG_REG},
  {IR_KILL, "KILL", IR_TY_REG},
  {IR_SAVE_ARGS, "SAVE_ARGS", IR_TY_IMM},
  {IR_NOP, "NOP", IR_TY_NOARG},
  {0, NULL, 0},
};

static Vector *code;
static Map *vars;
static int regno;
static int stacksize;
static int label;

IRInfo *get_irinfo(IR *ir) {
  for (IRInfo *info = irinfo; info->name; info++)
    if (info->op == ir->op)
      return info;
  assert(0 && "invalid instruction");
}

static char *tostr(IR *ir) {
  IRInfo *info = get_irinfo(ir);

  switch (info->ty) {
  case IR_TY_LABEL:
    return format(".L%d:\n", ir->lhs);
  case IR_TY_IMM:
    return format("%s %d\n", info->name, ir->lhs);
  case IR_TY_REG:
    return format("%s r%d\n", info->name, ir->lhs);
  case IR_TY_REG_REG:
    return format("%s r%d, r%d\n", info->name, ir->lhs, ir->rhs);
  case IR_TY_REG_IMM:
    return format("%s r%d, %d\n", info->name, ir->lhs, ir->rhs);
  case IR_TY_REG_LABEL:
    return format("%s r%d, .L%d\n", info->name, ir->lhs, ir->rhs);
  case IR_TY_CALL: {
    StringBuilder *sb = new_sb();
    sb_append(sb, format("r%d = %s(", ir->lhs, ir->name));
    for (int i = 0; i < ir->nargs; i++)
      sb_append(sb, format(", r%d", ir->args));
    sb_append(sb, ")\n");
    return sb_get(sb);
  }
  default:
    assert(info->ty == IR_TY_NOARG);
    return format("%s\n", info->name);
  }
}

void dump_ir(Vector *irv) {
  for (int i = 0; i < irv->len; i++) {
    Function *fn = irv->data[i];
    fprintf(stderr, "%s():\n", fn->name);
    for (int j = 0; j < fn->ir->len; j++)
      fprintf(stderr, "  %s", tostr(fn->ir->data[j]));
  }
}

static IR *add(int op, int lhs, int rhs) {
  IR *ir = calloc(1, sizeof(IR));
  ir->op = op;
  ir->lhs = lhs;
  ir->rhs = rhs;
  vec_push(code, ir);
  return ir;
}

static int gen_lval(Node *node) {
  if (node->ty != ND_IDENT)
    error("not an lvalue");

  if (!map_exists(vars, node->name)) {
    stacksize += 8;
    map_put(vars, node->name, (void *)(intptr_t)stacksize);
  }

  int r = regno++;
  int off = (intptr_t)map_get(vars, node->name);
  add(IR_MOV, r, 0);
  add(IR_SUB_IMM, r, off);
  return r;
}

static int gen_expr(Node *node) {
  if (node->ty == ND_NUM) {
    int r = regno++;
    add(IR_IMM, r, node->val);
    return r;
  }

  if (node->ty == ND_IDENT) {
    int r = gen_lval(node);
    add(IR_LOAD, r, r);
    return r;
  }

  if (node->ty == ND_CALL) {
    int args[6];
    for (int i = 0; i < node->args->len; i++)
      args[i] = gen_expr(node->args->data[i]);

    int r = regno++;

    IR *ir = add(IR_CALL, r, -1);
    ir->name = node->name;
    ir->nargs = node->args->len;
    memcpy(ir->args, args, sizeof(args));

    for (int i = 0; i < ir->nargs; i++)
      add(IR_KILL, ir->args[i], -1);
    return r;
  }

  if (node->ty == '=') {
    int rhs = gen_expr(node->rhs);
    int lhs = gen_lval(node->lhs);
    add(IR_STORE, lhs, rhs);
    add(IR_KILL, rhs, -1);
    return lhs;
  }

  assert(strchr("+-*/", node->ty));

  int ty;
  if (node->ty == '+')
    ty = IR_ADD;
  else if (node->ty == '-')
    ty = IR_SUB;
  else if (node->ty == '*')
    ty = IR_MUL;
  else
    ty = IR_DIV;

  int lhs = gen_expr(node->lhs);
  int rhs = gen_expr(node->rhs);
  add(ty, lhs, rhs);
  add(IR_KILL, rhs, -1);
  return lhs;
}

static void gen_stmt(Node *node) {
  if (node->ty == ND_IF) {
    int r = gen_expr(node->cond);
    int x = label++;

    add(IR_UNLESS, r, x);
    add(IR_KILL, r, -1);

    gen_stmt(node->then);

    if (!node->els) {
      add(IR_LABEL, x, -1);
      return;
    }

    int y = label++;
    add(IR_JMP, y, -1);
    add(IR_LABEL, x, -1);
    gen_stmt(node->els);
    add(IR_LABEL, y, -1);
    return;
  }

  if (node->ty == ND_RETURN) {
    int r = gen_expr(node->expr);
    add(IR_RETURN, r, -1);
    add(IR_KILL, r, -1);
    return;
  }

  if (node->ty == ND_EXPR_STMT) {
    int r = gen_expr(node->expr);
    add(IR_KILL, r, -1);
    return;
  }

  if (node->ty == ND_COMP_STMT) {
    for (int i = 0; i < node->stmts->len; i++)
      gen_stmt(node->stmts->data[i]);
    return;
  }

  error("unknown node: %d", node->ty);
}

static void gen_args(Vector *nodes) {
  if (nodes->len == 0)
    return;

  add(IR_SAVE_ARGS, nodes->len, -1);

  for (int i = 0; i < nodes->len; i++) {
    Node *node = nodes->data[i];
    if (node->ty != ND_IDENT)
      error("bad parameter");

    stacksize += 8;
    map_put(vars, node->name, (void *)(intptr_t)stacksize);
  }
}

Vector *gen_ir(Vector *nodes) {
  Vector *v = new_vec();

  for (int i = 0; i < nodes->len; i++) {
    Node *node = nodes->data[i];
    assert(node->ty == ND_FUNC);

    code = new_vec();
    vars = new_map();
    regno = 1;
    stacksize = 0;

    gen_args(node->args);
    gen_stmt(node->body);

    Function *fn = malloc(sizeof(Function));
    fn->name = node->name;
    fn->stacksize = stacksize;
    fn->ir = code;
    vec_push(v, fn);
  }
  return v;
}
