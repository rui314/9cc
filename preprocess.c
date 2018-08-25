// C preprocessor

#include "9cc.h"

static Map *macros;

typedef struct Context {
  Vector *input;
  Vector *output;
  int pos;
  struct Context *next;
} Context;

static Context *ctx;

static Context *new_ctx(Context *next, Vector *input) {
  Context *c = calloc(1, sizeof(Context));
  c->input = input;
  c->output = new_vec();
  c->next = next;
  return c;
}

static void append(Vector *v) {
  for (int i = 0; i < v->len; i++)
    vec_push(ctx->output, v->data[i]);
}

static void add(Token *t) { vec_push(ctx->output, t); }

static Token *next() {
  assert(ctx->pos < ctx->input->len);
  return ctx->input->data[ctx->pos++];
}

static bool eof() { return ctx->pos == ctx->input->len; }

static Token *get(int ty, char *msg) {
  Token *t = next();
  if (t->ty != ty)
    bad_token(t, msg);
  return t;
}

static void define() {
  Token *t = get(TK_IDENT, "macro name expected");
  char *name = t->name;

  Vector *v = new_vec();
  while (!eof()) {
    t = next();
    if (t->ty == '\n')
      break;
    vec_push(v, t);
  }
  map_put(macros, name, v);
}

static void include() {
  Token *t = get(TK_STR, "string expected");
  char *path = t->str;
  get('\n', "newline expected");
  append(tokenize(path, false));
}

Vector *preprocess(Vector *tokens) {
  if (!macros)
    macros = new_map();
  ctx = new_ctx(ctx, tokens);

  while (!eof()) {
    Token *t = next();

    if (t->ty == TK_IDENT) {
      Vector *macro = map_get(macros, t->name);
      if (macro)
        append(macro);
      else
        add(t);
      continue;
    }

    if (t->ty != '#') {
      add(t);
      continue;
    }

    t = get(TK_IDENT, "identifier expected");

    if (!strcmp(t->name, "define"))
      define();
    else if (!strcmp(t->name, "include"))
      include();
    else
      bad_token(t, "unknown directive");
  }

  Vector *v = ctx->output;
  ctx = ctx->next;
  return v;
}
