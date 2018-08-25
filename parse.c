#include "9cc.h"

// This is a recursive-descendent parser which constructs abstract
// syntax tree from input tokens.
//
// This parser knows only about BNF of the C grammer and doesn't care
// about its semantics. Therefore, some invalid expressions, such as
// `1+2=3`, are accepted by this parser, but that's intentional.
// Semantic errors are detected in a later pass.

typedef struct Env {
  Map *vars;
  Map *typedefs;
  Map *tags;
  struct Env *next;
} Env;

static Program *prog;
static Vector *lvars;

static Vector *tokens;
static int pos;
struct Env *env;

static int label = 1;
static Node null_stmt = {ND_NULL};
static Node break_stmt = {ND_BREAK};

static Env *new_env(Env *next) {
  Env *env = calloc(1, sizeof(Env));
  env->vars = new_map();
  env->typedefs = new_map();
  env->tags = new_map();
  env->next = next;
  return env;
}

static Var *find_var(char *name) {
  for (Env *e = env; e; e = e->next) {
    Var *var = map_get(e->vars, name);
    if (var)
      return var;
  }
  return NULL;
}

static Type *find_typedef(char *name) {
  for (Env *e = env; e; e = e->next) {
    Type *ty = map_get(e->typedefs, name);
    if (ty)
      return ty;
  }
  return NULL;
}

static Type *find_tag(char *name) {
  for (Env *e = env; e; e = e->next) {
    Type *ty = map_get(e->tags, name);
    if (ty)
      return ty;
  }
  return NULL;
}

static Var *add_lvar(Type *ty, char *name) {
  Var *var = calloc(1, sizeof(Var));
  var->ty = ty;
  var->is_local = true;
  vec_push(lvars, var);
  map_put(env->vars, name, var);
  return var;
}

static Var *add_gvar(Type *ty, char *name, char *data, int len) {
  Var *var = calloc(1, sizeof(Var));
  var->ty = ty;
  var->is_local = false;
  var->name = name;
  var->data = data;
  var->len = len;
  vec_push(prog->gvars, var);
  map_put(env->vars, name, var);
  return var;
}

static Node *assign();
static Node *expr();

static void expect(int ty) {
  Token *t = tokens->data[pos];
  if (t->ty == ty) {
    pos++;
    return;
  }

  if (isprint(ty))
    bad_token(t, format("%c expected", ty));
  assert(ty == TK_WHILE);
  bad_token(t, "'while' expected");
}

static Type *new_prim_ty(int ty, int size) {
  Type *ret = calloc(1, sizeof(Type));
  ret->ty = ty;
  ret->size = size;
  ret->align = size;
  return ret;
}

static Type *void_ty() {
  return new_prim_ty(VOID, 0);
}

static Type *char_ty() {
  return new_prim_ty(CHAR, 1);
}

static Type *int_ty() {
  return new_prim_ty(INT, 4);
}

static Type *func_ty(Type *base) {
  Type *ty = calloc(1, sizeof(Type));
  ty->returning = base;
  return ty;
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
  if (t->ty == TK_IDENT)
    return find_typedef(t->name);
  return t->ty == TK_INT || t->ty == TK_CHAR || t->ty == TK_VOID ||
         t->ty == TK_STRUCT || t->ty == TK_TYPEOF;
}

static Node *declaration(bool define);

static void add_members(Type *ty, Vector *members) {
  int off = 0;
  for (int i = 0; i < members->len; i++) {
    Node *node = members->data[i];
    assert(node->op == ND_VARDEF);

    Type *t = node->ty;
    off = roundup(off, t->align);
    t->offset = off;
    off += t->size;

    if (ty->align < node->ty->align)
      ty->align = node->ty->align;
  }

  ty->members = members;
  ty->size = roundup(off, ty->align);
}

static Type *decl_specifiers() {
  Token *t = tokens->data[pos++];

  if (t->ty == TK_IDENT) {
    Type *ty = find_typedef(t->name);
    if (!ty)
      pos--;
    return ty;
  }

  if (t->ty == TK_INT)
    return int_ty();

  if (t->ty == TK_CHAR)
    return char_ty();

  if (t->ty == TK_VOID)
    return void_ty();

  if (t->ty == TK_TYPEOF) {
    expect('(');
    Node *node = expr();
    expect(')');
    return get_type(node);
  }

  if (t->ty == TK_STRUCT) {
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
        vec_push(members, declaration(false));
    }

    if (!tag && !members)
      bad_token(t, "bad struct definition");

    Type *ty = NULL;
    if (tag && !members)
      ty = find_tag(tag);

    if (!ty) {
      ty = calloc(1, sizeof(Type));
      ty->ty = STRUCT;
    }

    if (members) {
      add_members(ty, members);
      if (tag)
        map_put(env->tags, tag, ty);
    }
    return ty;
  }

  bad_token(t, "typename expected");
}

static Node *new_node(int op, Token *t) {
  Node *node = calloc(1, sizeof(Node));
  node->op = op;
  node->token = t;
  return node;
}

static Node *new_binop(int op, Token *t, Node *lhs, Node *rhs) {
  Node *node = new_node(op, t);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

static Node *new_expr(int op, Token *t, Node *expr) {
  Node *node = new_node(op, t);
  node->expr = expr;
  return node;
}

Node *new_int_node(int val, Token *t) {
  Node *node = new_node(ND_NUM, t);
  node->ty = int_ty();
  node->val = val;
  return node;
}

static Node *compound_stmt();

static char *ident() {
  Token *t = tokens->data[pos++];
  if (t->ty != TK_IDENT)
    bad_token(t, "identifier expected");
  return t->name;
}

static Node *string_literal(Token *t) {
  Type *ty = ary_of(char_ty(), t->len);
  char *name = format(".L.str%d", label++);

  Node *node = new_node(ND_VAR, t);
  node->ty = ty;
  node->var = add_gvar(ty, name, t->str, t->len);
  return node;
}

static Node *local_variable(Token *t) {
  Var *var = find_var(t->name);
  if (!var)
    bad_token(t, "undefined variable");
  Node *node = new_node(ND_VAR, t);
  node->ty = var->ty;
  node->name = t->name;
  node->var = var;
  return node;
}

static Node *function_call(Token *t) {
  Var *var = find_var(t->name);

  Node *node = new_node(ND_CALL, t);
  node->name = t->name;
  node->args = new_vec();

  if (var && var->ty->ty == FUNC) {
    node->ty = var->ty;
  } else {
    warn_token(t, "undefined function");
    node->ty = func_ty(int_ty());
  }

  if (consume(')'))
    return node;

  vec_push(node->args, assign());
  while (consume(','))
    vec_push(node->args, assign());
  expect(')');
  return node;
}

static Node *primary() {
  Token *t = tokens->data[pos++];

  if (t->ty == '(') {
    if (consume('{')) {
      Node *node = new_node(ND_STMT_EXPR, t);
      node->body = compound_stmt();
      expect(')');
      return node;
    }
    Node *node = expr();
    expect(')');
    return node;
  }

  if (t->ty == TK_NUM)
    return new_int_node(t->val, t);

  if (t->ty == TK_STR)
    return string_literal(t);

  if (t->ty == TK_IDENT) {
    if (consume('('))
      return function_call(t);
    return local_variable(t);
  }

  bad_token(t, "primary expression expected");
}

static Node *mul();

static Node *postfix() {
  Node *lhs = primary();

  for (;;) {
    Token *t = tokens->data[pos];

    if (consume(TK_INC)) {
      lhs = new_expr(ND_POST_INC, t, lhs);
      continue;
    }

    if (consume(TK_DEC)) {
      lhs = new_expr(ND_POST_DEC, t, lhs);
      continue;
    }

    if (consume('.')) {
      lhs = new_expr(ND_DOT, t, lhs);
      lhs->name = ident();
      continue;
    }

    if (consume(TK_ARROW)) {
      lhs = new_expr(ND_DOT, t, new_expr(ND_DEREF, t, lhs));
      lhs->name = ident();
      continue;
    }

    if (consume('[')) {
      Node *node = new_binop('+', t, lhs, assign());
      lhs = new_expr(ND_DEREF, t, node);
      expect(']');
      continue;
    }
    return lhs;
  }
}

static Node *unary() {
  Token *t = tokens->data[pos];

  if (consume('-'))
    return new_expr(ND_NEG, t, unary());
  if (consume('*'))
    return new_expr(ND_DEREF, t, unary());
  if (consume('&'))
    return new_expr(ND_ADDR, t, unary());
  if (consume('!'))
    return new_expr('!', t, unary());
  if (consume('~'))
    return new_expr('~', t, unary());
  if (consume(TK_SIZEOF))
    return new_expr(ND_SIZEOF, t, unary());
  if (consume(TK_ALIGNOF))
    return new_expr(ND_ALIGNOF, t, unary());
  if (consume(TK_INC))
    return new_binop(ND_ADD_EQ, t, unary(), new_int_node(1, t));
  if (consume(TK_DEC))
    return new_binop(ND_SUB_EQ, t, unary(), new_int_node(1, t));
  return postfix();
}

static Node *mul() {
  Node *lhs = unary();
  for (;;) {
    Token *t = tokens->data[pos];
    if (consume('*'))
      lhs = new_binop('*', t, lhs, unary());
    else if (consume('/'))
      lhs = new_binop('/', t, lhs, unary());
    else if (consume('%'))
      lhs = new_binop('%', t, lhs, unary());
    else
      return lhs;
  }
}

static Node *add() {
  Node *lhs = mul();
  for (;;) {
    Token *t = tokens->data[pos];
    if (consume('+'))
      lhs = new_binop('+', t, lhs, mul());
    else if (consume('-'))
      lhs = new_binop('-', t, lhs, mul());
    else
      return lhs;
  }
}

static Node *shift() {
  Node *lhs = add();
  for (;;) {
    Token *t = tokens->data[pos];
    if (consume(TK_SHL))
      lhs = new_binop(ND_SHL, t, lhs, add());
    else if (consume(TK_SHR))
      lhs = new_binop(ND_SHR, t, lhs, add());
    else
      return lhs;
  }
}

static Node *relational() {
  Node *lhs = shift();
  for (;;) {
    Token *t = tokens->data[pos];
    if (consume('<'))
      lhs = new_binop('<', t, lhs, shift());
    else if (consume('>'))
      lhs = new_binop('<', t, shift(), lhs);
    else if (consume(TK_LE))
      lhs = new_binop(ND_LE, t, lhs, shift());
    else if (consume(TK_GE))
      lhs = new_binop(ND_LE, t, shift(), lhs);
    else
      return lhs;
  }
}

static Node *equality() {
  Node *lhs = relational();
  for (;;) {
    Token *t = tokens->data[pos];
    if (consume(TK_EQ))
      lhs = new_binop(ND_EQ, t, lhs, relational());
    else if (consume(TK_NE))
      lhs = new_binop(ND_NE, t, lhs, relational());
    else
      return lhs;
  }
}

static Node *bit_and() {
  Node *lhs = equality();
  while (consume('&')) {
    Token *t = tokens->data[pos];
    lhs = new_binop('&', t, lhs, equality());
  }
  return lhs;
}

static Node *bit_xor() {
  Node *lhs = bit_and();
  while (consume('^')) {
    Token *t = tokens->data[pos];
    lhs = new_binop('^', t, lhs, bit_and());
  }
  return lhs;
}

static Node *bit_or() {
  Node *lhs = bit_xor();
  while (consume('|')) {
    Token *t = tokens->data[pos];
    lhs = new_binop('|', t, lhs, bit_xor());
  }
  return lhs;
}

static Node *logand() {
  Node *lhs = bit_or();
  while (consume(TK_LOGAND)) {
    Token *t = tokens->data[pos];
    lhs = new_binop(ND_LOGAND, t, lhs, bit_or());
  }
  return lhs;
}

static Node *logor() {
  Node *lhs = logand();
  while (consume(TK_LOGOR)) {
    Token *t = tokens->data[pos];
    lhs = new_binop(ND_LOGOR, t, lhs, logand());
  }
  return lhs;
}

static Node *conditional() {
  Node *cond = logor();
  Token *t = tokens->data[pos];
  if (!consume('?'))
    return cond;

  Node *node = new_node('?', t);
  node->cond = cond;
  node->then = expr();
  expect(':');
  node->els = conditional();
  return node;
}

static int assignment_op() {
  if (consume('='))
    return '=';
  if (consume(TK_MUL_EQ))
    return ND_MUL_EQ;
  if (consume(TK_DIV_EQ))
    return ND_DIV_EQ;
  if (consume(TK_MOD_EQ))
    return ND_MOD_EQ;
  if (consume(TK_ADD_EQ))
    return ND_ADD_EQ;
  if (consume(TK_SUB_EQ))
    return ND_SUB_EQ;
  if (consume(TK_SHL_EQ))
    return ND_SHL_EQ;
  if (consume(TK_SHR_EQ))
    return ND_SHR_EQ;
  if (consume(TK_BITAND_EQ))
    return ND_BITAND_EQ;
  if (consume(TK_XOR_EQ))
    return ND_XOR_EQ;
  if (consume(TK_BITOR_EQ))
    return ND_BITOR_EQ;
  return 0;
}

static Node *assign() {
  Node *lhs = conditional();
  Token *t = tokens->data[pos];
  int op = assignment_op();
  if (op)
    return new_binop(op, t, lhs, assign());
  return lhs;
}

static Node *expr() {
  Node *lhs = assign();
  Token *t = tokens->data[pos];
  if (!consume(','))
    return lhs;
  return new_binop(',', t, lhs, expr());
}

static Type *read_array(Type *ty) {
  Vector *v = new_vec();

  while (consume('[')) {
    if (consume(']')) {
      vec_push(v, (void *)(intptr_t)-1);
      continue;
    }

    Token *t = tokens->data[pos];
    Node *len = expr();
    if (len->op != ND_NUM)
      bad_token(t, "number expected");
    vec_push(v, (void *)(intptr_t)len->val);
    expect(']');
  }

  for (int i = v->len - 1; i >= 0; i--) {
    int len = (intptr_t)v->data[i];
    ty = ary_of(ty, len);
  }
  return ty;
}

static Node *declarator(Type *ty);

static Node *direct_decl(Type *ty) {
  Token *t = tokens->data[pos];
  Node *node;
  Type *placeholder = calloc(1, sizeof(Type));

  if (t->ty == TK_IDENT) {
    node = new_node(ND_VARDEF, t);
    node->ty = placeholder;
    node->name = ident();
  } else if (consume('(')) {
    node = declarator(placeholder);
    expect(')');
  } else {
    bad_token(t, "bad direct-declarator");
  }

  // Read the second half of type name (e.g. `[3][5]`).
  *placeholder = *read_array(ty);

  // Read an initializer.
  if (consume('='))
    node->init = assign();
  return node;
}

static Node *declarator(Type *ty) {
  while (consume('*'))
    ty = ptr_to(ty);
  return direct_decl(ty);
}

static Node *declaration(bool define) {
  Type *ty = decl_specifiers();
  Node *node = declarator(ty);
  expect(';');
  if (define)
    node->var = add_lvar(node->ty, node->name);
  return node;
}

static Node *param_declaration() {
  Type *ty = decl_specifiers();
  Node *node = declarator(ty);
  if (node->ty->ty == ARY)
    node->ty = ptr_to(node->ty->ary_of);
  node->var = add_lvar(node->ty, node->name);
  return node;
}

static Node *expr_stmt() {
  Token *t = tokens->data[pos];
  Node *node = new_expr(ND_EXPR_STMT, t, expr());
  expect(';');
  return node;
}

static Node *stmt() {
  Token *t = tokens->data[pos++];

  switch (t->ty) {
  case TK_TYPEDEF: {
    Node *node = declaration(false);
    assert(node->name);
    map_put(env->typedefs, node->name, node->ty);
    return &null_stmt;
  }
  case TK_IF: {
    Node *node = new_node(ND_IF, t);
    expect('(');
    node->cond = expr();
    expect(')');

    node->then = stmt();

    if (consume(TK_ELSE))
      node->els = stmt();
    return node;
  }
  case TK_FOR: {
    Node *node = new_node(ND_FOR, t);
    expect('(');
    env = new_env(env);

    if (is_typename())
      node->init = declaration(true);
    else if (consume(';'))
      node->init = &null_stmt;
    else
      node->init = expr_stmt();

    if (!consume(';')) {
      node->cond = expr();
      expect(';');
    }

    if (!consume(')')) {
      node->inc = new_expr(ND_EXPR_STMT, t, expr());
      expect(')');
    }

    node->body = stmt();
    env = env->next;
    return node;
  }
  case TK_WHILE: {
    Node *node = new_node(ND_FOR, t);
    node->init = &null_stmt;
    node->inc = &null_stmt;
    expect('(');
    node->cond = expr();
    expect(')');
    node->body = stmt();
    return node;
  }
  case TK_DO: {
    Node *node = new_node(ND_DO_WHILE, t);
    node->body = stmt();
    expect(TK_WHILE);
    expect('(');
    node->cond = expr();
    expect(')');
    expect(';');
    return node;
  }
  case TK_BREAK:
    return &break_stmt;
  case TK_RETURN: {
    Node *node = new_node(ND_RETURN, t);
    node->expr = expr();
    expect(';');
    return node;
  }
  case '{': {
    env = new_env(env);
    Node *node = new_node(ND_COMP_STMT, t);
    node->stmts = new_vec();
    while (!consume('}'))
      vec_push(node->stmts, stmt());
    env = env->next;
    return node;
  }
  case ';':
    return &null_stmt;
  default:
    pos--;
    if (is_typename())
      return declaration(true);
    return expr_stmt();
  }
}

static Node *compound_stmt() {
  Token *t = tokens->data[pos];
  Node *node = new_node(ND_COMP_STMT, t);
  node->stmts = new_vec();

  env = new_env(env);
  while (!consume('}'))
    vec_push(node->stmts, stmt());
  env = env->next;
  return node;
}

static void toplevel() {
  bool is_typedef = consume(TK_TYPEDEF);
  bool is_extern = consume(TK_EXTERN);

  Type *ty = decl_specifiers();
  while (consume('*'))
    ty = ptr_to(ty);

  char *name = ident();

  // Function
  if (consume('(')) {
    Token *t = tokens->data[pos];
    Node *node = new_node(ND_DECL, t);
    vec_push(prog->nodes, node);

    lvars = new_vec();

    node->name = name;
    node->args = new_vec();

    node->ty = calloc(1, sizeof(Type));
    node->ty->ty = FUNC;
    node->ty->returning = ty;

    if (!consume(')')) {
      vec_push(node->args, param_declaration());
      while (consume(','))
        vec_push(node->args, param_declaration());
      expect(')');
    }

    add_lvar(node->ty, name);

    if (consume(';'))
      return;

    node->op = ND_FUNC;
    t = tokens->data[pos];
    expect('{');
    if (is_typedef)
      bad_token(t, "typedef has function definition");
    node->body = compound_stmt();
    node->lvars = lvars;
    return;
  }

  ty = read_array(ty);
  expect(';');

  if (is_typedef) {
    map_put(env->typedefs, name, ty);
    return;
  }

  // Global variable
  ty->is_extern = is_extern;
  char *data = calloc(1, ty->size);
  int len = ty->size;
  add_gvar(ty, name, data, len);
};

static bool is_eof() {
  Token *t = tokens->data[pos];
  return t->ty == TK_EOF;
}

Program *parse(Vector *tokens_) {
  tokens = tokens_;
  pos = 0;
  env = new_env(NULL);

  prog = calloc(1, sizeof(Program));
  prog->nodes = new_vec();
  prog->gvars = new_vec();
  prog->funcs = new_vec();

  while (!is_eof())
    toplevel();
  return prog;
}
