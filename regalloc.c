// Linear scan register allocator.
//
// Before this pass, it is assumed that we have infinite number of
// registers. This pass maps them to finite number of registers.
// Here is the algorithm:
//
// First, we find the definition and the last use for each register.
// A register is considered "live" in the range. At the definition of
// some register R, if all physical registers are already allocated,
// one of them (including R itself) needs to be spilled to the stack.
// As long as one register is spilled, the algorithm is logically
// correct. As a heuristic, we spill a register whose last use is
// furthest.
//
// We then insert load and store instructions for spilled registesr.
// The last register (num_regs-1'th register) is reserved for that
// purpose.

#include "9cc.h"

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

static void set_last_use(Reg *r, int ic) {
  if (r && r->last_use < ic)
    r->last_use = ic;
}

static Vector *collect_regs(Function *fn) {
  Vector *v = new_vec();
  int ic = 1; // instruction counter

  for (int i = 0; i < fn->bbs->len; i++) {
    BB *bb = fn->bbs->data[i];

    if (bb->param) {
      bb->param->def = ic;
      vec_push(v, bb->param);
    }

    for (int i = 0; i < bb->ir->len; i++, ic++) {
      IR *ir = bb->ir->data[i];

      if (ir->r0 && !ir->r0->def) {
        ir->r0->def = ic;
        vec_push(v, ir->r0);
      }

      set_last_use(ir->r1, ic);
      set_last_use(ir->r2, ic);
      set_last_use(ir->bbarg, ic);

      if (ir->op == IR_CALL)
        for (int i = 0; i < ir->nargs; i++)
          set_last_use(ir->args[i], ic);
    }

    for (int i = 0; i < bb->out_regs->len; i++) {
      Reg *r = bb->out_regs->data[i];
      set_last_use(r, ic);
    }
  }

  return v;
}

static int choose_to_spill(Reg **used) {
  int k = 0;
  for (int i = 1; i < num_regs; i++)
    if (used[k]->last_use < used[i]->last_use)
      k = i;
  return k;
}

// Allocate registers.
static void scan(Vector *regs) {
  Reg **used = calloc(num_regs, sizeof(Reg *));

  for (int i = 0; i < regs->len; i++) {
    Reg *r = regs->data[i];

    // Find an unused slot.
    bool found = false;
    for (int i = 0; i < num_regs - 1; i++) {
      if (used[i] && r->def < used[i]->last_use)
        continue;
      r->rn = i;
      used[i] = r;
      found = true;
      break;
    }

    if (found)
      continue;

    // Choose a register to spill and mark it as "spilled".
    used[num_regs - 1] = r;
    int k = choose_to_spill(used);

    r->rn = k;
    used[k]->rn = num_regs - 1;
    used[k]->spill = true;
    used[k] = r;
  }
}

static void spill_store(Vector *v, IR *ir) {
  Reg *r = ir->r0;
  if (!r || !r->spill)
    return;

  IR *ir2 = calloc(1, sizeof(IR));
  ir2->op = IR_STORE_SPILL;
  ir2->r1 = r;
  ir2->var = r->var;
  vec_push(v, ir2);
}

static void spill_load(Vector *v, IR *ir, Reg *r) {
  if (!r || !r->spill)
    return;

  IR *ir2 = calloc(1, sizeof(IR));
  ir2->op = IR_LOAD_SPILL;
  ir2->r0 = r;
  ir2->var = r->var;
  vec_push(v, ir2);
}

static void emit_spill_code(BB *bb) {
  Vector *v = new_vec();

  for (int i = 0; i < bb->ir->len; i++) {
    IR *ir = bb->ir->data[i];

    spill_load(v, ir, ir->r1);
    spill_load(v, ir, ir->r2);
    spill_load(v, ir, ir->bbarg);
    vec_push(v, ir);
    spill_store(v, ir);
  }
  bb->ir = v;
}

void alloc_regs(Program *prog) {
  for (int i = 0; i < prog->funcs->len; i++) {
    Function *fn = prog->funcs->data[i];

    // Convert SSA to x86-ish two-address form.
    for (int i = 0; i < fn->bbs->len; i++) {
      BB *bb = fn->bbs->data[i];
      three_to_two(bb);
    }

    // Allocate registers and decide which registers to spill.
    Vector *regs = collect_regs(fn);
    scan(regs);

    // Reserve a stack area for spilled registers.
    for (int i = 0; i < regs->len; i++) {
      Reg *r = regs->data[i];
      if (!r->spill)
        continue;

      Var *var = calloc(1, sizeof(Var));
      var->ty = ptr_to(int_ty());
      var->is_local = true;
      var->name = "spill";

      r->var = var;
      vec_push(fn->lvars, var);
    }

    // Convert accesses to spilled registers to loads and stores.
    for (int i = 0; i < fn->bbs->len; i++) {
      BB *bb = fn->bbs->data[i];
      emit_spill_code(bb);
    }
  }
}
