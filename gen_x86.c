#include "9cc.h"

// This pass generates x86-64 assembly from IR.

static int nlabel;

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

static void emit(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  printf("\t");
  vprintf(fmt, ap);
  printf("\n");
}

static void emit_cmp(char *insn, IR *ir) {
  emit("cmp %s, %s", regs[ir->lhs], regs[ir->rhs]);
  emit("%s %s", insn, regs8[ir->lhs]);
  emit("movzb %s, %s", regs[ir->lhs], regs8[ir->lhs]);
}

static char *reg(int r, int size) {
  if (size == 1)
    return regs8[r];
  if (size == 4)
    return regs32[r];
  assert(size == 8);
  return regs[r];
}

void gen(Function *fn) {
  char *ret = format(".Lend%d", nlabel++);

  printf(".global %s\n", fn->name);
  printf("%s:\n", fn->name);
  emit("push rbp");
  emit("mov rbp, rsp");
  emit("sub rsp, %d", roundup(fn->stacksize, 16));
  emit("push r12");
  emit("push r13");
  emit("push r14");
  emit("push r15");

  for (int i = 0; i < fn->ir->len; i++) {
    IR *ir = fn->ir->data[i];
    int lhs = ir->lhs;
    int rhs = ir->rhs;

    switch (ir->op) {
    case IR_IMM:
      emit("mov %s, %d", regs[lhs], rhs);
      break;
    case IR_BPREL:
      emit("lea %s, [rbp-%d]", regs[lhs], rhs);
      break;
    case IR_MOV:
      emit("mov %s, %s", regs[lhs], regs[rhs]);
      break;
    case IR_RETURN:
      emit("mov rax, %s", regs[lhs]);
      emit("jmp %s", ret);
      break;
    case IR_CALL: {
      for (int i = 0; i < ir->nargs; i++)
        emit("mov %s, %s", argreg64[i], regs[ir->args[i]]);

      emit("push r10");
      emit("push r11");
      emit("mov rax, 0");
      emit("call %s", ir->name);
      emit("pop r11");
      emit("pop r10");

      emit("mov %s, rax", regs[lhs]);
      break;
    }
    case IR_LABEL:
      printf(".L%d:\n", lhs);
      break;
    case IR_LABEL_ADDR:
      emit("lea %s, %s", regs[lhs], ir->name);
      break;
    case IR_NEG:
      emit("neg %s", regs[lhs]);
      break;
    case IR_EQ:
      emit_cmp("sete", ir);
      break;
    case IR_NE:
      emit_cmp("setne", ir);
      break;
    case IR_LT:
      emit_cmp("setl", ir);
      break;
    case IR_LE:
      emit_cmp("setle", ir);
      break;
    case IR_AND:
      emit("and %s, %s", regs[lhs], regs[rhs]);
      break;
    case IR_OR:
      emit("or %s, %s", regs[lhs], regs[rhs]);
      break;
    case IR_XOR:
      emit("xor %s, %s", regs[lhs], regs[rhs]);
      break;
    case IR_SHL:
      emit("mov cl, %s", regs8[rhs]);
      emit("shl %s, cl", regs[lhs]);
      break;
    case IR_SHR:
      emit("mov cl, %s", regs8[rhs]);
      emit("shr %s, cl", regs[lhs]);
      break;
    case IR_JMP:
      emit("jmp .L%d", lhs);
      break;
    case IR_IF:
      emit("cmp %s, 0", regs[lhs]);
      emit("jne .L%d", rhs);
      break;
    case IR_UNLESS:
      emit("cmp %s, 0", regs[lhs]);
      emit("je .L%d", rhs);
      break;
    case IR_LOAD:
      emit("mov %s, [%s]", reg(lhs, ir->size), regs[rhs]);
      if (ir->size == 1)
        emit("movzb %s, %s", regs[lhs], regs8[lhs]);
      break;
    case IR_STORE:
      emit("mov [%s], %s", regs[lhs], reg(rhs, ir->size));
      break;
    case IR_STORE8_ARG:
      emit("mov [rbp-%d], %s", lhs, argreg8[rhs]);
      break;
    case IR_STORE32_ARG:
      emit("mov [rbp-%d], %s", lhs, argreg32[rhs]);
      break;
    case IR_STORE64_ARG:
      emit("mov [rbp-%d], %s", lhs, argreg64[rhs]);
      break;
    case IR_ADD:
      emit("add %s, %s", regs[lhs], regs[rhs]);
      break;
    case IR_ADD_IMM:
      emit("add %s, %d", regs[lhs], rhs);
      break;
    case IR_SUB:
      emit("sub %s, %s", regs[lhs], regs[rhs]);
      break;
    case IR_SUB_IMM:
      emit("sub %s, %d", regs[lhs], rhs);
      break;
    case IR_MUL:
      emit("mov rax, %s", regs[rhs]);
      emit("mul %s", regs[lhs]);
      emit("mov %s, rax", regs[lhs]);
      break;
    case IR_MUL_IMM:
      if (rhs < 256 && __builtin_popcount(rhs) == 1) {
        emit("shl %s, %d", regs[lhs], __builtin_ctz(rhs));
        break;
      }
      emit("mov rax, %d", rhs);
      emit("mul %s", regs[lhs]);
      emit("mov %s, rax", regs[lhs]);
      break;
    case IR_DIV:
      emit("mov rax, %s", regs[lhs]);
      emit("cqo");
      emit("div %s", regs[rhs]);
      emit("mov %s, rax", regs[lhs]);
      break;
    case IR_MOD:
      emit("mov rax, %s", regs[lhs]);
      emit("cqo");
      emit("div %s", regs[rhs]);
      emit("mov %s, rdx", regs[lhs]);
      break;
    case IR_NOP:
      break;
    default:
      assert(0 && "unknown operator");
    }
  }

  printf("%s:\n", ret);
  emit("pop r15");
  emit("pop r14");
  emit("pop r13");
  emit("pop r12");
  emit("mov rsp, rbp");
  emit("pop rbp");
  emit("ret");
}

void gen_x86(Vector *globals, Vector *fns) {
  printf(".intel_syntax noprefix\n");

  printf(".data\n");
  for (int i = 0; i < globals->len; i++) {
    Var *var = globals->data[i];
    if (var->is_extern)
      continue;
    printf("%s:\n", var->name);
    emit(".ascii \"%s\"", escape(var->data, var->len));
  }

  printf(".text\n");
  for (int i = 0; i < fns->len; i++)
    gen(fns->data[i]);
}
