#include "9cc.h"

// Tokenizer
static Token *add_token(Vector *v, int ty, char *input) {
  Token *t = calloc(1, sizeof(Token));
  t->ty = ty;
  t->input = input;
  vec_push(v, t);
  return t;
}

static struct {
  char *name;
  int ty;
} symbols[] = {
    {"char", TK_CHAR},     {"do", TK_DO},
    {"else", TK_ELSE},     {"for", TK_FOR},
    {"if", TK_IF},         {"int", TK_INT},
    {"return", TK_RETURN}, {"sizeof", TK_SIZEOF},
    {"while", TK_WHILE},   {"&&", TK_LOGAND},
    {"||", TK_LOGOR},      {"==", TK_EQ},
    {"!=", TK_NE},         {NULL, 0},
};

static int read_string(StringBuilder *sb, char *p) {
  char *start = p;

  while (*p != '"') {
    if (!*p)
      error("premature end of input");

    if (*p != '\\') {
      sb_add(sb, *p++);
      continue;
    }

    p++;
    if (*p == 'a')
      sb_add(sb, '\a');
    else if (*p == 'b')
      sb_add(sb, '\b');
    else if (*p == 'f')
      sb_add(sb, '\f');
    else if (*p == 'n')
      sb_add(sb, '\n');
    else if (*p == 'r')
      sb_add(sb, '\r');
    else if (*p == 't')
      sb_add(sb, '\t');
    else if (*p == 'v')
      sb_add(sb, '\v');
    else if (*p == '\0')
      error("PREMATURE end of input");
    else
      sb_add(sb, *p);
    p++;
  }
  return p - start + 1;
}

// Tokenized input is stored to this array.
Vector *tokenize(char *p) {
  Vector *v = new_vec();

loop:
  while (*p) {
    // Skip whitespace
    if (isspace(*p)) {
      p++;
      continue;
    }

    // String literal
    if (*p == '"') {
      Token *t = add_token(v, TK_STR, p);
      p++;

      StringBuilder *sb = new_sb();
      p += read_string(sb, p);
      t->str = sb_get(sb);
      t->len = sb->len;
      continue;
    }

    // Multi-letter symbol or keyword
    for (int i = 0; symbols[i].name; i++) {
      char *name = symbols[i].name;
      int len = strlen(name);
      if (strncmp(p, name, len))
        continue;

      add_token(v, symbols[i].ty, p);
      p += len;
      goto loop;
    }

    // Single-letter token
    if (strchr("+-*/;=(),{}<>[]&", *p)) {
      add_token(v, *p, p);
      p++;
      continue;
    }

    // Identifier
    if (isalpha(*p) || *p == '_') {
      int len = 1;
      while (isalpha(p[len]) || isdigit(p[len]) || p[len] == '_')
        len++;

      Token *t = add_token(v, TK_IDENT, p);
      t->name = strndup(p, len);
      p += len;
      continue;
    }

    // Number
    if (isdigit(*p)) {
      Token *t = add_token(v, TK_NUM, p);
      for (; isdigit(*p); p++)
        t->val = t->val * 10 + *p - '0';
      continue;
    }

    error("cannot tokenize: %s", p);
  }

  add_token(v, TK_EOF, p);
  return v;
}
