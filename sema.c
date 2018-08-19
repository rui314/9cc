#include "9cc.h"

static Type int_ty = {INT, NULL};

typedef struct {
  Type *ty;
  int offset;
} Var;

static Map *vars;
static int stacksize;

static void walk(Node *node) {
  switch (node->op) {
  case ND_NUM:
    return;
  case ND_IDENT: {
    Var *var = map_get(vars, node->name);
    if (!var)
      error("undefined variable: %s", node->name);
    node->ty = var->ty;
    node->op = ND_LVAR;
    node->offset = var->offset;
    return;
  }
  case ND_VARDEF: {
    stacksize += 8;
    node->offset = stacksize;

    Var *var = calloc(1, sizeof(Var));
    var->ty = node->ty;
    var->offset = stacksize;
    map_put(vars, node->name, var);

    if (node->init)
      walk(node->init);
    return;
  }
  case ND_IF:
    walk(node->cond);
    walk(node->then);
    if (node->els)
      walk(node->els);
    return;
  case ND_FOR:
    walk(node->init);
    walk(node->cond);
    walk(node->inc);
    walk(node->body);
    return;
  case '+':
    walk(node->lhs);
    walk(node->rhs);
    node->ty = node->lhs->ty;
    return;
  case '-':
  case '*':
  case '/':
  case '=':
  case '<':
  case ND_LOGAND:
  case ND_LOGOR:
    walk(node->lhs);
    walk(node->rhs);
    node->ty = node->lhs->ty;
    return;
  case ND_DEREF:
  case ND_RETURN:
    walk(node->expr);
    return;
  case ND_CALL:
    for (int i = 0; i < node->args->len; i++)
      walk(node->args->data[i]);
    node->ty = &int_ty;
    return;
  case ND_FUNC:
    for (int i = 0; i < node->args->len; i++)
      walk(node->args->data[i]);
    walk(node->body);
    return;
  case ND_COMP_STMT:
    for (int i = 0; i < node->stmts->len; i++)
      walk(node->stmts->data[i]);
    return;
  case ND_EXPR_STMT:
    walk(node->expr);
    return;
  default:
    assert(0 && "unknown node type");
  }
}

void sema(Vector *nodes) {
  for (int i = 0; i < nodes->len; i++) {
    Node *node = nodes->data[i];
    assert(node->op == ND_FUNC);

    vars = new_map();
    stacksize = 0;
    walk(node);
    node->stacksize = stacksize;
  }
}
