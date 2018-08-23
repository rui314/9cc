#include "9cc.h"

static void loc2linecol(SrcLoc loc, char **pline, int *line, int *col) {
  char *p = loc.file->text;

  *pline = p;
  *line = 1;
  *col = 1;

  while (*p) {
    if (*col == 1)
      *pline = p;

    if (p == loc.pos)
      break;

    if (*p == '\n') {
      *line += 1;
      *col = 1;
    } else if (*p == '\t')
      *col += 4;
    else
      *col += 1;

    p++;
  }
}

noreturn void errorloc(SrcLoc loc, char *fmt, ...) {
  char *pline;
  int line;
  int col;

  // Lazily calculating line/col this way
  // simplifies the line accounting in token.c.
  loc2linecol(loc, &pline, &line, &col);

  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, ":%s:%d:%d\n", loc.file->fname, line, col);

  while (*pline && *pline != '\n') {
    // Convert tab to space for consistent output.
    if(*pline == '\t')
      fputs("    ", stderr);
    else
      fputc(*pline, stderr);

    pline++;
  }

  fputc('\n', stderr);
  for(int i = 0; i < col-1; i++)
    fputc(' ', stderr);
  fputs("^\n", stderr);

  exit(1);
}

noreturn void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

char *format(char *fmt, ...) {
  char buf[2048];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  return strdup(buf);
}

Vector *new_vec() {
  Vector *v = malloc(sizeof(Vector));
  v->data = malloc(sizeof(void *) * 16);
  v->capacity = 16;
  v->len = 0;
  return v;
}

void vec_push(Vector *v, void *elem) {
  if (v->len == v->capacity) {
    v->capacity *= 2;
    v->data = realloc(v->data, sizeof(void *) * v->capacity);
  }
  v->data[v->len++] = elem;
}

Map *new_map(void) {
  Map *map = malloc(sizeof(Map));
  map->keys = new_vec();
  map->vals = new_vec();
  return map;
}

void map_put(Map *map, char *key, void *val) {
  vec_push(map->keys, key);
  vec_push(map->vals, val);
}

void map_puti(Map *map, char *key, int val) {
  map_put(map, key, (void *)(intptr_t)val);
}

void *map_get(Map *map, char *key) {
  for (int i = map->keys->len - 1; i >= 0; i--)
    if (!strcmp(map->keys->data[i], key))
      return map->vals->data[i];
  return NULL;
}

int map_geti(Map *map, char *key, int default_) {
  for (int i = map->keys->len - 1; i >= 0; i--)
    if (!strcmp(map->keys->data[i], key))
      return (intptr_t)map->vals->data[i];
  return default_;
}

bool map_exists(Map *map, char *key) {
  for (int i = 0; i < map->keys->len; i++)
    if (!strcmp(map->keys->data[i], key))
      return true;
  return false;
}

StringBuilder *new_sb(void) {
  StringBuilder *sb = malloc(sizeof(StringBuilder));
  sb->data = malloc(8);
  sb->capacity = 8;
  sb->len = 0;
  return sb;
}

static void sb_grow(StringBuilder *sb, int len) {
  if (sb->len + len <= sb->capacity)
    return;

  while (sb->len + len > sb->capacity)
    sb->capacity *= 2;
  sb->data = realloc(sb->data, sb->capacity);
}

void sb_add(StringBuilder *sb, char s) {
  sb_grow(sb, 1);
  sb->data[sb->len++] = s;
}

void sb_append(StringBuilder *sb, char *s) { sb_append_n(sb, s, strlen(s)); }

void sb_append_n(StringBuilder *sb, char *s, int len) {
  sb_grow(sb, len);
  memcpy(sb->data + sb->len, s, len);
  sb->len += len;
}

char *sb_get(StringBuilder *sb) {
  sb_add(sb, '\0');
  return sb->data;
}

int roundup(int x, int align) { return (x + align - 1) & ~(align - 1); }

Type *ptr_to(Type *base) {
  Type *ty = calloc(1, sizeof(Type));
  ty->ty = PTR;
  ty->size = 8;
  ty->align = 8;
  ty->ptr_to = base;
  return ty;
}

Type *ary_of(Type *base, int len) {
  Type *ty = calloc(1, sizeof(Type));
  ty->ty = ARY;
  ty->size = base->size * len;
  ty->align = base->align;
  ty->ary_of = base;
  ty->len = len;
  return ty;
}
