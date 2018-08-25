#include "9cc.h"

// Error reporting

static char *buf;
static char *filename;

// Finds a line pointed by a given pointer from the input file
// to print it out.
static void print_line(char *start, char *path, char *pos) {
  int line = 0;
  int col = 0;

  for (char *p = start; p; p++) {
    if (*p == '\n') {
      start = p + 1;
      line++;
      col = 0;
      continue;
    }

    if (p != pos) {
      col++;
      continue;
    }

    fprintf(stderr, "error at %s:%d:%d\n\n", path, line + 1, col + 1);

    int linelen = strchr(p, '\n') - start;
    fprintf(stderr, "%.*s\n", linelen, start);

    for (int i = 0; i < col; i++)
      fprintf(stderr, " ");
    fprintf(stderr, "^\n\n");
    return;
  }
}

noreturn void bad_token(Token *t, char *msg) {
  print_line(t->buf, t->filename, t->start);
  error(msg);
}

// Atomic unit in the grammar is called "token".
// For example, `123`, `"abc"` and `while` are tokens.
// The tokenizer splits an input string into tokens.
// Spaces and comments are removed by the tokenizer.

static Vector *tokens;
static Map *keywords;

static Token *add(int ty, char *start) {
  Token *t = calloc(1, sizeof(Token));
  t->ty = ty;
  t->start = start;
  t->filename = filename;
  t->buf = buf;
  vec_push(tokens, t);
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

static char *read_file(char *path) {
  FILE *fp = stdin;
  if (strcmp(path, "-")) {
    fp = fopen(path, "r");
    if (!fp) {
      perror(path);
      exit(1);
    }
  }

  StringBuilder *sb = new_sb();
  char buf[4096];
  for (;;) {
    int nread = fread(buf, 1, sizeof(buf), fp);
    if (nread == 0)
      break;
    sb_append_n(sb, buf, nread);
  }

  if (sb->data[sb->len] != '\n')
    sb_add(sb, '\n');
  return sb_get(sb);
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

static char *block_comment(char *pos) {
  for (char *p = pos + 2; *p; p++)
    if (!strncmp(p, "*/", 2))
      return p + 2;
  print_line(buf, filename, pos);
  error("unclosed comment");
}

static char *char_literal(char *p) {
  Token *t = add(TK_NUM, p++);

  if (!*p)
    goto err;

  if (*p != '\\') {
    t->val = *p++;
  } else {
    if (!p[1])
      goto err;
    int esc = escaped[(unsigned)p[1]];
    t->val = esc ? esc : p[1];
    p += 2;
  }

  if (*p == '\'')
    return p + 1;

err:
  bad_token(t, "unclosed character literal");
}

static char *string_literal(char *p) {
  Token *t = add(TK_STR, p++);
  StringBuilder *sb = new_sb();

  while (*p != '"') {
    if (!*p)
      goto err;

    if (*p != '\\') {
      sb_add(sb, *p++);
      continue;
    }

    p++;
    if (*p == '\0')
      goto err;
    int esc = escaped[(unsigned)*p];
    sb_add(sb, esc ? esc : *p);
    p++;
  }

  t->str = sb_get(sb);
  t->len = sb->len;
  return p + 1;

err:
  bad_token(t, "unclosed string literal");
}

static char *ident(char *p) {
  int len = 1;
  while (isalpha(p[len]) || isdigit(p[len]) || p[len] == '_')
    len++;

  char *name = strndup(p, len);
  int ty = map_geti(keywords, name, TK_IDENT);
  Token *t = add(ty, p);
  t->name = name;
  return p + len;
}

static char *hexadecimal(char *p) {
  Token *t = add(TK_NUM, p);
  p += 2;

  if (!isxdigit(*p))
    bad_token(t, "bad hexadecimal number");

  for (;;) {
    if ('0' <= *p && *p <= '9')
      t->val = t->val * 16 + *p++ - '0';
    else if ('a' <= *p && *p <= 'f')
      t->val = t->val * 16 + *p++ - 'a' + 10;
    else if ('A' <= *p && *p <= 'F')
      t->val = t->val * 16 + *p++ - 'A' + 10;
    else
      return p;
  }
}

static char *octal(char *p) {
  Token *t = add(TK_NUM, p++);
  while ('0' <= *p && *p <= '7')
    t->val = t->val * 8 + *p++ - '0';
  return p;
}

static char *decimal(char *p) {
  Token *t = add(TK_NUM, p);
  while (isdigit(*p))
    t->val = t->val * 10 + *p++ - '0';
  return p;
}

static char *number(char *p) {
  if (!strncasecmp(p, "0x", 2))
    return hexadecimal(p);
  if (*p == '0')
    return octal(p);
  return decimal(p);
}

// Tokenized input is stored to this array.
static void scan() {
  char *p = buf;

loop:
  while (*p) {
    // New line (preprocessor-only token)
    if (*p == '\n') {
      add(*p, p);
      p++;
      continue;
    }

    // Whitespace
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
      p = block_comment(p);
      continue;
    }

    // Character literal
    if (*p == '\'') {
      p = char_literal(p);
      continue;
    }

    // String literal
    if (*p == '"') {
      p = string_literal(p);
      continue;
    }

    // Multi-letter symbol
    for (int i = 0; symbols[i].name; i++) {
      char *name = symbols[i].name;
      int len = strlen(name);
      if (strncmp(p, name, len))
        continue;

      add(symbols[i].ty, p);
      p += len;
      goto loop;
    }

    // Single-letter symbol
    if (strchr("+-*/;=(),{}<>[]&.!?:|^%~#", *p)) {
      add(*p, p);
      p++;
      continue;
    }

    // Keyword or identifier
    if (isalpha(*p) || *p == '_') {
      p = ident(p);
      continue;
    }

    // Number
    if (isdigit(*p)) {
      p = number(p);
      continue;
    }

    print_line(buf, filename, p);
    error("cannot tokenize");
  }
}

static void canonicalize_newline() {
  char *p = buf;
  for (char *q = p; *q;) {
    if (q[0] == '\r' && q[1] == '\n')
      q++;
    *p++ = *q++;
  }
  *p = '\0';
}

static void remove_backslash_newline() {
  char *p = buf;
  for (char *q = p; *q;) {
    if (q[0] == '\\' && q[1] == '\n')
      q += 2;
    else
      *p++ = *q++;
  }
  *p = '\0';
}

static void strip_newlines() {
  Vector *v = new_vec();
  for (int i = 0; i < tokens->len; i++) {
    Token *t = tokens->data[i];
    if (t->ty != '\n')
      vec_push(v, t);
  }
  tokens = v;
}

static void append(Token *x, Token *y) {
  StringBuilder *sb = new_sb();
  sb_append_n(sb, x->str, x->len - 1);
  sb_append_n(sb, y->str, y->len - 1);
  x->str = sb_get(sb);
  x->len = sb->len;
}

static void join_string_literals() {
  Vector *v = new_vec();
  Token *last = NULL;

  for (int i = 0; i < tokens->len; i++) {
    Token *t = tokens->data[i];
    if (last && last->ty == TK_STR && t->ty == TK_STR) {
      append(last, t);
      continue;
    }

    last = t;
    vec_push(v, t);
  }
  tokens = v;
}

Vector *tokenize(char *path, bool add_eof) {
  if (!keywords)
    keywords = keyword_map();

  Vector *tokens_ = tokens;
  char *filename_ = filename;
  char *buf_ = buf;

  tokens = new_vec();
  filename = path;
  buf = read_file(path);

  canonicalize_newline();
  remove_backslash_newline();

  scan();
  if (add_eof)
    add(TK_EOF, buf);

  tokens = preprocess(tokens);
  strip_newlines();
  join_string_literals();

  Vector *ret = tokens;
  buf = buf_;
  tokens = tokens_;
  filename = filename_;
  return ret;
}
