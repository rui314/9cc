// Liveness analysis.

#include "9cc.h"

// Fill bb->succ and bb->pred.
static void add_edges(BB *bb) {
  if (bb->succ->len > 0)
    return;
  assert(bb->ir->len);

  IR *ir = bb->ir->data[bb->ir->len - 1];

  if (ir->bb1) {
    vec_push(bb->succ, ir->bb1);
    vec_push(ir->bb1->pred, bb);
    add_edges(ir->bb1);
  }

  if (ir->bb2) {
    vec_push(bb->succ, ir->bb2);
    vec_push(ir->bb2->pred, bb);
    add_edges(ir->bb2);
  }
}

// Initializes bb->def_regs.
static void set_def_regs(BB *bb) {
  if (bb->param)
    vec_union1(bb->def_regs, bb->param);

  for (int i = 0; i < bb->ir->len; i++) {
    IR *ir = bb->ir->data[i];
    if (ir->r0)
      vec_union1(bb->def_regs, ir->r0);
  }
}

// Back-propagate r in the call flow graph.
static void propagate(BB *bb, Reg *r) {
  if (!r || vec_contains(bb->def_regs, r))
    return;

  if (!vec_union1(bb->in_regs, r))
    return;

  for (int i = 0; i < bb->pred->len; i++) {
    BB *pred = bb->pred->data[i];
    if (vec_union1(pred->out_regs, r))
      propagate(pred, r);
  }
}

// Initializes bb->in_regs and bb->out_regs.
static void visit(BB *bb, IR *ir) {
  propagate(bb, ir->r1);
  propagate(bb, ir->r2);
  propagate(bb, ir->bbarg);

  if (ir->op == IR_CALL)
    for (int i = 0; i < ir->nargs; i++)
      propagate(bb, ir->args[i]);
}

void liveness(Program *prog) {
  for (int i = 0; i < prog->funcs->len; i++) {
    Function *fn = prog->funcs->data[i];
    add_edges(fn->bbs->data[0]);

    for (int i = 0; i < fn->bbs->len; i++) {
      BB *bb = fn->bbs->data[i];
      set_def_regs(bb);

      for (int i = 0; i < bb->ir->len; i++) {
	IR *ir = bb->ir->data[i];
	visit(bb, ir);
      }
    }

    // Incoming registers of the entry BB correspond to
    // uninitialized variables in a program.
    // Add dummy definitions to make later analysis easy.
    BB *ent = fn->bbs->data[0];
    for (int i = 0; i < ent->in_regs->len; i++) {
      Reg *r = ent->in_regs->data[i];
      IR *ir = calloc(1, sizeof(IR));
      ir->op = IR_MOV;
      ir->r0 = r;
      ir->imm = 0;
      vec_push(ent->ir, ir);
      vec_push(ent->def_regs, r);
    }
    ent->in_regs = new_vec();
  }
}
