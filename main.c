#include "9cc.h"

static char *read_file(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    perror(filename);
    exit(1);
  }

  StringBuilder *sb = new_sb();
  char buf[4096];
  for (;;) {
    int nread = fread(buf, 1, sizeof(buf), fp);
    if (nread == 0)
      break;
    sb_lappend(sb, buf, nread);
  }
  return sb_get(sb);
}

int main(int argc, char **argv) {
  if (!strcmp(argv[1], "-test")) {
    util_test();
    return 0;
  }

  bool dump_ir1 = false;
  bool dump_ir2 = false;
  char *filename;

  if (argc == 3 && !strcmp(argv[1], "-dump-ir1")) {
    dump_ir1 = true;
    filename = argv[2];
  } else if (argc == 3 && !strcmp(argv[1], "-dump-ir2")) {
    dump_ir2 = true;
    filename = argv[2];
  } else {
    if (argc != 2)
      error("Usage: 9cc [-test] [-dump-ir1] [-dump-ir2] <file>\n");
    filename = argv[1];
  }

  // Tokenize and parse.
  char *input = read_file(filename);
  Vector *tokens = tokenize(input);
  Vector *nodes = parse(tokens);
  Vector *globals = sema(nodes);
  Vector *fns = gen_ir(nodes);

  if (dump_ir1)
    dump_ir(fns);

  alloc_regs(fns);

  if (dump_ir2)
    dump_ir(fns);

  gen_x86(globals, fns);
  return 0;
}
