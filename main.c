#include "9cc.h"

char *filename;

static char *read_file(char *filename) {
  FILE *fp = stdin;
  if (strcmp(filename, "-")) {
    fp = fopen(filename, "r");
    if (!fp) {
      perror(filename);
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

  if (sb->data[sb->len-1] != '\n')
    sb_add(sb, '\n');
  return sb_get(sb);
}

void usage() { error("Usage: 9cc [-test] [-dump-ir1] [-dump-ir2] <file>"); }

int main(int argc, char **argv) {
  if (argc == 1)
    usage();

  if (argc == 2 && !strcmp(argv[1], "-test")) {
    util_test();
    return 0;
  }

  bool dump_ir1 = false;
  bool dump_ir2 = false;

  if (argc == 3 && !strcmp(argv[1], "-dump-ir1")) {
    dump_ir1 = true;
    filename = argv[2];
  } else if (argc == 3 && !strcmp(argv[1], "-dump-ir2")) {
    dump_ir2 = true;
    filename = argv[2];
  } else {
    if (argc != 2)
      usage();
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
