#include "9cc.h"

static Map *vars;
static int stacksize;

static void walk(Node *node) {
  switch (node->ty) {
  case ND_NUM:
    return;
  case ND_IDENT:
    if (!map_exists(vars, node->name))
      error("undefined variable: %s", node->name);
    node->ty = ND_LVAR;
    node->offset = (intptr_t)map_get(vars, node->name);
    return;
  case ND_VARDEF:
    stacksize += 8;
    map_put(vars, node->name, (void *)(intptr_t)stacksize);
    node->offset = stacksize;
    if (node->init)
      walk(node->init);
    return;
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
  case '-':
  case '*':
  case '/':
  case '=':
  case '<':
  case ND_LOGAND:
  case ND_LOGOR:
    walk(node->lhs);
    walk(node->rhs);
    return;
  case ND_RETURN:
    walk(node->expr);
    return;
  case ND_CALL:
    for (int i = 0; i < node->args->len; i++)
      walk(node->args->data[i]);
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
    assert(node->ty == ND_FUNC);

    vars = new_map();
    stacksize = 0;
    walk(node);
    node->stacksize = stacksize;
  }
}
