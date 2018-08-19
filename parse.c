#include "9cc.h"

static Vector *tokens;
static int pos;
static Type int_ty = {INT, NULL};

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

static bool is_typename() {
  Token *t = tokens->data[pos];
  return t->ty == TK_INT;
}

static Node *new_node(int op, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->op = op;
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
    node->ty = &int_ty;
    node->op = ND_NUM;
    node->val = t->val;
    return node;
  }

  if (t->ty == TK_IDENT) {
    node->name = t->name;

    if (!consume('(')) {
      node->op = ND_IDENT;
      return node;
    }

    node->op = ND_CALL;
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

static Node *mul();

static Node *unary() {
  if (consume('*')) {
    Node *node = calloc(1, sizeof(Node));
    node->op = ND_DEREF;
    node->expr = mul();
    return node;
  }
  return term();
}

static Node *mul() {
  Node *lhs = unary();
  for (;;) {
    Token *t = tokens->data[pos];
    if (t->ty != '*' && t->ty != '/')
      return lhs;
    pos++;
    lhs = new_node(t->ty, lhs, unary());
  }
}

static Node *add() {
  Node *lhs = mul();
  for (;;) {
    Token *t = tokens->data[pos];
    if (t->ty != '+' && t->ty != '-')
      return lhs;
    pos++;
    lhs = new_node(t->ty, lhs, mul());
  }
}

static Node *rel() {
  Node *lhs = add();
  for (;;) {
    Token *t = tokens->data[pos];
    if (t->ty == '<') {
      pos++;
      lhs = new_node('<', lhs, add());
      continue;
    }
    if (t->ty == '>') {
      pos++;
      lhs = new_node('<', add(), lhs);
      continue;
    }
    return lhs;
  }
}

static Node *logand() {
  Node *lhs = rel();
  for (;;) {
    Token *t = tokens->data[pos];
    if (t->ty != TK_LOGAND)
      return lhs;
    pos++;
    lhs = new_node(ND_LOGAND, lhs, rel());
  }
}

static Node *logor() {
  Node *lhs = logand();
  for (;;) {
    Token *t = tokens->data[pos];
    if (t->ty != TK_LOGOR)
      return lhs;
    pos++;
    lhs = new_node(ND_LOGOR, lhs, logand());
  }
}

static Node *assign() {
  Node *lhs = logor();
  if (consume('='))
    return new_node('=', lhs, logor());
  return lhs;
}

static Type *type() {
  Token *t = tokens->data[pos];
  if (t->ty != TK_INT)
    error("typename expected, but got %s", t->input);
  pos++;

  Type *ty = &int_ty;
  while (consume('*'))
    ty = ptr_of(ty);
  return ty;
}

static Node *decl() {
  Node *node = calloc(1, sizeof(Node));
  node->op = ND_VARDEF;

  // Read the first half of type name (e.g. `int *`).
  node->ty = type();

  // Read an identifier.
  Token *t = tokens->data[pos];
  if (t->ty != TK_IDENT)
    error("variable name expected, but got %s", t->input);
  node->name = t->name;
  pos++;

  // Read the second half of type name (e.g. `[3][5]`).
  Vector *ary_size = new_vec();
  while (consume('[')) {
    Node *len = term();
    if (len->op != ND_NUM)
      error("number expected");
    vec_push(ary_size, len);
    expect(']');
  }
  for (int i = ary_size->len - 1; i >= 0; i--) {
    Node *len = ary_size->data[i];
    node->ty = ary_of(node->ty, len->val);
  }

  // Read an initializer.
  if (consume('='))
    node->init = assign();
  expect(';');
  return node;
}

static Node *param() {
  Node *node = calloc(1, sizeof(Node));
  node->op = ND_VARDEF;
  node->ty = type();

  Token *t = tokens->data[pos];
  if (t->ty != TK_IDENT)
    error("parameter name expected, but got %s", t->input);
  node->name = t->name;
  pos++;
  return node;
}

static Node *expr_stmt() {
  Node *node = calloc(1, sizeof(Node));
  node->op = ND_EXPR_STMT;
  node->expr = assign();
  expect(';');
  return node;
}

static Node *stmt() {
  Node *node = calloc(1, sizeof(Node));
  Token *t = tokens->data[pos];

  switch (t->ty) {
  case TK_INT:
    return decl();
  case TK_IF:
    pos++;
    node->op = ND_IF;
    expect('(');
    node->cond = assign();
    expect(')');

    node->then = stmt();

    if (consume(TK_ELSE))
      node->els = stmt();
    return node;
  case TK_FOR:
    pos++;
    node->op = ND_FOR;
    expect('(');
    if (is_typename())
      node->init = decl();
    else
      node->init = expr_stmt();
    node->cond = assign();
    expect(';');
    node->inc = assign();
    expect(')');
    node->body = stmt();
    return node;
  case TK_RETURN:
    pos++;
    node->op = ND_RETURN;
    node->expr = assign();
    expect(';');
    return node;
  case '{':
    pos++;
    node->op = ND_COMP_STMT;
    node->stmts = new_vec();
    while (!consume('}'))
      vec_push(node->stmts, stmt());
    return node;
  default:
    return expr_stmt();
  }
}

static Node *compound_stmt() {
  Node *node = calloc(1, sizeof(Node));
  node->op = ND_COMP_STMT;
  node->stmts = new_vec();

  while (!consume('}'))
    vec_push(node->stmts, stmt());
  return node;
}

static Node *function() {
  Node *node = calloc(1, sizeof(Node));
  node->op = ND_FUNC;
  node->args = new_vec();

  Token *t = tokens->data[pos];
  if (t->ty != TK_INT)
    error("function return type expected, but got %s", t->input);
  pos++;

  t = tokens->data[pos];
  if (t->ty != TK_IDENT)
    error("function name expected, but got %s", t->input);
  node->name = t->name;
  pos++;

  expect('(');
  if (!consume(')')) {
    vec_push(node->args, param());
    while (consume(','))
      vec_push(node->args, param());
    expect(')');
  }

  expect('{');
  node->body = compound_stmt();
  return node;
};

Vector *parse(Vector *tokens_) {
  tokens = tokens_;
  pos = 0;

  Vector *v = new_vec();
  while (((Token *)tokens->data[pos])->ty != TK_EOF)
    vec_push(v, function());
  return v;
}
