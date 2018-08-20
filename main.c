#include "9cc.h"

int main(int argc, char **argv) {
  if (!strcmp(argv[1], "-test")) {
    util_test();
    return 0;
  }

  char *input;
  bool dump_ir1 = false;
  bool dump_ir2 = false;

  if (argc == 3 && !strcmp(argv[1], "-dump-ir1")) {
    dump_ir1 = true;
    input = argv[2];
  } else if (argc == 3 && !strcmp(argv[1], "-dump-ir2")) {
    dump_ir2 = true;
    input = argv[2];
  } else {
    if (argc != 2)
      error("Usage: 9cc [-test] [-dump-ir1] [-dump-ir2] <code>\n");
    input = argv[1];
  }

  // Tokenize and parse.
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
