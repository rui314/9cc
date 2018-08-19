#include "9cc.h"

// Tokenizer
static Token *add_token(Vector *v, int ty, char *input) {
  Token *t = malloc(sizeof(Token));
  t->ty = ty;
  t->input = input;
  vec_push(v, t);
  return t;
}

static struct {
  char *name;
  int ty;
} symbols[] = {
    {"else", TK_ELSE}, {"for", TK_FOR},  {"if", TK_IF}, {"return", TK_RETURN},
    {"&&", TK_LOGAND}, {"||", TK_LOGOR}, {NULL, 0},
};

// Tokenized input is stored to this array.
static Vector *scan(char *p) {
  Vector *v = new_vec();

  int i = 0;

loop:
  while (*p) {
    // Skip whitespace
    if (isspace(*p)) {
      p++;
      continue;
    }

    // Single-letter token
    if (strchr("+-*/;=(),{}<>", *p)) {
      add_token(v, *p, p);
      i++;
      p++;
      continue;
    }

    // Multi-letter token
    for (int i = 0; symbols[i].name; i++) {
      char *name = symbols[i].name;
      int len = strlen(name);
      if (strncmp(p, name, len))
        continue;

      add_token(v, symbols[i].ty, p);
      i++;
      p += len;
      goto loop;
    }

    // Identifier
    if (isalpha(*p) || *p == '_') {
      int len = 1;
      while (isalpha(p[len]) || isdigit(p[len]) || p[len] == '_')
        len++;

      Token *t = add_token(v, TK_IDENT, p);
      t->name = strndup(p, len);
      i++;
      p += len;
      continue;
    }

    // Number
    if (isdigit(*p)) {
      Token *t = add_token(v, TK_NUM, p);
      t->val = strtol(p, &p, 10);
      i++;
      continue;
    }

    error("cannot tokenize: %s", p);
  }

  add_token(v, TK_EOF, p);
  return v;
}

Vector *tokenize(char *p) { return scan(p); }
