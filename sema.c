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
  node->expr = base;
  return node;
}

static Node *walk(Node *node, bool decay) {
  switch (node->op) {
  case ND_NUM:
    return node;
  case ND_IDENT: {
    Var *var = map_get(vars, node->name);
    if (!var)
      error("undefined variable: %s", node->name);

    node->op = ND_LVAR;
    node->offset = var->offset;

    if (decay && var->ty->ty == ARY)
      return addr_of(node, var->ty->ary_of);
    node->ty = var->ty;
    return node;
  }
  case ND_VARDEF: {
    stacksize += size_of(node->ty);
    node->offset = stacksize;

    Var *var = calloc(1, sizeof(Var));
    var->ty = node->ty;
    var->offset = stacksize;
    map_put(vars, node->name, var);

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
    node->init = walk(node->init, true);
    node->cond = walk(node->cond, true);
    node->inc = walk(node->inc, true);
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
    node->rhs = walk(node->rhs, true);
    node->ty = node->lhs->ty;
    return node;
  case '*':
  case '/':
  case '<':
  case ND_LOGAND:
  case ND_LOGOR:
    node->lhs = walk(node->lhs, true);
    node->rhs = walk(node->rhs, true);
    node->ty = node->lhs->ty;
    return node;
  case ND_ADDR:
    node->expr = walk(node->expr, true);
    node->ty = ptr_of(node->expr->ty);
    return node;
  case ND_DEREF:
    node->expr = walk(node->expr, true);
    if (node->expr->ty->ty != PTR)
      error("operand must be a pointer");
    node->ty = node->expr->ty->ptr_of;
    return node;
  case ND_RETURN:
    node->expr = walk(node->expr, true);
    return node;
  case ND_SIZEOF: {
    Node *expr = walk(node->expr, false);

    Node *ret = calloc(1, sizeof(Node));
    ret->op = ND_NUM;
    ret->ty = INT;
    ret->val = size_of(expr->ty);
    return ret;
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
  case ND_COMP_STMT:
    for (int i = 0; i < node->stmts->len; i++)
      node->stmts->data[i] = walk(node->stmts->data[i], true);
    return node;
  case ND_EXPR_STMT:
    node->expr = walk(node->expr, true);
    return node;
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
