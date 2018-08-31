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

    if (!ir->r0 || !ir->r1) {
      vec_push(v, ir);
      continue;
    }

    assert(ir->r0 != ir->r1);

    IR *ir2 = calloc(1, sizeof(IR));
    ir2->op = IR_MOV;
    ir2->r0 = ir->r0;
    ir2->r2 = ir->r1;
    vec_push(v, ir2);

    ir->r1 = ir->r0;
    vec_push(v, ir);
  }
  bb->ir = v;
}

static void mark(IR *ir, Reg *r) {
  if (!r || r->marked)
    return;
  if (!ir->kill)
    ir->kill = new_vec();
  r->marked = true;
  vec_push(ir->kill, r);
}

static void mark_last_use(IR *ir) {
  mark(ir, ir->r0);
  mark(ir, ir->r1);
  mark(ir, ir->r2);
  mark(ir, ir->bbarg);

  if (ir->op == IR_CALL)
    for (int i = 0; i < ir->nargs; i++)
      mark(ir, ir->args[i]);
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

static void regalloc(IR *ir) {
  alloc(ir->r0);
  alloc(ir->r1);
  alloc(ir->r2);
  alloc(ir->bbarg);

  if (ir->op == IR_CALL)
    for (int i = 0; i < ir->nargs; i++)
      alloc(ir->args[i]);

  if (!ir->kill)
    return;

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
    }
  }

  for (int i = prog->funcs->len - 1; i >= 0; i--) {
    Function *fn = prog->funcs->data[i];

    for (int i = fn->bbs->len - 1; i >= 0; i--) {
      BB *bb = fn->bbs->data[i];

      for (int i = bb->ir->len - 1; i >= 0; i--) {
        IR *ir = bb->ir->data[i];
        mark_last_use(ir);
      }
    }
  }

  for (int i = 0; i < prog->funcs->len; i++) {
    Function *fn = prog->funcs->data[i];

    for (int i = 0; i < fn->bbs->len; i++) {
      BB *bb = fn->bbs->data[i];
      alloc(bb->param);

      for (int i = 0; i < bb->ir->len; i++) {
        IR *ir = bb->ir->data[i];
        regalloc(ir);
      }
    }
  }
}
