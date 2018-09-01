#include "9cc.h"

void usage() {
  error("Usage: 9cc [-test] [-dump-ir1] [-dump-ir2] <file>");
}

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
  Program *prog = parse(tokens);
  sema(prog);
  gen_ir(prog);

  if (dump_ir1)
    dump_ir(prog->funcs);

  optimize(prog);
  liveness(prog);
  alloc_regs(prog);

  if (dump_ir2)
    dump_ir(prog->funcs);

  gen_x86(prog);
  return 0;
}
