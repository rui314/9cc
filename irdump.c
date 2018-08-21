#include "9cc.h"

IRInfo irinfo[] = {
        [IR_ADD] = {"ADD", IR_TY_REG_REG},
        [IR_CALL] = {"CALL", IR_TY_CALL},
        [IR_DIV] = {"DIV", IR_TY_REG_REG},
        [IR_IMM] = {"IMM", IR_TY_REG_IMM},
        [IR_JMP] = {"JMP", IR_TY_JMP},
        [IR_KILL] = {"KILL", IR_TY_REG},
        [IR_LABEL] = {"", IR_TY_LABEL},
        [IR_LABEL_ADDR] = {"LABEL_ADDR", IR_TY_LABEL_ADDR},
        [IR_EQ] = {"EQ", IR_TY_REG_REG},
        [IR_NE] = {"NE", IR_TY_REG_REG},
        [IR_LT] = {"LT", IR_TY_REG_REG},
        [IR_LOAD8] = {"LOAD8", IR_TY_REG_REG},
        [IR_LOAD32] = {"LOAD32", IR_TY_REG_REG},
        [IR_LOAD64] = {"LOAD64", IR_TY_REG_REG},
        [IR_MOV] = {"MOV", IR_TY_REG_REG},
        [IR_MUL] = {"MUL", IR_TY_REG_REG},
        [IR_NOP] = {"NOP", IR_TY_NOARG},
        [IR_RETURN] = {"RET", IR_TY_REG},
        [IR_STORE8] = {"STORE8", IR_TY_REG_REG},
        [IR_STORE32] = {"STORE32", IR_TY_REG_REG},
        [IR_STORE64] = {"STORE64", IR_TY_REG_REG},
        [IR_STORE8_ARG] = {"STORE8_ARG", IR_TY_IMM_IMM},
        [IR_STORE32_ARG] = {"STORE32_ARG", IR_TY_IMM_IMM},
        [IR_STORE64_ARG] = {"STORE64_ARG", IR_TY_IMM_IMM},
        [IR_SUB] = {"SUB", IR_TY_REG_REG},
        [IR_BPREL] = {"BPREL", IR_TY_REG_IMM},
        [IR_IF] = {"IF", IR_TY_REG_LABEL},
        [IR_UNLESS] = {"UNLESS", IR_TY_REG_LABEL},
};

static char *tostr(IR *ir) {
  IRInfo info = irinfo[ir->op];

  switch (info.ty) {
  case IR_TY_LABEL:
    return format(".L%d:", ir->lhs);
  case IR_TY_LABEL_ADDR:
    return format("  %s r%d, %s", info.name, ir->lhs, ir->name);
  case IR_TY_IMM:
    return format("  %s %d", info.name, ir->lhs);
  case IR_TY_REG:
    return format("  %s r%d", info.name, ir->lhs);
  case IR_TY_JMP:
    return format("  %s .L%d", info.name, ir->lhs);
  case IR_TY_REG_REG:
    return format("  %s r%d, r%d", info.name, ir->lhs, ir->rhs);
  case IR_TY_REG_IMM:
    return format("  %s r%d, %d", info.name, ir->lhs, ir->rhs);
  case IR_TY_IMM_IMM:
    return format("  %s %d, %d", info.name, ir->lhs, ir->rhs);
  case IR_TY_REG_LABEL:
    return format("  %s r%d, .L%d", info.name, ir->lhs, ir->rhs);
  case IR_TY_CALL: {
    StringBuilder *sb = new_sb();
    sb_append(sb, format("  r%d = %s(", ir->lhs, ir->name));
    for (int i = 0; i < ir->nargs; i++) {
      if (i != 0)
        sb_append(sb, ", ");
      sb_append(sb, format("r%d", ir->args[i]));
    }
    sb_append(sb, ")");
    return sb_get(sb);
  }
  default:
    assert(info.ty == IR_TY_NOARG);
    return format("  %s", info.name);
  }
}

void dump_ir(Vector *irv) {
  for (int i = 0; i < irv->len; i++) {
    Function *fn = irv->data[i];
    fprintf(stderr, "%s():\n", fn->name);
    for (int j = 0; j < fn->ir->len; j++)
      fprintf(stderr, "%s\n", tostr(fn->ir->data[j]));
  }
}
