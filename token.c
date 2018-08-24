#include "9cc.h"

/// Error reporting

static char *input_file;

static void print_line(Token *t) {
  char *start = input_file;
  int line = 0;
  int col = 0;

  for (char *p = input_file; p; p++) {
    if (*p == '\n') {
      start = p + 1;
      line++;
      col = 0;
      continue;
    }

    if (p != t->start) {
      col++;
      continue;
    }

    fprintf(stderr, "error at %s:%d:%d\n\n", filename, line + 1, col + 1);

    int linelen = strchr(p, '\n') - start;
    fprintf(stderr, "%.*s\n", linelen, start);

    for (int i = 0; i < col; i++)
      fprintf(stderr, " ");
    fprintf(stderr, "^\n\n");
    return;
  }
}

noreturn void bad_token(Token *t, char *msg) {
  print_line(t);
  error(msg);
}

// Atomic unit in the grammar is called "token".
// For example, `123`, `"abc"` and `while` are tokens.
// The tokenizer splits an input string into tokens.
// Spaces and comments are removed by the tokenizer.

static Token *add_token(Vector *v, int ty, char *start) {
  Token *t = calloc(1, sizeof(Token));
  t->ty = ty;
  t->start = start;
  vec_push(v, t);
  return t;
}

static struct {
  char *name;
  int ty;
} symbols[] = {
    {"<<=", TK_SHL_EQ},   {">>=", TK_SHR_EQ},
    {"!=", TK_NE},        {"&&", TK_LOGAND},
    {"++", TK_INC},       {"--", TK_DEC},
    {"->", TK_ARROW},     {"<<", TK_SHL},
    {"<=", TK_LE},        {"==", TK_EQ},
    {">=", TK_GE},        {">>", TK_SHR},
    {"||", TK_LOGOR},     {"*=", TK_MUL_EQ},
    {"/=", TK_DIV_EQ},    {"%=", TK_MOD_EQ},
    {"+=", TK_ADD_EQ},    {"-=", TK_SUB_EQ},
    {"&=", TK_BITAND_EQ}, {"^=", TK_XOR_EQ},
    {"|=", TK_BITOR_EQ},  {NULL, 0},
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
      error("premature end of input");
    int esc = escaped[(unsigned)*p];
    sb_add(sb, esc ? esc : *p);
    p++;
  }
  return p + 1;
}

static Map *keyword_map() {
  Map *map = new_map();
  map_puti(map, "_Alignof", TK_ALIGNOF);
  map_puti(map, "break", TK_BREAK);
  map_puti(map, "char", TK_CHAR);
  map_puti(map, "do", TK_DO);
  map_puti(map, "else", TK_ELSE);
  map_puti(map, "extern", TK_EXTERN);
  map_puti(map, "for", TK_FOR);
  map_puti(map, "if", TK_IF);
  map_puti(map, "int", TK_INT);
  map_puti(map, "return", TK_RETURN);
  map_puti(map, "sizeof", TK_SIZEOF);
  map_puti(map, "struct", TK_STRUCT);
  map_puti(map, "typedef", TK_TYPEDEF);
  map_puti(map, "void", TK_VOID);
  map_puti(map, "while", TK_WHILE);
  return map;
}

// Tokenized input is stored to this array.
Vector *tokenize(char *p) {
  input_file = p;
  Vector *v = new_vec();
  Map *keywords = keyword_map();

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
      for (p += 2; *p; p++) {
        if (strncmp(p, "*/", 2))
          continue;
        p += 2;
        goto loop;
      }
      error("unclosed comment");
    }

    // Character literal
    if (*p == '\'') {
      Token *t = add_token(v, TK_NUM, p++);
      p = read_char(&t->val, p);
      continue;
    }

    // String literal
    if (*p == '"') {
      Token *t = add_token(v, TK_STR, p++);
      StringBuilder *sb = new_sb();
      p = read_string(sb, p);
      t->str = sb_get(sb);
      t->len = sb->len;
      continue;
    }

    // Multi-letter symbol
    for (int i = 0; symbols[i].name; i++) {
      char *name = symbols[i].name;
      int len = strlen(name);
      if (strncmp(p, name, len))
        continue;

      add_token(v, symbols[i].ty, p);
      p += len;
      goto loop;
    }

    // Single-letter symbol
    if (strchr("+-*/;=(),{}<>[]&.!?:|^%~", *p)) {
      add_token(v, *p, p);
      p++;
      continue;
    }

    // Keyword or identifier
    if (isalpha(*p) || *p == '_') {
      int len = 1;
      while (isalpha(p[len]) || isdigit(p[len]) || p[len] == '_')
        len++;

      char *name = strndup(p, len);
      int ty = map_geti(keywords, name, -1);

      Token *t = add_token(v, (ty == -1) ? TK_IDENT : ty, p);
      t->name = name;
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
