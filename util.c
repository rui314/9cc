#include "9cc.h"

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

void *map_get(Map *map, char *key) {
  for (int i = map->keys->len - 1; i >= 0; i--)
    if (!strcmp(map->keys->data[i], key))
      return map->vals->data[i];
  return NULL;
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

void sb_append(StringBuilder *sb, char *s) { sb_lappend(sb, s, strlen(s)); }

void sb_lappend(StringBuilder *sb, char *s, int len) {
  sb_grow(sb, len);
  memcpy(sb->data + sb->len, s, len);
  sb->len += len;
}

char *sb_get(StringBuilder *sb) {
  sb_add(sb, '\0');
  return sb->data;
}

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

Type *struct_of(Vector *members) {
  Type *ty = calloc(1, sizeof(Type));
  ty->ty = STRUCT;
  ty->members = new_vec();

  int off = 0;
  for (int i = 0; i < members->len; i++) {
    Node *node = members->data[i];
    assert(node->op == ND_VARDEF);

    Type *t = node->ty;
    off = roundup(off, t->align);
    t->offset = off;
    off += t->size;

    if (ty->align < node->ty->align)
      ty->align = node->ty->align;
  }
  ty->size = roundup(off, ty->align);
  return ty;
}

int roundup(int x, int align) { return (x + align - 1) & ~(align - 1); }
