// C preprocessor

#include "9cc.h"

Vector *preprocess(Vector *tokens) {
  Vector *v = new_vec();

  for (int i = 0; i < tokens->len;) {
    Token *t = tokens->data[i];
    if (t->ty != '#') {
      i++;
      vec_push(v, t);
      continue;
    }

    t = tokens->data[++i];
    if (t->ty != TK_IDENT || strcmp(t->name, "include"))
      bad_token(t, "'include' expected");

    t = tokens->data[++i];
    if (t->ty != TK_STR)
      bad_token(t, "string expected");

    char *path = t->str;

    t = tokens->data[++i];
    if (t->ty != '\n')
      bad_token(t, "newline expected");

    Vector *nv = tokenize(path, false);
    for (int i = 0; i < nv->len; i++)
      vec_push(v, nv->data[i]);
  }
  return v;
}
