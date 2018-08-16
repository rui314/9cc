#include "9cc.h"

char *regs[] = {"rdi", "rsi", "r10", "r11", "r12", "r13", "r14", "r15"};

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
    used[i] = true;
    reg_map[ir_reg] = i;
    return i;
  }
  error("register exhausted");
}

static void kill(int r) {
  assert(used[r]);
  used[r] = false;
}

void alloc_regs(Vector *irv) {
  reg_map = malloc(sizeof(int) * irv->len);
  for (int i = 0; i < irv->len; i++)
    reg_map[i] = -1;

  for (int i = 0; i < irv->len; i++) {
    IR *ir = irv->data[i];

    switch (ir->op) {
    case IR_IMM:
      ir->lhs = alloc(ir->lhs);
      break;
    case IR_MOV:
    case '+':
    case '-':
    case '*':
      ir->lhs = alloc(ir->lhs);
      ir->rhs = alloc(ir->rhs);
      break;
    case IR_RETURN:
      kill(reg_map[ir->lhs]);
      break;
    case IR_KILL:
      kill(reg_map[ir->lhs]);
      ir->op = IR_NOP;
      break;
    default:
      assert(0 && "unknown operator");
    }
  }
}

