#include "9cc.h"

static Type int_ty = {INT, NULL};

typedef struct {
  Type *ty;
  int offset;
} Var;

static Map *vars;
static int stacksize;

static void swap(Node **p, Node **q) {
  Node *r = *p;
  *p = *q;
  *q = r;
}

static Node *addr_of(Node *base, Type *ty) {
  Node *node = calloc(1, sizeof(Node));
  node->op = ND_ADDR;
  node->ty = ptr_of(ty);

  Node *copy = calloc(1, sizeof(Node));
  memcpy(copy, base, sizeof(Node));
  node->expr = copy;
  return node;
}

static void walk(Node *node, bool decay) {
  switch (node->op) {
  case ND_NUM:
    return;
  case ND_IDENT: {
    Var *var = map_get(vars, node->name);
    if (!var)
      error("undefined variable: %s", node->name);

    node->op = ND_LVAR;
    node->offset = var->offset;

    if (decay && var->ty->ty == ARY)
      *node = *addr_of(node, var->ty->ary_of);
    else
      node->ty = var->ty;
    return;
  }
  case ND_VARDEF: {
    stacksize += size_of(node->ty);
    node->offset = stacksize;

    Var *var = calloc(1, sizeof(Var));
    var->ty = node->ty;
    var->offset = stacksize;
    map_put(vars, node->name, var);

    if (node->init)
      walk(node->init, true);
    return;
  }
  case ND_IF:
    walk(node->cond, true);
    walk(node->then, true);
    if (node->els)
      walk(node->els, true);
    return;
  case ND_FOR:
    walk(node->init, true);
    walk(node->cond, true);
    walk(node->inc, true);
    walk(node->body, true);
    return;
  case '+':
  case '-':
    walk(node->lhs, true);
    walk(node->rhs, true);

    if (node->rhs->ty->ty == PTR)
      swap(&node->lhs, &node->rhs);
    if (node->rhs->ty->ty == PTR)
      error("'pointer %c pointer' is not defined", node->op);

    node->ty = node->lhs->ty;
    return;
  case '=':
    walk(node->lhs, false);
    walk(node->rhs, true);
    node->ty = node->lhs->ty;
    return;
  case '*':
  case '/':
  case '<':
  case ND_LOGAND:
  case ND_LOGOR:
    walk(node->lhs, true);
    walk(node->rhs, true);
    node->ty = node->lhs->ty;
    return;
  case ND_ADDR:
    walk(node->expr, true);
    node->ty = ptr_of(node->expr->ty);
    return;
  case ND_DEREF:
    walk(node->expr, true);
    if (node->expr->ty->ty != PTR)
      error("operand must be a pointer");
    node->ty = node->expr->ty->ptr_of;
    return;
  case ND_RETURN:
    walk(node->expr, true);
    return;
  case ND_CALL:
    for (int i = 0; i < node->args->len; i++)
      walk(node->args->data[i], true);
    node->ty = &int_ty;
    return;
  case ND_FUNC:
    for (int i = 0; i < node->args->len; i++)
      walk(node->args->data[i], true);
    walk(node->body, true);
    return;
  case ND_COMP_STMT:
    for (int i = 0; i < node->stmts->len; i++)
      walk(node->stmts->data[i], true);
    return;
  case ND_EXPR_STMT:
    walk(node->expr, true);
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
    walk(node, true);
    node->stacksize = stacksize;
  }
}
