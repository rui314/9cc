#include "9cc.h"

char *regs[] = {"rbp", "r10", "r11", "rbx", "r12", "r13", "r14", "r15"};

static bool used[sizeof(regs) / sizeof(*regs)];
static int *reg_map;

static int alloc(int ir_reg) {
  if (reg_map[ir_reg] != -1) {
    int r = reg_map[ir_reg];
    assert(used[r]);
    return r;
  }

  for (int i = 0; i < sizeof(regs) / sizeof(*regs); i++) {
    if (used[i])
      continue;
    reg_map[ir_reg] = i;
    used[i] = true;
    return i;
  }
  error("register exhausted");
}

static void kill(int r) {
  assert(used[r]);
  used[r] = false;
}

static void visit(Vector *irv) {
  // r0 is a reserved register that is always mapped to rbp.
  reg_map[0] = 0;
  used[0] = true;

  for (int i = 0; i < irv->len; i++) {
    IR *ir = irv->data[i];
    IRInfo *info = get_irinfo(ir);

    switch (info->ty) {
    case IR_TY_REG:
    case IR_TY_REG_IMM:
    case IR_TY_REG_LABEL:
      ir->lhs = alloc(ir->lhs);
      break;
    case IR_TY_REG_REG:
      ir->lhs = alloc(ir->lhs);
      ir->rhs = alloc(ir->rhs);
      break;
    case IR_TY_CALL:
      ir->lhs = alloc(ir->lhs);
      for (int i = 0; i < ir->nargs; i++)
        ir->args[i] = alloc(ir->args[i]);
      break;
    }

    if (ir->op == IR_KILL) {
      kill(ir->lhs);
      ir->op = IR_NOP;
    }
  }
}

void alloc_regs(Vector *fns) {
  for (int i = 0; i < fns->len; i++) {
    Function *fn = fns->data[i];

    reg_map = malloc(sizeof(int) * fn->ir->len);
    for (int j = 0; j < fn->ir->len; j++)
      reg_map[j] = -1;
    memset(used, 0, sizeof(used));

    visit(fn->ir);
  }
}
