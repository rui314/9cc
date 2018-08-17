#include "9cc.h"

static Vector *tokens;
static int pos;

static Node *assign();

static void expect(int ty) {
  Token *t = tokens->data[pos];
  if (t->ty != ty)
    error("%c (%d) expected, but got %c (%d)", ty, ty, t->ty, t->ty);
  pos++;
}

static bool consume(int ty) {
  Token *t = tokens->data[pos];
  if (t->ty != ty)
    return false;
  pos++;
  return true;
}

static Node *new_node(int op, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->ty = op;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *term() {
  Token *t = tokens->data[pos++];

  if (t->ty == '(') {
    Node *node = assign();
    expect(')');
    return node;
  }

  Node *node = calloc(1, sizeof(Node));

  if (t->ty == TK_NUM) {
    node->ty = ND_NUM;
    node->val = t->val;
    return node;
  }

  if (t->ty == TK_IDENT) {
    node->name = t->name;

    if (!consume('(')) {
      node->ty = ND_IDENT;
      return node;
    }

    node->ty = ND_CALL;
    node->args = new_vec();
    if (consume(')'))
      return node;

    vec_push(node->args, assign());
    while (consume(','))
      vec_push(node->args, assign());
    expect(')');
    return node;
  }

  error("number expected, but got %s", t->input);
}

static Node *mul() {
  Node *lhs = term();
  for (;;) {
    Token *t = tokens->data[pos];
    int op = t->ty;
    if (op != '*' && op != '/')
      return lhs;
    pos++;
    lhs = new_node(op, lhs, term());
  }
}

static Node *expr() {
  Node *lhs = mul();
  for (;;) {
    Token *t = tokens->data[pos];
    int op = t->ty;
    if (op != '+' && op != '-')
      return lhs;
    pos++;
    lhs = new_node(op, lhs, mul());
  }
}

static Node *assign() {
  Node *lhs = expr();
  if (consume('='))
    return new_node('=', lhs, expr());
  return lhs;
}

static Node *stmt() {
  Node *node = calloc(1, sizeof(Node));
  Token *t = tokens->data[pos];

  switch (t->ty) {
  case TK_IF:
    pos++;
    node->ty = ND_IF;
    expect('(');
    node->cond = assign();
    expect(')');

    node->then = stmt();

    if (consume(TK_ELSE))
      node->els = stmt();
    return node;
  case TK_RETURN:
    pos++;
    node->ty = ND_RETURN;
    node->expr = assign();
    expect(';');
    return node;
  default:
    node->ty = ND_EXPR_STMT;
    node->expr = assign();
    expect(';');
    return node;
  }
}

static Node *compound_stmt() {
  Node *node = calloc(1, sizeof(Node));
  node->ty = ND_COMP_STMT;
  node->stmts = new_vec();

  for (;;) {
    Token *t = tokens->data[pos];
    if (t->ty == TK_EOF)
      return node;
    vec_push(node->stmts, stmt());
  }
}

Node *parse(Vector *v) {
  tokens = v;
  pos = 0;
  return compound_stmt();
}
