#include "9cc.h"

void usage() { error("Usage: 9cc [-test] [-dump-ir1] [-dump-ir2] <file>"); }

int main(int argc, char **argv) {
  if (argc == 1)
    usage();

  if (argc == 2 && !strcmp(argv[1], "-test")) {
    util_test();
    return 0;
  }

  char *path;
  bool dump_ir1 = false;
  bool dump_ir2 = false;

  if (argc == 3 && !strcmp(argv[1], "-dump-ir1")) {
    dump_ir1 = true;
    path = argv[2];
  } else if (argc == 3 && !strcmp(argv[1], "-dump-ir2")) {
    dump_ir2 = true;
    path = argv[2];
  } else {
    if (argc != 2)
      usage();
    path = argv[1];
  }

  // Tokenize and parse.
  Vector *tokens = tokenize(path, true);
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
