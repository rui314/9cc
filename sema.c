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
// - Scales operands for pointer arithmetic. E.g. ptr+1 becomes ptr+4
//   for integer and becomes ptr+8 for pointer.
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

static Node *new_lvar_node(Type *ty, int offset) {
  Node *node = calloc(1, sizeof(Node));
  node->op = ND_LVAR;
  node->ty = ty;
  node->offset = offset;
  return node;
}

static Node *new_gvar_node(Type *ty, char *name) {
  Node *node = calloc(1, sizeof(Node));
  node->op = ND_GVAR;
  node->ty = ty;
  node->name = name;
  return node;
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

noreturn static void bad_node(Node *node, char *msg) {
  bad_token(node->token, msg);
}

static void warn_node(Node *node, char *msg) { warn_token(node->token, msg); }

static void check_lval(Node *node) {
  int op = node->op;
  if (op != ND_LVAR && op != ND_GVAR && op != ND_DEREF && op != ND_DOT)
    bad_node(node, "not an lvalue");
}

static Node *scale_ptr(Node *node, Type *ty) {
  Node *e = calloc(1, sizeof(Node));
  e->op = '*';
  e->lhs = node;
  e->rhs = new_int_node(ty->ptr_to->size, node->token);
  return e;
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
    Node *n = new_gvar_node(node->ty, var->name);
    return maybe_decay(n, decay);
  }
  case ND_IDENT: {
    Var *var = find_var(node->name);
    if (!var)
      bad_node(node, "undefined variable");

    Node *n;
    if (var->is_local)
      n = new_lvar_node(var->ty, var->offset);
    else
      n = new_gvar_node(var->ty, var->name);
    return maybe_decay(n, decay);
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
    if (node->cond)
      node->cond = walk(node->cond, true);
    if (node->inc)
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
      bad_node(node, format("'pointer %c pointer' is not defined", node->op));

    if (node->lhs->ty->ty == PTR)
      node->rhs = scale_ptr(node->rhs, node->lhs->ty);

    node->ty = node->lhs->ty;
    return node;
  case ND_ADD_EQ:
  case ND_SUB_EQ:
    node->lhs = walk(node->lhs, false);
    check_lval(node->lhs);
    node->rhs = walk(node->rhs, true);
    node->ty = node->lhs->ty;

    if (node->lhs->ty->ty == PTR)
      node->rhs = scale_ptr(node->rhs, node->lhs->ty);
    return node;
  case '=':
  case ND_MUL_EQ:
  case ND_DIV_EQ:
  case ND_MOD_EQ:
  case ND_SHL_EQ:
  case ND_SHR_EQ:
  case ND_BITAND_EQ:
  case ND_XOR_EQ:
  case ND_BITOR_EQ:
    node->lhs = walk(node->lhs, false);
    check_lval(node->lhs);
    node->rhs = walk(node->rhs, true);
    node->ty = node->lhs->ty;
    return node;
  case ND_DOT:
    node->expr = walk(node->expr, true);
    if (node->expr->ty->ty != STRUCT)
      bad_node(node, "struct expected before '.'");

    Type *ty = node->expr->ty;
    if (!ty->members)
      bad_node(node, "incomplete type");

    for (int i = 0; i < ty->members->len; i++) {
      Node *m = ty->members->data[i];
      if (strcmp(m->name, node->name))
        continue;
      node->ty = m->ty;
      node->offset = m->ty->offset;
      return maybe_decay(node, decay);
    }
    bad_node(node, format("member missing: %s", node->name));
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
  case ND_POST_INC:
  case ND_POST_DEC:
  case ND_NEG:
  case '!':
  case '~':
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
      bad_node(node, "operand must be a pointer");

    if (node->expr->ty->ptr_to->ty == VOID)
      bad_node(node, "cannot dereference void pointer");

    node->ty = node->expr->ty->ptr_to;
    return maybe_decay(node, decay);
  case ND_RETURN:
  case ND_EXPR_STMT:
    node->expr = walk(node->expr, true);
    return node;
  case ND_SIZEOF: {
    Node *expr = walk(node->expr, false);
    return new_int_node(expr->ty->size, expr->token);
  }
  case ND_ALIGNOF: {
    Node *expr = walk(node->expr, false);
    return new_int_node(expr->ty->align, expr->token);
  }
  case ND_CALL: {
    Var *var = find_var(node->name);
    if (var && var->ty->ty == FUNC) {
      node->ty = var->ty->returning;
    } else {
      warn_node(node, "undefined function");
      node->ty = &int_ty;
    }

    for (int i = 0; i < node->args->len; i++)
      node->args->data[i] = walk(node->args->data[i], true);
    return node;
  }
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

    assert(node->op == ND_DECL || node->op == ND_FUNC);

    Var *var = new_global(node->ty, node->name, "", 0);
    map_put(env->vars, node->name, var);

    if (node->op == ND_DECL)
      continue;

    stacksize = 0;

    for (int i = 0; i < node->args->len; i++)
      node->args->data[i] = walk(node->args->data[i], true);
    node->body = walk(node->body, true);

    node->stacksize = stacksize;
  }

  return globals;
}
