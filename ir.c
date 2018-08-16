#include "9cc.h"

static IR *new_ir(int op, int lhs, int rhs) {
  IR *ir = malloc(sizeof(IR));
  ir->op = op;
  ir->lhs = lhs;
  ir->rhs = rhs;
  return ir;
}

static int gen(Vector *v, Node *node) {
  static int regno;

  if (node->ty == ND_NUM) {
    int r = regno++;
    vec_push(v, new_ir(IR_IMM, r, node->val));
    return r;
  }

  assert(strchr("+-*", node->ty));

  int lhs = gen(v, node->lhs);
  int rhs = gen(v, node->rhs);

  vec_push(v, new_ir(node->ty, lhs, rhs));
  vec_push(v, new_ir(IR_KILL, rhs, 0));
  return lhs;
}

Vector *gen_ir(Node *node) {
  Vector *v = new_vec();
  int r = gen(v, node);
  vec_push(v, new_ir(IR_RETURN, r, 0));
  return v;
}
