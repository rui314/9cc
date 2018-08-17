#include "9cc.h"

static Vector *code;

static IR *add(int op, int lhs, int rhs) {
  IR *ir = malloc(sizeof(IR));
  ir->op = op;
  ir->lhs = lhs;
  ir->rhs = rhs;
  vec_push(code, ir);
  return ir;
}

static int gen_expr(Node *node) {
  static int regno;

  if (node->ty == ND_NUM) {
    int r = regno++;
    add(IR_IMM, r, node->val);
    return r;
  }

  assert(strchr("+-*/", node->ty));

  int lhs = gen_expr(node->lhs);
  int rhs = gen_expr(node->rhs);

  add(node->ty, lhs, rhs);
  add(IR_KILL, rhs, 0);
  return lhs;
}

static void gen_stmt(Node *node) {
  if (node->ty == ND_RETURN) {
    int r = gen_expr(node->expr);
    add(IR_RETURN, r, 0);
    add(IR_KILL, r, 0);
    return;
  }

  if (node->ty == ND_EXPR_STMT) {
    int r = gen_expr(node->expr);
    add(IR_KILL, r, 0);
    return;
  }

  if (node->ty == ND_COMP_STMT) {
    for (int i = 0; i < node->stmts->len; i++)
      gen_stmt(node->stmts->data[i]);
    return;
  }

  error("unknown node: %d", node->ty);
}

Vector *gen_ir(Node *node) {
  assert(node->ty == ND_COMP_STMT);
  code = new_vec();
  gen_stmt(node);
  return code;
}
