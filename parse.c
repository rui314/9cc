#include "9cc.h"

// This is a recursive-descendent parser which constructs abstract
// syntax tree from input tokens.
//
// This parser knows only about BNF of the C grammer and doesn't care
// about its semantics. Therefore, some invalid expressions, such as
// `1+2=3`, are accepted by this parser, but that's intentional.
// Semantic errors are detected in a later pass.

typedef struct Env {
  Map *tags;
  struct Env *next;
} Env;

static Vector *tokens;
static int pos;
struct Env *env;
static Node null_stmt = {ND_NULL};

static Env *new_env(Env *next) {
  Env *env = calloc(1, sizeof(Env));
  env->tags = new_map();
  env->next = next;
  return env;
}

static Node *assign();

static void expect(int ty) {
  Token *t = tokens->data[pos];
  if (t->ty != ty)
    error("%c (%d) expected, but got %c (%d)", ty, ty, t->ty, t->ty);
  pos++;
}

static Type *new_prim_ty(int ty, int size) {
  Type *ret = calloc(1, sizeof(Type));
  ret->ty = ty;
  ret->size = size;
  ret->align = size;
  return ret;
}

static Type *char_ty() { return new_prim_ty(CHAR, 1); }
static Type *int_ty() { return new_prim_ty(INT, 4); }

static bool consume(int ty) {
  Token *t = tokens->data[pos];
  if (t->ty != ty)
    return false;
  pos++;
  return true;
}

static bool is_typename() {
  Token *t = tokens->data[pos];
  return t->ty == TK_INT || t->ty == TK_CHAR || t->ty == TK_STRUCT;
}

static Node *decl();

static Type *read_type() {
  Token *t = tokens->data[pos];

  if (t->ty == TK_INT) {
    pos++;
    return int_ty();
  }

  if (t->ty == TK_CHAR) {
    pos++;
    return char_ty();
  }

  if (t->ty == TK_STRUCT) {
    pos++;

    char *tag = NULL;
    Token *t = tokens->data[pos];
    if (t->ty == TK_IDENT) {
      pos++;
      tag = t->name;
    }

    Vector *members = NULL;
    if (consume('{')) {
      members = new_vec();
      while (!consume('}'))
        vec_push(members, decl());
    }

    if (!tag && !members)
      error("bad struct definition");

    if (tag && members) {
      map_put(env->tags, tag, members);
    } else if (tag && !members) {
      members = map_get(env->tags, tag);
      if (!members)
        error("incomplete type: %s", tag);
    }

    return struct_of(members);
  }

  return NULL;
}

static Node *new_binop(int op, Node *lhs, Node *rhs) {
  Node *node = calloc(1, sizeof(Node));
  node->op = op;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *new_expr(int op, Node *expr) {
  Node *node = calloc(1, sizeof(Node));
  node->op = op;
  node->expr = expr;
  return node;
}

static Node *compound_stmt();

static char *ident() {
  Token *t = tokens->data[pos++];
  if (t->ty != TK_IDENT)
    error("identifier expected, but got %s", t->input);
  return t->name;
}

static Node *primary() {
  Token *t = tokens->data[pos++];

  if (t->ty == '(') {
    if (consume('{')) {
      Node *node = calloc(1, sizeof(Node));
      node->op = ND_STMT_EXPR;
      node->body = compound_stmt();
      expect(')');
      return node;
    }
    Node *node = assign();
    expect(')');
    return node;
  }

  Node *node = calloc(1, sizeof(Node));

  if (t->ty == TK_NUM) {
    node->ty = int_ty();
    node->op = ND_NUM;
    node->val = t->val;
    return node;
  }

  if (t->ty == TK_STR) {
    node->ty = ary_of(char_ty(), strlen(t->str));
    node->op = ND_STR;
    node->data = t->str;
    node->len = t->len;
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

static Node *postfix() {
  Node *lhs = primary();

  for (;;) {
    if (consume('.')) {
      Node *node = calloc(1, sizeof(Node));
      node->op = ND_DOT;
      node->expr = lhs;
      node->name = ident();
      lhs = node;
      continue;
    }

    if (consume(TK_ARROW)) {
      Node *node = calloc(1, sizeof(Node));
      node->op = ND_DOT;
      node->expr = new_expr(ND_DEREF, lhs);
      node->name = ident();
      lhs = node;
      continue;
    }

    if (consume('[')) {
      lhs = new_expr(ND_DEREF, new_binop('+', lhs, assign()));
      expect(']');
      continue;
    }
    return lhs;
  }
}

static Node *unary() {
  if (consume('*'))
    return new_expr(ND_DEREF, mul());
  if (consume('&'))
    return new_expr(ND_ADDR, mul());
  if (consume(TK_SIZEOF))
    return new_expr(ND_SIZEOF, unary());
  if (consume(TK_ALIGNOF))
    return new_expr(ND_ALIGNOF, unary());
  return postfix();
}

static Node *mul() {
  Node *lhs = unary();
  for (;;) {
    Token *t = tokens->data[pos];
    if (t->ty != '*' && t->ty != '/')
      return lhs;
    pos++;
    lhs = new_binop(t->ty, lhs, unary());
  }
}

static Node *add() {
  Node *lhs = mul();
  for (;;) {
    Token *t = tokens->data[pos];
    if (t->ty != '+' && t->ty != '-')
      return lhs;
    pos++;
    lhs = new_binop(t->ty, lhs, mul());
  }
}

static Node *rel() {
  Node *lhs = add();
  for (;;) {
    Token *t = tokens->data[pos];
    if (t->ty == '<') {
      pos++;
      lhs = new_binop('<', lhs, add());
      continue;
    }
    if (t->ty == '>') {
      pos++;
      lhs = new_binop('<', add(), lhs);
      continue;
    }
    return lhs;
  }
}

static Node *equality() {
  Node *lhs = rel();
  for (;;) {
    Token *t = tokens->data[pos];
    if (t->ty == TK_EQ) {
      pos++;
      lhs = new_binop(ND_EQ, lhs, rel());
      continue;
    }
    if (t->ty == TK_NE) {
      pos++;
      lhs = new_binop(ND_NE, lhs, rel());
      continue;
    }
    return lhs;
  }
}

static Node *logand() {
  Node *lhs = equality();
  for (;;) {
    Token *t = tokens->data[pos];
    if (t->ty != TK_LOGAND)
      return lhs;
    pos++;
    lhs = new_binop(ND_LOGAND, lhs, equality());
  }
}

static Node *logor() {
  Node *lhs = logand();
  for (;;) {
    Token *t = tokens->data[pos];
    if (t->ty != TK_LOGOR)
      return lhs;
    pos++;
    lhs = new_binop(ND_LOGOR, lhs, logand());
  }
}

static Node *assign() {
  Node *lhs = logor();
  if (consume('='))
    return new_binop('=', lhs, logor());
  return lhs;
}

static Type *type() {
  Token *t = tokens->data[pos];
  Type *ty = read_type();
  if (!ty)
    error("typename expected, but got %s", t->input);

  while (consume('*'))
    ty = ptr_to(ty);
  return ty;
}

static Type *read_array(Type *ty) {
  Vector *v = new_vec();
  while (consume('[')) {
    Node *len = primary();
    if (len->op != ND_NUM)
      error("number expected");
    vec_push(v, len);
    expect(']');
  }
  for (int i = v->len - 1; i >= 0; i--) {
    Node *len = v->data[i];
    ty = ary_of(ty, len->val);
  }
  return ty;
}

static Node *decl() {
  Node *node = calloc(1, sizeof(Node));
  node->op = ND_VARDEF;

  // Read the first half of type name (e.g. `int *`).
  node->ty = type();

  // Read an identifier.
  node->name = ident();

  // Read the second half of type name (e.g. `[3][5]`).
  node->ty = read_array(node->ty);

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
  node->name = ident();
  return node;
}

static Node *expr_stmt() {
  Node *node = new_expr(ND_EXPR_STMT, assign());
  expect(';');
  return node;
}

static Node *stmt() {
  Node *node = calloc(1, sizeof(Node));
  Token *t = tokens->data[pos];

  switch (t->ty) {
  case TK_INT:
  case TK_CHAR:
  case TK_STRUCT:
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
    node->inc = new_expr(ND_EXPR_STMT, assign());
    expect(')');
    node->body = stmt();
    return node;
  case TK_WHILE:
    pos++;
    node->op = ND_FOR;
    node->init = &null_stmt;
    node->inc = &null_stmt;
    expect('(');
    node->cond = assign();
    expect(')');
    node->body = stmt();
    return node;
  case TK_DO:
    pos++;
    node->op = ND_DO_WHILE;
    node->body = stmt();
    expect(TK_WHILE);
    expect('(');
    node->cond = assign();
    expect(')');
    expect(';');
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
  case ';':
    pos++;
    return &null_stmt;
  default:
    return expr_stmt();
  }
}

static Node *compound_stmt() {
  Node *node = calloc(1, sizeof(Node));
  node->op = ND_COMP_STMT;
  node->stmts = new_vec();

  env = new_env(env);
  while (!consume('}'))
    vec_push(node->stmts, stmt());
  env = env->next;
  return node;
}

static Node *toplevel() {
  bool is_extern = consume(TK_EXTERN);
  Type *ty = type();
  if (!ty) {
    Token *t = tokens->data[pos];
    error("typename expected, but got %s", t->input);
  }

  char *name = ident();

  // Function
  if (consume('(')) {
    Node *node = calloc(1, sizeof(Node));
    node->op = ND_FUNC;
    node->ty = ty;
    node->name = name;
    node->args = new_vec();

    if (!consume(')')) {
      vec_push(node->args, param());
      while (consume(','))
        vec_push(node->args, param());
      expect(')');
    }

    expect('{');
    node->body = compound_stmt();
    return node;
  }

  // Global variable
  Node *node = calloc(1, sizeof(Node));
  node->op = ND_VARDEF;
  node->ty = read_array(ty);
  node->name = name;
  if (is_extern) {
    node->is_extern = true;
  } else {
    node->data = calloc(1, node->ty->size);
    node->len = node->ty->size;
  }
  expect(';');
  return node;
};

Vector *parse(Vector *tokens_) {
  tokens = tokens_;
  pos = 0;
  env = new_env(env);

  Vector *v = new_vec();
  while (((Token *)tokens->data[pos])->ty != TK_EOF)
    vec_push(v, toplevel());
  return v;
}
