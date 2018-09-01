// Optimization pass. In this pass, we promote all non-address-taken
// integer variables to register values. As a result, we may have more
// register values than the number of the physical registers, but
// that's fine. Regalloc will spill them out to memory.

#include "9cc.h"

// Rewrite
//
//  BPREL r1, <offset>
//  STORE r1, r2
//  LOAD  r3, r1
//
// to
//
//  NOP
//  r4 = r2
//  r3 = r4
static void opt(IR *ir) {
  if (ir->op == IR_BPREL) {
    Var *var = ir->var;
    if (var->address_taken || var->ty->ty != INT)
      return;

    if (!var->promoted)
      var->promoted = new_reg();

    ir->op = IR_NOP;
    ir->r0->promoted = var->promoted;
    return;
  }

  if (ir->op == IR_LOAD) {
    if (!ir->r2->promoted)
      return;
    ir->op = IR_MOV;
    ir->r2 = ir->r2->promoted;
    return;
  }

  if (ir->op == IR_STORE) {
    if (!ir->r1->promoted)
      return;
    ir->op = IR_MOV;
    ir->r0 = ir->r1->promoted;
    ir->r1 = NULL;
    return;
  }
}

void optimize(Program *prog) {
  for (int i = 0; i < prog->funcs->len; i++) {
    Function *fn = prog->funcs->data[i];
    for (int i = 0; i < fn->bbs->len; i++) {
      BB *bb = fn->bbs->data[i];
      for (int i = 0; i < bb->ir->len; i++)
        opt(bb->ir->data[i]);
    }
  }
}
