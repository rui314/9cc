#include "9cc.h"

// Atomic unit in the grammar is called "token".
// For example, `123`, `"abc"` and `while` are tokens.
// The tokenizer splits an input string into tokens.
// Spaces and comments are removed by the tokenizer.

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
    {"_Alignof", TK_ALIGNOF},
    {"char", TK_CHAR},
    {"do", TK_DO},
    {"else", TK_ELSE},
    {"extern", TK_EXTERN},
    {"for", TK_FOR},
    {"if", TK_IF},
    {"int", TK_INT},
    {"return", TK_RETURN},
    {"sizeof", TK_SIZEOF},
    {"struct", TK_STRUCT},
    {"while", TK_WHILE},
    {"&&", TK_LOGAND},
    {"||", TK_LOGOR},
    {"==", TK_EQ},
    {"!=", TK_NE},
    {NULL, 0},
};

static char escaped[256] = {
        ['a'] = '\a', ['b'] = '\b',   ['f'] = '\f',
        ['n'] = '\n', ['r'] = '\r',   ['t'] = '\t',
        ['v'] = '\v', ['e'] = '\033', ['E'] = '\033',
};

static char *read_char(int *result, char *p) {
  if (!*p)
    error("premature end of input");

  if (*p != '\\') {
    *result = *p++;
  } else {
    p++;
    if (!*p)
      error("premature end of input");
    int esc = escaped[(unsigned)*p];
    *result = esc ? esc : *p;
    p++;
  }

  if (*p != '\'')
    error("unclosed character literal");
  return p + 1;
}

static char *read_string(StringBuilder *sb, char *p) {
  while (*p != '"') {
    if (!*p)
      error("premature end of input");

    if (*p != '\\') {
      sb_add(sb, *p++);
      continue;
    }

    p++;
    if (*p == '\0')
      error("PREMATURE end of input");
    int esc = escaped[(unsigned)*p];
    sb_add(sb, esc ? esc : *p);
    p++;
  }
  return p + 1;
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

    // Line comment
    if (!strncmp(p, "//", 2)) {
      while (*p && *p != '\n')
        p++;
      continue;
    }

    // Block comment
    if (!strncmp(p, "/*", 2)) {
      p += 2;
      for (;;) {
        if (*p == '\0')
          error("premature end of input");
        if (!strncmp(p, "*/", 2)) {
          p += 2;
          break;
        }
      }
      continue;
    }

    // Character literal
    if (*p == '\'') {
      Token *t = add_token(v, TK_NUM, p);
      p++;
      p = read_char(&t->val, p);
      continue;
    }

    // String literal
    if (*p == '"') {
      Token *t = add_token(v, TK_STR, p);
      p++;

      StringBuilder *sb = new_sb();
      p = read_string(sb, p);
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
