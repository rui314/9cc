#include "9cc.h"

// Semantics analyzer. This pass plays a few important roles as shown
// below:
//
// - Add types to nodes. For example, a tree that represents "1+2" is
//   typed as INT because the result type of an addition of two
//   integers is integer.
//
// - Resolve variable names based on the C scope rules.
//   Local variables are resolved to offsets from the base pointer.
//   Global variables are resolved to their names.
//
// - Insert nodes to make array-to-pointer conversion explicit.
//   Recall that, in C, "array of T" is automatically converted to
//   "pointer to T" in most contexts.
//
// - Reject bad assignments, such as `1=2+3`.

static Type int_ty = {INT, 4, 4};

typedef struct Env {
  Map *vars;
  struct Env *next;
} Env;

static Vector *globals;
static Env *env;
static int str_label;
static int stacksize;

static Env *new_env(Env *next) {
  Env *env = calloc(1, sizeof(Env));
  env->vars = new_map();
  env->next = next;
  return env;
}

static Var *new_global(Type *ty, char *name, char *data, int len) {
  Var *var = calloc(1, sizeof(Var));
  var->ty = ty;
  var->is_local = false;
  var->name = name;
  var->data = data;
  var->len = len;
  return var;
}

static Var *find_var(char *name) {
  for (Env *e = env; e; e = e->next) {
    Var *var = map_get(e->vars, name);
    if (var)
      return var;
  }
  return NULL;
}

static void swap(Node **p, Node **q) {
  Node *r = *p;
  *p = *q;
  *q = r;
}

static Node *maybe_decay(Node *base, bool decay) {
  if (!decay || base->ty->ty != ARY)
    return base;

  Node *node = calloc(1, sizeof(Node));
  node->op = ND_ADDR;
  node->ty = ptr_to(base->ty->ary_of);
  node->expr = base;
  return node;
}

static void check_lval(Node *node) {
  int op = node->op;
  if (op != ND_LVAR && op != ND_GVAR && op != ND_DEREF && op != ND_DOT)
    error("not an lvalue: %d (%s)", op, node->name);
}

static Node *new_int(int val) {
  Node *node = calloc(1, sizeof(Node));
  node->op = ND_NUM;
  node->ty = INT;
  node->val = val;
  return node;
}

static Node *walk(Node *node, bool decay) {
  switch (node->op) {
  case ND_NUM:
  case ND_NULL:
  case ND_BREAK:
    return node;
  case ND_STR: {
    // A string literal is converted to a reference to an anonymous
    // global variable of type char array.
    Var *var = new_global(node->ty, format(".L.str%d", str_label++), node->data,
                          node->len);
    vec_push(globals, var);

    Node *ret = calloc(1, sizeof(Node));
    ret->op = ND_GVAR;
    ret->ty = node->ty;
    ret->name = var->name;
    return maybe_decay(ret, decay);
  }
  case ND_IDENT: {
    Var *var = find_var(node->name);
    if (!var)
      error("undefined variable: %s", node->name);

    if (var->is_local) {
      Node *ret = calloc(1, sizeof(Node));
      ret->op = ND_LVAR;
      ret->ty = var->ty;
      ret->offset = var->offset;
      return maybe_decay(ret, decay);
    }

    Node *ret = calloc(1, sizeof(Node));
    ret->op = ND_GVAR;
    ret->ty = var->ty;
    ret->name = var->name;
    return maybe_decay(ret, decay);
  }
  case ND_VARDEF: {
    stacksize = roundup(stacksize, node->ty->align);
    stacksize += node->ty->size;
    node->offset = stacksize;

    Var *var = calloc(1, sizeof(Var));
    var->ty = node->ty;
    var->is_local = true;
    var->offset = stacksize;
    map_put(env->vars, node->name, var);

    if (node->init)
      node->init = walk(node->init, true);
    return node;
  }
  case ND_IF:
    node->cond = walk(node->cond, true);
    node->then = walk(node->then, true);
    if (node->els)
      node->els = walk(node->els, true);
    return node;
  case ND_FOR:
    env = new_env(env);
    node->init = walk(node->init, true);
    node->cond = walk(node->cond, true);
    node->inc = walk(node->inc, true);
    node->body = walk(node->body, true);
    env = env->next;
    return node;
  case ND_DO_WHILE:
    node->cond = walk(node->cond, true);
    node->body = walk(node->body, true);
    return node;
  case '+':
  case '-':
    node->lhs = walk(node->lhs, true);
    node->rhs = walk(node->rhs, true);

    if (node->rhs->ty->ty == PTR)
      swap(&node->lhs, &node->rhs);
    if (node->rhs->ty->ty == PTR)
      error("'pointer %c pointer' is not defined", node->op);

    node->ty = node->lhs->ty;
    return node;
  case '=':
    node->lhs = walk(node->lhs, false);
    check_lval(node->lhs);
    node->rhs = walk(node->rhs, true);
    node->ty = node->lhs->ty;
    return node;
  case ND_DOT:
    node->expr = walk(node->expr, true);
    if (node->expr->ty->ty != STRUCT)
      error("struct expected before '.'");

    Type *ty = node->expr->ty;
    if (!ty->members)
      error("incomplete type");

    for (int i = 0; i < ty->members->len; i++) {
      Node *m = ty->members->data[i];
      if (strcmp(m->name, node->name))
        continue;
      node->ty = m->ty;
      node->offset = m->ty->offset;
      return maybe_decay(node, decay);
    }
    error("member missing: %s", node->name);
  case '?':
    node->cond = walk(node->cond, true);
    node->then = walk(node->then, true);
    node->els = walk(node->els, true);
    node->ty = node->then->ty;
    return node;
  case '*':
  case '/':
  case '%':
  case '<':
  case '|':
  case '^':
  case '&':
  case ND_EQ:
  case ND_NE:
  case ND_LE:
  case ND_SHL:
  case ND_SHR:
  case ND_LOGAND:
  case ND_LOGOR:
    node->lhs = walk(node->lhs, true);
    node->rhs = walk(node->rhs, true);
    node->ty = node->lhs->ty;
    return node;
  case ',':
    node->lhs = walk(node->lhs, true);
    node->rhs = walk(node->rhs, true);
    node->ty = node->rhs->ty;
    return node;
  case ND_PRE_INC:
  case ND_PRE_DEC:
  case ND_POST_INC:
  case ND_POST_DEC:
  case ND_NEG:
  case '!':
    node->expr = walk(node->expr, true);
    node->ty = node->expr->ty;
    return node;
  case ND_ADDR:
    node->expr = walk(node->expr, true);
    check_lval(node->expr);
    node->ty = ptr_to(node->expr->ty);
    return node;
  case ND_DEREF:
    node->expr = walk(node->expr, true);

    if (node->expr->ty->ty != PTR)
      error("operand must be a pointer");

    if (node->expr->ty->ptr_to->ty == VOID)
      error("cannot dereference void pointer");

    node->ty = node->expr->ty->ptr_to;
    return node;
  case ND_RETURN:
  case ND_EXPR_STMT:
    node->expr = walk(node->expr, true);
    return node;
  case ND_SIZEOF: {
    Node *expr = walk(node->expr, false);
    return new_int(expr->ty->size);
  }
  case ND_ALIGNOF: {
    Node *expr = walk(node->expr, false);
    return new_int(expr->ty->align);
  }
  case ND_CALL:
    for (int i = 0; i < node->args->len; i++)
      node->args->data[i] = walk(node->args->data[i], true);
    node->ty = &int_ty;
    return node;
  case ND_FUNC:
    for (int i = 0; i < node->args->len; i++)
      node->args->data[i] = walk(node->args->data[i], true);
    node->body = walk(node->body, true);
    return node;
  case ND_COMP_STMT: {
    env = new_env(env);
    for (int i = 0; i < node->stmts->len; i++)
      node->stmts->data[i] = walk(node->stmts->data[i], true);
    env = env->next;
    return node;
  }
  case ND_STMT_EXPR:
    node->body = walk(node->body, true);
    node->ty = &int_ty;
    return node;
  default:
    assert(0 && "unknown node type");
  }
}

Vector *sema(Vector *nodes) {
  env = new_env(NULL);
  globals = new_vec();

  for (int i = 0; i < nodes->len; i++) {
    Node *node = nodes->data[i];

    if (node->op == ND_VARDEF) {
      Var *var = new_global(node->ty, node->name, node->data, node->len);
      var->is_extern = node->is_extern;
      vec_push(globals, var);
      map_put(env->vars, node->name, var);
      continue;
    }

    assert(node->op == ND_FUNC);

    stacksize = 0;
    walk(node, true);
    node->stacksize = stacksize;
  }

  return globals;
}
