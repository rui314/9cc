#include "9cc.h"

static Type int_ty = {INT, NULL};

typedef struct Env {
  Map *vars;
  struct Env *next;
} Env;

static Vector *globals;
static int str_label;
static int stacksize;

static Env *new_env(Env *next) {
  Env *env = calloc(1, sizeof(Env));
  env->vars = new_map();
  env->next = next;
  return env;
}

static Var *find(Env *env, char *name) {
  for (; env; env = env->next) {
    Var *var = map_get(env->vars, name);
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
  node->ty = ptr_of(base->ty->ary_of);
  node->expr = base;
  return node;
}

static Node *walk(Env *env, Node *node, bool decay) {
  switch (node->op) {
  case ND_NUM:
    return node;
  case ND_STR: {
    Var *var = calloc(1, sizeof(Var));
    vec_push(globals, var);
    var->ty = node->ty;
    var->is_local = false;
    var->name = format(".L.str%d", str_label++);
    var->data = node->str;
    var->len = strlen(node->str) + 1;

    Node *ret = calloc(1, sizeof(Node));
    ret->op = ND_GVAR;
    ret->ty = node->ty;
    ret->name = var->name;
    return maybe_decay(ret, decay);
  }
  case ND_IDENT: {
    Var *var = find(env, node->name);
    if (!var)
      error("undefined variable: %s", node->name);

    Node *ret = calloc(1, sizeof(Node));
    ret->op = ND_LVAR;
    ret->offset = var->offset;
    ret->ty = var->ty;
    return maybe_decay(ret, decay);
  }
  case ND_VARDEF: {
    stacksize += size_of(node->ty);
    node->offset = stacksize;

    Var *var = calloc(1, sizeof(Var));
    var->ty = node->ty;
    var->is_local = true;
    var->offset = stacksize;
    map_put(env->vars, node->name, var);

    if (node->init)
      node->init = walk(env, node->init, true);
    return node;
  }
  case ND_IF:
    node->cond = walk(env, node->cond, true);
    node->then = walk(env, node->then, true);
    if (node->els)
      node->els = walk(env, node->els, true);
    return node;
  case ND_FOR:
    node->init = walk(env, node->init, true);
    node->cond = walk(env, node->cond, true);
    node->inc = walk(env, node->inc, true);
    node->body = walk(env, node->body, true);
    return node;
  case '+':
  case '-':
    node->lhs = walk(env, node->lhs, true);
    node->rhs = walk(env, node->rhs, true);

    if (node->rhs->ty->ty == PTR)
      swap(&node->lhs, &node->rhs);
    if (node->rhs->ty->ty == PTR)
      error("'pointer %c pointer' is not defined", node->op);

    node->ty = node->lhs->ty;
    return node;
  case '=':
    node->lhs = walk(env, node->lhs, false);
    if (node->lhs->op != ND_LVAR && node->lhs->op != ND_DEREF)
      error("not an lvalue: %d (%s)", node->op, node->name);

    node->rhs = walk(env, node->rhs, true);
    node->ty = node->lhs->ty;
    return node;
  case '*':
  case '/':
  case '<':
  case ND_LOGAND:
  case ND_LOGOR:
    node->lhs = walk(env, node->lhs, true);
    node->rhs = walk(env, node->rhs, true);
    node->ty = node->lhs->ty;
    return node;
  case ND_ADDR:
    node->expr = walk(env, node->expr, true);
    node->ty = ptr_of(node->expr->ty);
    return node;
  case ND_DEREF:
    node->expr = walk(env, node->expr, true);
    if (node->expr->ty->ty != PTR)
      error("operand must be a pointer");
    node->ty = node->expr->ty->ptr_of;
    return node;
  case ND_RETURN:
    node->expr = walk(env, node->expr, true);
    return node;
  case ND_SIZEOF: {
    Node *expr = walk(env, node->expr, false);

    Node *ret = calloc(1, sizeof(Node));
    ret->op = ND_NUM;
    ret->ty = INT;
    ret->val = size_of(expr->ty);
    return ret;
  }
  case ND_CALL:
    for (int i = 0; i < node->args->len; i++)
      node->args->data[i] = walk(env, node->args->data[i], true);
    node->ty = &int_ty;
    return node;
  case ND_FUNC:
    for (int i = 0; i < node->args->len; i++)
      node->args->data[i] = walk(env, node->args->data[i], true);
    node->body = walk(env, node->body, true);
    return node;
  case ND_COMP_STMT: {
    Env *newenv = new_env(env);
    for (int i = 0; i < node->stmts->len; i++)
      node->stmts->data[i] = walk(newenv, node->stmts->data[i], true);
    return node;
  }
  case ND_EXPR_STMT:
    node->expr = walk(env, node->expr, true);
    return node;
  default:
    assert(0 && "unknown node type");
  }
}

void sema(Vector *nodes) {
  for (int i = 0; i < nodes->len; i++) {
    Node *node = nodes->data[i];
    assert(node->op == ND_FUNC);

    globals = new_vec();
    stacksize = 0;

    walk(new_env(NULL), node, true);
    node->stacksize = stacksize;
    node->globals = globals;
  }
}
