// C preprocessor

#include "9cc.h"

static Map *macros;

typedef struct Env {
  Vector *input;
  Vector *output;
  int pos;
  struct Env *prev;
} Env;

static Env *env;

static Env *new_env(Env *prev, Vector *input) {
  Env *env = calloc(1, sizeof(Env));
  env->input = input;
  env->output = new_vec();
  env->prev = prev;
  return env;
}

enum {
  OBJLIKE,
  FUNCLIKE,
};

typedef struct Macro {
  int ty;
  Vector *tokens;
  Vector *params;
} Macro;

static Macro *new_macro(int ty, char *name) {
  Macro *m = calloc(1, sizeof(Macro));
  m->ty = ty;
  m->tokens = new_vec();
  m->params = new_vec();
  map_put(macros, name, m);
  return m;
}

static void append(Vector *v) {
  for (int i = 0; i < v->len; i++)
    vec_push(env->output, v->data[i]);
}

static void emit(Token *t) {
  vec_push(env->output, t);
}

static Token *next() {
  assert(env->pos < env->input->len);
  return env->input->data[env->pos++];
}

static bool is_eof() {
  return env->pos == env->input->len;
}

static Token *get(int ty, char *msg) {
  Token *t = next();
  if (t->ty != ty)
    bad_token(t, msg);
  return t;
}

static char *ident(char *msg) {
  Token *t = get(TK_IDENT, "parameter name expected");
  return t->name;
}

static Token *peek() {
  return env->input->data[env->pos];
}

static bool consume(int ty) {
  if (peek()->ty != ty)
    return false;
  env->pos++;
  return true;
}

static Vector *read_until_eol() {
  Vector *v = new_vec();
  while (!is_eof()) {
    Token *t = next();
    if (t->ty == '\n')
      break;
    vec_push(v, t);
  }
  return v;
}

static Token *new_int(Token *tmpl, int val) {
  Token *t = calloc(1, sizeof(Token));
  *t = *tmpl;
  t->ty = TK_NUM;
  t->val = val;
  return t;
}

static Token *new_string(Token *tmpl, char *str, int len) {
  Token *t = calloc(1, sizeof(Token));
  *t = *tmpl;
  t->ty = TK_STR;
  t->str = str;
  t->len = len;
  return t;
}

static Token *new_param(Token *tmpl, int val) {
  Token *t = calloc(1, sizeof(Token));
  *t = *tmpl;
  t->ty = TK_PARAM;
  t->val = val;
  return t;
}

static bool is_ident(Token *t, char *s) {
  return t->ty == TK_IDENT && !strcmp(t->name, s);
}

// Replaces macro parameter tokens with TK_PARAM tokens.
static void replace_macro_params(Macro *m) {
  Vector *params = m->params;
  Vector *tokens = m->tokens;

  Map *map = new_map();
  for (int i = 0; i < params->len; i++) {
    char *name = params->data[i];
    map_puti(map, name, i);
  }

  for (int i = 0; i < tokens->len; i++) {
    Token *t = tokens->data[i];
    if (t->ty != TK_IDENT)
      continue;
    int n = map_geti(map, t->name, -1);
    if (n == -1)
      continue;
    tokens->data[i] = new_param(t, n);
  }
}

// Replaces '#' followed by a macro parameter with one token.
static void replace_hash_ident(Macro *m) {
  Vector *tokens = m->tokens;
  Vector *v = new_vec();

  int i = 0;
  for (; i < tokens->len - 1; i++) {
    Token *t1 = tokens->data[i];
    Token *t2 = tokens->data[i + 1];

    if (t1->ty == '#' && t2->ty == TK_PARAM) {
      t2->stringize = true;
      vec_push(v, t2);
      i++;
    } else {
      vec_push(v, t1);
    }
  }

  if (i == tokens->len - 1)
    vec_push(v, tokens->data[i]);
  m->tokens = v;
}

static Vector *read_one_arg() {
  Vector *v = new_vec();
  Token *start = peek();
  int level = 0;

  while (!is_eof()) {
    Token *t = peek();
    if (level == 0)
      if (t->ty == ')' || t->ty == ',')
        return v;

    next();
    if (t->ty == '(')
      level++;
    else if (t->ty == ')')
      level--;
    vec_push(v, t);
  }
  bad_token(start, "unclosed macro argument");
}

static Vector *read_args() {
  Vector *v = new_vec();
  if (consume(')'))
    return v;
  vec_push(v, read_one_arg());
  while (!consume(')')) {
    get(',', "comma expected");
    vec_push(v, read_one_arg());
  }
  return v;
}

static bool emit_special_macro(Token *t) {
  if (is_ident(t, "__LINE__")) {
    emit(new_int(t, get_line_number(t)));
    return true;
  }
  return false;
}

static void apply_objlike(Macro *m, Token *start) {
  for (int i = 0; i < m->tokens->len; i++) {
    Token *t = m->tokens->data[i];
    if (emit_special_macro(t))
      continue;
    emit(t);
  }
}

static void apply_funclike(Macro *m, Token *start) {
  get('(', "comma expected");

  Vector *args = read_args();
  if (m->params->len != args->len)
    bad_token(start, "number of parameter does not match");

  for (int i = 0; i < m->tokens->len; i++) {
    Token *t = m->tokens->data[i];
    if (emit_special_macro(t))
      continue;

    if (t->ty == TK_PARAM) {
      if (t->stringize) {
        char *s = stringize(args->data[t->val]);
        emit(new_string(t, s, strlen(s) + 1));
      } else {
        append(args->data[t->val]);
      }
      continue;
    }
    emit(t);
  }
}

static void apply(Macro *m, Token *start) {
  if (m->ty == OBJLIKE)
    apply_objlike(m, start);
  else
    apply_funclike(m, start);
}

static void define_funclike(char *name) {
  Macro *m = new_macro(FUNCLIKE, name);
  while (!consume(')')) {
    if (m->params->len > 0)
      get(',', ", expected");
    vec_push(m->params, ident("parameter name expected"));
  }

  m->tokens = read_until_eol();
  replace_macro_params(m);
  replace_hash_ident(m);
}

static void define_objlike(char *name) {
  Macro *m = new_macro(OBJLIKE, name);
  m->tokens = read_until_eol();
}

static void define() {
  char *name = ident("macro name expected");
  if (consume('('))
    return define_funclike(name);
  return define_objlike(name);
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
  env = new_env(env, tokens);

  while (!is_eof()) {
    Token *t = next();

    if (t->ty == TK_IDENT) {
      Macro *m = map_get(macros, t->name);
      if (m)
        apply(m, t);
      else
        emit(t);
      continue;
    }

    if (t->ty != '#') {
      emit(t);
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

  Vector *v = env->output;
  env = env->prev;
  return v;
}
