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

#include "9cc.h"

static bool *used;

// Rewrite `A = B op C` to `A = B; A = A op C`.
static void three_to_two(BB *bb) {
  Vector *v = new_vec();

  for (int i = 0; i < bb->ir->len; i++) {
    IR *ir = bb->ir->data[i];

    if (!ir->r0 || !ir->r1 || ir->r0 == ir->r1) {
      vec_push(v, ir);
      continue;
    }

    IR *ir2 = calloc(1, sizeof(IR));
    ir2->op = IR_MOV;
    ir2->kill = new_vec();
    ir2->r0 = ir->r0;
    ir2->r2 = ir->r1;
    vec_push(v, ir2);

    ir->r1 = ir->r0;
    vec_push(v, ir);
  }
  bb->ir = v;
}

static void alloc(Reg *r) {
  if (!r || r->rn != -1)
    return;

  for (int i = 0; i < num_regs; i++) {
    if (used[i])
      continue;
    used[i] = true;
    r->rn = i;
    return;
  }
  error("register exhausted");
}

static void visit(IR *ir) {
  alloc(ir->r0);
  alloc(ir->r1);
  alloc(ir->r2);

  if (ir->op == IR_CALL)
    for (int i = 0; i < ir->nargs; i++)
      alloc(ir->args[i]);

  for (int i = 0; i < ir->kill->len; i++) {
    Reg *r = ir->kill->data[i];
    assert(r->rn != -1);
    used[r->rn] = false;
  }
}

void alloc_regs(Program *prog) {
  used = calloc(1, num_regs);

  for (int i = 0; i < prog->funcs->len; i++) {
    Function *fn = prog->funcs->data[i];
    for (int i = 0; i < fn->bbs->len; i++) {
      BB *bb = fn->bbs->data[i];
      three_to_two(bb);

      for (int i = 0; i < bb->ir->len; i++) {
        IR *ir = bb->ir->data[i];
        visit(ir);
      }
    }
  }
}
