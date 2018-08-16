#include "9cc.h"

static Vector *tokens;
static int pos;

static Node *new_node(int op, Node *lhs, Node *rhs) {
  Node *node = malloc(sizeof(Node));
  node->ty = op;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *new_node_num(int val) {
  Node *node = malloc(sizeof(Node));
  node->ty = ND_NUM;
  node->val = val;
  return node;
}

static Node *number() {
  Token *t = tokens->data[pos];
  if (t->ty != TK_NUM)
    error("number expected, but got %s", t->input);
  pos++;
  return new_node_num(t->val);
}

static Node *expr() {
  Node *lhs = number();
  for (;;) {
    Token *t = tokens->data[pos];
    int op = t->ty;
    if (op != '+' && op != '-')
      break;
    pos++;
    lhs = new_node(op, lhs, number());
  }

  Token *t = tokens->data[pos];
  if (t->ty != TK_EOF)
    error("stray token: %s", t->input);
  return lhs;
}

Node *parse(Vector *v) {
  tokens = v;
  pos = 0;
  return expr();
}
