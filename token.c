#include "9cc.h"

// Tokenizer
static Token *add_token(Vector *v, int ty, char *input) {
  Token *t = malloc(sizeof(Token));
  t->ty = ty;
  t->input = input;
  vec_push(v, t);
  return t;
}

// Tokenized input is stored to this array.
Vector *tokenize(char *p) {
  Vector *v = new_vec();

  int i = 0;
  while (*p) {
    // Skip whitespace
    if (isspace(*p)) {
      p++;
      continue;
    }

    // Single-letter token
    if (strchr("+-*", *p)) {
      add_token(v, *p, p);
      i++;
      p++;
      continue;
    }

    // Number
    if (isdigit(*p)) {
      Token *t = add_token(v, TK_NUM, p);
      t->val = strtol(p, &p, 10);
      i++;
      continue;
    }

    fprintf(stderr, "cannot tokenize: %s", p);
    exit(1);
  }

  add_token(v, TK_EOF, p);
  return v;
}
