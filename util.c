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

void vec_pushi(Vector *v, int val) {
  vec_push(v, (void *)(intptr_t)val);
}

void *vec_pop(Vector *v) {
  assert(v->len);
  return v->data[--v->len];
}

void *vec_last(Vector *v) {
  assert(v->len);
  return v->data[v->len - 1];
}

bool vec_contains(Vector *v, void *elem) {
  for (int i = 0; i < v->len; i++)
    if (v->data[i] == elem)
      return true;
  return false;
}

bool vec_union1(Vector *v, void *elem) {
  if (vec_contains(v, elem))
    return false;
  vec_push(v, elem);
  return true;
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

void sb_add(StringBuilder *sb, char c) {
  sb_grow(sb, 1);
  sb->data[sb->len++] = c;
}

void sb_append(StringBuilder *sb, char *s) {
  sb_append_n(sb, s, strlen(s));
}

void sb_append_n(StringBuilder *sb, char *s, int len) {
  sb_grow(sb, len);
  memcpy(sb->data + sb->len, s, len);
  sb->len += len;
}

char *sb_get(StringBuilder *sb) {
  sb_add(sb, '\0');
  return sb->data;
}

int roundup(int x, int align) {
  return (x + align - 1) & ~(align - 1);
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

static Type *new_ty(int ty, int size) {
  Type *ret = calloc(1, sizeof(Type));
  ret->ty = ty;
  ret->size = size;
  ret->align = size;
  return ret;
}

Type *void_ty() {
  return new_ty(VOID, 0);
}

Type *bool_ty() {
  return new_ty(BOOL, 1);
}

Type *char_ty() {
  return new_ty(CHAR, 1);
}

Type *int_ty() {
  return new_ty(INT, 4);
}

Type *func_ty(Type *returning) {
  Type *ty = calloc(1, sizeof(Type));
  ty->returning = returning;
  return ty;
}

bool same_type(Type *x, Type *y) {
  if (x->ty != y->ty)
    return false;

  switch (x->ty) {
  case PTR:
    return same_type(x->ptr_to, y->ptr_to);
  case ARY:
    return x->size == y->size && same_type(x->ary_of, y->ary_of);
  case STRUCT:
  case FUNC:
    return x == y;
  default:
    return true;
  }
}
