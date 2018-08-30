#include "9cc.h"

static char *tostr_call(IR *ir) {
  StringBuilder *sb = new_sb();
  sb_append(sb, format("r%d = %s(", ir->lhs, ir->name));
  for (int i = 0; i < ir->nargs; i++) {
    if (i != 0)
      sb_append(sb, ", ");
    sb_append(sb, format("r%d", ir->args[i]));
  }
  sb_append(sb, ")");
  return sb_get(sb);
}

static char *tostr(IR *ir) {
  int lhs = ir->lhs;
  int rhs = ir->rhs;

  switch (ir->op) {
  case IR_ADD:
    return format("ADD r%d, r%d", lhs, rhs);
  case IR_CALL:
    return tostr_call(ir);
  case IR_DIV:
    return format("DIV r%d, r%d", lhs, rhs);
  case IR_IMM:
    return format("r%d = %d", lhs, ir->imm);
  case IR_JMP:
    return format("JMP .L%d", ir->bb1->label);
  case IR_LABEL_ADDR:
    return format("r%d = .L%d", lhs, ir->label);
  case IR_EQ:
    return format("EQ r%d, r%d", lhs, rhs);
  case IR_NE:
    return format("NE r%d, r%d", lhs, rhs);
  case IR_LE:
    return format("LE r%d, r%d", lhs, rhs);
  case IR_LT:
    return format("LT r%d, r%d", lhs, rhs);
  case IR_AND:
    return format("AND r%d, r%d", lhs, rhs);
  case IR_OR:
    return format("OR r%d, r%d", lhs, rhs);
  case IR_XOR:
    return format("XOR r%d, r%d", lhs, rhs);
  case IR_SHL:
    return format("SHL r%d, r%d", lhs, rhs);
  case IR_SHR:
    return format("SHR r%d, r%d", lhs, rhs);
  case IR_LOAD:
    return format("LOAD%d r%d, r%d", ir->size, lhs, rhs);
  case IR_MOD:
    return format("MOD r%d, r%d", lhs, rhs);
  case IR_MOV:
    return format("MOV r%d, r%d", lhs, rhs);
  case IR_MUL:
    return format("MUL r%d, r%d", lhs, rhs);
  case IR_NOP:
    return "NOP";
  case IR_RETURN:
    return format("RET r%d", lhs);
  case IR_STORE:
    return format("STORE%d r%d, r%d", ir->size, lhs, rhs);
  case IR_STORE_ARG:
    return format("STORE_ARG%d %d, %d", ir->size, ir->imm, ir->imm2);
  case IR_SUB:
    return format("SUB r%d, r%d", lhs, rhs);
  case IR_BPREL:
    return format("BPREL r%d, %d", lhs, ir->imm);
  case IR_BR:
    return format("BR r%d, .L%d, L%d", lhs, ir->bb1->label, ir->bb2->label);
  default:
    assert(0 && "unknown op");
  }
}

void dump_ir(Vector *irv) {
  for (int i = 0; i < irv->len; i++) {
    Function *fn = irv->data[i];
    fprintf(stderr, "%s:\n", fn->name);

    for (int i = 0; i < fn->bbs->len; i++) {
      BB *bb = fn->bbs->data[i];
      fprintf(stderr, ".L%d:\n", bb->label);

      for (int i = 0; i < bb->ir->len; i++) {
        IR *ir = bb->ir->data[i];
        fprintf(stderr, "\t%s\n", tostr(ir));
      }
    }
  }
}
