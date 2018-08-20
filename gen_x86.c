#include "9cc.h"

static int label;

const char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
const char *argreg32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
const char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static char *escape(char *s, int len) {
  char *buf = malloc(len * 4);
  char *p = buf;
  for (int i = 0; i < len; i++) {
    if (s[i] == '\\') {
      *p++ = '\\';
      *p++ = '\\';
    } else if (isgraph(s[i]) || s[i] == ' ') {
      *p++ = s[i];
    } else {
      sprintf(p, "\\%03o", s[i]);
      p += 4;
    }
  }
  *p = '\0';
  return buf;
}

void gen(Function *fn) {
  char *ret = format(".Lend%d", label++);

  printf(".global %s\n", fn->name);
  printf("%s:\n", fn->name);
  printf("  push rbp\n");
  printf("  mov rbp, rsp\n");
  printf("  sub rsp, %d\n", fn->stacksize);
  printf("  push r12\n");
  printf("  push r13\n");
  printf("  push r14\n");
  printf("  push r15\n");

  for (int i = 0; i < fn->ir->len; i++) {
    IR *ir = fn->ir->data[i];

    switch (ir->op) {
    case IR_IMM:
      printf("  mov %s, %d\n", regs[ir->lhs], ir->rhs);
      break;
    case IR_SUB_IMM:
      printf("  sub %s, %d\n", regs[ir->lhs], ir->rhs);
      break;
    case IR_MOV:
      printf("  mov %s, %s\n", regs[ir->lhs], regs[ir->rhs]);
      break;
    case IR_RETURN:
      printf("  mov rax, %s\n", regs[ir->lhs]);
      printf("  jmp %s\n", ret);
      break;
    case IR_CALL: {
      for (int i = 0; i < ir->nargs; i++)
        printf("  mov %s, %s\n", argreg64[i], regs[ir->args[i]]);

      printf("  push r10\n");
      printf("  push r11\n");
      printf("  mov rax, 0\n");
      printf("  call %s\n", ir->name);
      printf("  pop r11\n");
      printf("  pop r10\n");

      printf("  mov %s, rax\n", regs[ir->lhs]);
      break;
    }
    case IR_LABEL:
      printf(".L%d:\n", ir->lhs);
      break;
    case IR_LABEL_ADDR:
      printf("  lea %s, %s\n", regs[ir->lhs], ir->name);
      break;
    case IR_LT:
      printf("  cmp %s, %s\n", regs[ir->lhs], regs[ir->rhs]);
      printf("  setl %s\n", regs8[ir->lhs]);
      printf("  movzb %s, %s\n", regs[ir->lhs], regs8[ir->lhs]);
      break;
    case IR_JMP:
      printf("  jmp .L%d\n", ir->lhs);
      break;
    case IR_UNLESS:
      printf("  cmp %s, 0\n", regs[ir->lhs]);
      printf("  je .L%d\n", ir->rhs);
      break;
    case IR_LOAD8:
      printf("  mov %s, [%s]\n", regs8[ir->lhs], regs[ir->rhs]);
      printf("  movzb %s, %s\n", regs[ir->lhs], regs8[ir->lhs]);
      break;
    case IR_LOAD32:
      printf("  mov %s, [%s]\n", regs32[ir->lhs], regs[ir->rhs]);
      break;
    case IR_LOAD64:
      printf("  mov %s, [%s]\n", regs[ir->lhs], regs[ir->rhs]);
      break;
    case IR_STORE8:
      printf("  mov [%s], %s\n", regs[ir->lhs], regs8[ir->rhs]);
    case IR_STORE32:
      printf("  mov [%s], %s\n", regs[ir->lhs], regs32[ir->rhs]);
      break;
    case IR_STORE64:
      printf("  mov [%s], %s\n", regs[ir->lhs], regs[ir->rhs]);
      break;
    case IR_STORE8_ARG:
      printf("  mov [rbp-%d], %s\n", ir->lhs, argreg8[ir->rhs]);
      break;
    case IR_STORE32_ARG:
      printf("  mov [rbp-%d], %s\n", ir->lhs, argreg32[ir->rhs]);
      break;
    case IR_STORE64_ARG:
      printf("  mov [rbp-%d], %s\n", ir->lhs, argreg64[ir->rhs]);
      break;
    case IR_ADD:
      printf("  add %s, %s\n", regs[ir->lhs], regs[ir->rhs]);
      break;
    case IR_SUB:
      printf("  sub %s, %s\n", regs[ir->lhs], regs[ir->rhs]);
      break;
    case IR_MUL:
      printf("  mov rax, %s\n", regs[ir->rhs]);
      printf("  mul %s\n", regs[ir->lhs]);
      printf("  mov %s, rax\n", regs[ir->lhs]);
      break;
    case IR_DIV:
      printf("  mov rax, %s\n", regs[ir->lhs]);
      printf("  cqo\n");
      printf("  div %s\n", regs[ir->rhs]);
      printf("  mov %s, rax\n", regs[ir->lhs]);
      break;
    case IR_NOP:
      break;
    default:
      assert(0 && "unknown operator");
    }
  }

  printf("%s:\n", ret);
  printf("  pop r15\n");
  printf("  pop r14\n");
  printf("  pop r13\n");
  printf("  pop r12\n");
  printf("  mov rsp, rbp\n");
  printf("  pop rbp\n");
  printf("  ret\n");
}

void gen_x86(Vector *globals, Vector *fns) {
  printf(".intel_syntax noprefix\n");

  printf(".data\n");
  for (int i = 0; i < globals->len; i++) {
    Var *var = globals->data[i];
    printf("%s:\n", var->name);
    printf("  .ascii \"%s\"\n", escape(var->data, var->len));
  }

  printf(".text\n");
  for (int i = 0; i < fns->len; i++)
    gen(fns->data[i]);
}
