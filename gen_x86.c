#include "9cc.h"

// This pass generates x86-64 assembly from IR.

static int label;

const char *argreg8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
const char *argreg32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};
const char *argreg64[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

static char *escape(char *s, int len) {
  static char escaped[256] = {
          ['\b'] = 'b', ['\f'] = 'f',  ['\n'] = 'n',  ['\r'] = 'r',
          ['\t'] = 't', ['\\'] = '\\', ['\''] = '\'', ['"'] = '"',
  };

  StringBuilder *sb = new_sb();
  for (int i = 0; i < len; i++) {
    char esc = escaped[(unsigned)s[i]];
    if (esc) {
      sb_add(sb, '\\');
      sb_add(sb, esc);
    } else if (isgraph(s[i]) || s[i] == ' ') {
      sb_add(sb, s[i]);
    } else {
      sb_append(sb, format("\\%03o", s[i]));
    }
  }
  return sb_get(sb);
}

void emit_cmp(IR *ir, char *insn) {
  printf("  cmp %s, %s\n", regs[ir->lhs], regs[ir->rhs]);
  printf("  %s %s\n", insn, regs8[ir->lhs]);
  printf("  movzb %s, %s\n", regs[ir->lhs], regs8[ir->lhs]);
}

void gen(Function *fn) {
  char *ret = format(".Lend%d", label++);

  printf(".global %s\n", fn->name);
  printf("%s:\n", fn->name);
  printf("  push rbp\n");
  printf("  mov rbp, rsp\n");
  printf("  sub rsp, %d\n", roundup(fn->stacksize, 16));
  printf("  push r12\n");
  printf("  push r13\n");
  printf("  push r14\n");
  printf("  push r15\n");

  for (int i = 0; i < fn->ir->len; i++) {
    IR *ir = fn->ir->data[i];
    int lhs = ir->lhs;
    int rhs = ir->rhs;

    switch (ir->op) {
    case IR_IMM:
      printf("  mov %s, %d\n", regs[lhs], rhs);
      break;
    case IR_BPREL:
      printf("  lea %s, [rbp-%d]\n", regs[lhs], rhs);
      break;
    case IR_MOV:
      printf("  mov %s, %s\n", regs[lhs], regs[rhs]);
      break;
    case IR_RETURN:
      printf("  mov rax, %s\n", regs[lhs]);
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

      printf("  mov %s, rax\n", regs[lhs]);
      break;
    }
    case IR_LABEL:
      printf(".L%d:\n", lhs);
      break;
    case IR_LABEL_ADDR:
      printf("  lea %s, %s\n", regs[lhs], ir->name);
      break;
    case IR_NEG:
      printf("  neg %s\n", regs[lhs]);
      break;
    case IR_EQ:
      emit_cmp(ir, "sete");
      break;
    case IR_NE:
      emit_cmp(ir, "setne");
      break;
    case IR_LT:
      emit_cmp(ir, "setl");
      break;
    case IR_LE:
      emit_cmp(ir, "setle");
      break;
    case IR_AND:
      printf("  and %s, %s\n", regs[lhs], regs[rhs]);
      break;
    case IR_OR:
      printf("  or %s, %s\n", regs[lhs], regs[rhs]);
      break;
    case IR_XOR:
      printf("  xor %s, %s\n", regs[lhs], regs[rhs]);
      break;
    case IR_SHL:
      printf("  mov cl, %s\n", regs8[rhs]);
      printf("  shl %s, cl\n", regs[lhs]);
      break;
    case IR_SHR:
      printf("  mov cl, %s\n", regs8[rhs]);
      printf("  shr %s, cl\n", regs[lhs]);
      break;
    case IR_JMP:
      printf("  jmp .L%d\n", lhs);
      break;
    case IR_IF:
      printf("  cmp %s, 0\n", regs[lhs]);
      printf("  jne .L%d\n", rhs);
      break;
    case IR_UNLESS:
      printf("  cmp %s, 0\n", regs[lhs]);
      printf("  je .L%d\n", rhs);
      break;
    case IR_LOAD8:
      printf("  mov %s, [%s]\n", regs8[lhs], regs[rhs]);
      printf("  movzb %s, %s\n", regs[lhs], regs8[lhs]);
      break;
    case IR_LOAD32:
      printf("  mov %s, [%s]\n", regs32[lhs], regs[rhs]);
      break;
    case IR_LOAD64:
      printf("  mov %s, [%s]\n", regs[lhs], regs[rhs]);
      break;
    case IR_STORE8:
      printf("  mov [%s], %s\n", regs[lhs], regs8[rhs]);
    case IR_STORE32:
      printf("  mov [%s], %s\n", regs[lhs], regs32[rhs]);
      break;
    case IR_STORE64:
      printf("  mov [%s], %s\n", regs[lhs], regs[rhs]);
      break;
    case IR_STORE8_ARG:
      printf("  mov [rbp-%d], %s\n", lhs, argreg8[rhs]);
      break;
    case IR_STORE32_ARG:
      printf("  mov [rbp-%d], %s\n", lhs, argreg32[rhs]);
      break;
    case IR_STORE64_ARG:
      printf("  mov [rbp-%d], %s\n", lhs, argreg64[rhs]);
      break;
    case IR_ADD:
      printf("  add %s, %s\n", regs[lhs], regs[rhs]);
      break;
    case IR_ADD_IMM:
      printf("  add %s, %d\n", regs[lhs], rhs);
      break;
    case IR_SUB:
      printf("  sub %s, %s\n", regs[lhs], regs[rhs]);
      break;
    case IR_SUB_IMM:
      printf("  sub %s, %d\n", regs[lhs], rhs);
      break;
    case IR_MUL:
      printf("  mov rax, %s\n", regs[rhs]);
      printf("  mul %s\n", regs[lhs]);
      printf("  mov %s, rax\n", regs[lhs]);
      break;
    case IR_MUL_IMM:
      printf("  mov rax, %d\n", rhs);
      printf("  mul %s\n", regs[lhs]);
      printf("  mov %s, rax\n", regs[lhs]);
      break;
    case IR_DIV:
      printf("  mov rax, %s\n", regs[lhs]);
      printf("  cqo\n");
      printf("  div %s\n", regs[rhs]);
      printf("  mov %s, rax\n", regs[lhs]);
      break;
    case IR_MOD:
      printf("  mov rax, %s\n", regs[lhs]);
      printf("  cqo\n");
      printf("  div %s\n", regs[rhs]);
      printf("  mov %s, rdx\n", regs[lhs]);
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
    if (var->is_extern)
      continue;
    printf("%s:\n", var->name);
    printf("  .ascii \"%s\"\n", escape(var->data, var->len));
  }

  printf(".text\n");
  for (int i = 0; i < fns->len; i++)
    gen(fns->data[i]);
}
