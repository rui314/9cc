#include "9cc.h"

// Register allocator.
//
// Before this pass, it is assumed that we have infinite number of
// registers. This pass maps them to a finite number of registers.
// We actually have only 7 registers.
//
// We allocate registers only within a single expression. In other
// words, there are no registers that live beyond semicolons.
// This design choice simplifies the implementation a lot, since
// practically we don't have to think about the case in which
// registers are exhausted and need to be spilled to memory.

static bool *used;
static int reg_map[8192];
static int reg_map_sz = sizeof(reg_map) / sizeof(*reg_map);

static int alloc(int ir_reg) {
  if (reg_map_sz <= ir_reg)
    error("program too big");

  if (reg_map[ir_reg] != -1) {
    int r = reg_map[ir_reg];
    assert(used[r]);
    return r;
  }

  for (int i = 0; i < nregs; i++) {
    if (used[i])
      continue;
    reg_map[ir_reg] = i;
    used[i] = true;
    return i;
  }
  error("register exhausted");
}

static void visit(Vector *irv) {
  for (int i = 0; i < irv->len; i++) {
    IR *ir = irv->data[i];

    switch (irinfo[ir->op].ty) {
    case IR_TY_BINARY:
      ir->lhs = alloc(ir->lhs);
      if (!ir->is_imm)
        ir->rhs = alloc(ir->rhs);
      break;
    case IR_TY_REG:
    case IR_TY_REG_IMM:
    case IR_TY_REG_LABEL:
    case IR_TY_LABEL_ADDR:
      ir->lhs = alloc(ir->lhs);
      break;
    case IR_TY_MEM:
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
      assert(used[ir->lhs]);
      used[ir->lhs] = false;
      ir->op = IR_NOP;
    }
  }
}

void alloc_regs(Vector *fns) {
  used = calloc(1, nregs);
  for (int i = 0; i < reg_map_sz; i++)
    reg_map[i] = -1;

  for (int i = 0; i < fns->len; i++) {
    Function *fn = fns->data[i];
    visit(fn->ir);
  }
}
