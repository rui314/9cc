#include "9cc.h"

// Unit tests for our data structures.
//
// This kind of file is usually built as an independent executable in
// a common build config, but in 9cc I took a different approach.
// This file is just a part of the main executable. This scheme greatly
// simplifies build config.
//
// In return for the simplicity, the main executable becomes slightly
// larger, but that's not a problem for toy programs like 9cc.
// What is most important is to write tests while keeping everything simple.

static void expect(int line, int expected, int actual) {
  if (expected == actual)
    return;
  fprintf(stderr, "%d: %d expected, but got %d\n", line, expected, actual);
  exit(1);
}

static void vec_test() {
  Vector *vec = new_vec();
  expect(__LINE__, 0, vec->len);

  for (int i = 0; i < 100; i++)
    vec_push(vec, (void *)(intptr_t)i);

  expect(__LINE__, 100, vec->len);
  expect(__LINE__, 0, (intptr_t)vec->data[0]);
  expect(__LINE__, 50, (intptr_t)vec->data[50]);
  expect(__LINE__, 99, (intptr_t)vec->data[99]);
}

static void map_test() {
  Map *map = new_map();
  expect(__LINE__, 0, (intptr_t)map_get(map, "foo"));

  map_put(map, "foo", (void *)2);
  expect(__LINE__, 2, (intptr_t)map_get(map, "foo"));

  map_put(map, "bar", (void *)4);
  expect(__LINE__, 4, (intptr_t)map_get(map, "bar"));

  map_put(map, "foo", (void *)6);
  expect(__LINE__, 6, (intptr_t)map_get(map, "foo"));
}

static void sb_test() {
  StringBuilder *sb1 = new_sb();
  expect(__LINE__, 0, strlen(sb_get(sb1)));

  StringBuilder *sb2 = new_sb();
  sb_append(sb2, "foo");
  expect(__LINE__, 1, !strcmp(sb_get(sb2), "foo"));

  StringBuilder *sb3 = new_sb();
  sb_append(sb3, "foo");
  sb_append(sb3, "bar");
  expect(__LINE__, 1, !strcmp(sb_get(sb3), "foobar"));

  StringBuilder *sb4 = new_sb();
  sb_append(sb4, "foo");
  sb_append(sb4, "bar");
  sb_append(sb4, "foo");
  sb_append(sb4, "bar");
  expect(__LINE__, 1, !strcmp(sb_get(sb4), "foobarfoobar"));
}

void util_test() {
  vec_test();
  map_test();
  sb_test();
}
