#include "9cc.h"

void gen_x86(Vector *irv) {
  for (int i = 0; i < irv->len; i++) {
    IR *ir = irv->data[i];

    switch (ir->op) {
    case IR_IMM:
      printf("  mov %s, %d\n", regs[ir->lhs], ir->rhs);
      break;
    case IR_MOV:
      printf("  mov %s, %s\n", regs[ir->lhs], regs[ir->rhs]);
      break;
    case IR_RETURN:
      printf("  mov rax, %s\n", regs[ir->lhs]);
      printf("  ret\n");
      break;
    case '+':
      printf("  add %s, %s\n", regs[ir->lhs], regs[ir->rhs]);
      break;
    case '-':
      printf("  sub %s, %s\n", regs[ir->lhs], regs[ir->rhs]);
      break;
    case IR_NOP:
      break;
    default:
      assert(0 && "unknown operator");
    }
  }
}
