// C preprocessor

#include "9cc.h"

static Map *defined;

static void append(Vector *v1, Vector *v2) {
  for (int i = 0; i < v2->len; i++)
    vec_push(v1, v2->data[i]);
}

Vector *preprocess(Vector *tokens) {
  if (!defined)
    defined = new_map();

  Vector *v = new_vec();

  for (int i = 0; i < tokens->len;) {
    Token *t = tokens->data[i++];

    if (t->ty == TK_IDENT) {
      Vector *macro = map_get(defined, t->name);
      if (macro)
        append(v, macro);
      else
        vec_push(v, t);
      continue;
    }

    if (t->ty != '#') {
      vec_push(v, t);
      continue;
    }

    t = tokens->data[i++];
    if (t->ty != TK_IDENT)
      bad_token(t, "identifier expected");

    if (!strcmp(t->name, "define")) {
      t = tokens->data[i++];
      if (t->ty != TK_IDENT)
        bad_token(t, "macro name expected");
      char *name = t->name;

      Vector *v2 = new_vec();
      while (i < tokens->len) {
        t = tokens->data[i++];
        if (t->ty == '\n')
          break;
        vec_push(v2, t);
      }

      map_put(defined, name, v2);
      continue;
    }

    if (!strcmp(t->name, "include")) {
      t = tokens->data[i++];
      if (t->ty != TK_STR)
        bad_token(t, "string expected");

      char *path = t->str;

      t = tokens->data[i++];
      if (t->ty != '\n')
        bad_token(t, "newline expected");
      append(v, tokenize(path, false));
      continue;
    }
  }
  return v;
}
