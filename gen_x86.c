#include "9cc.h"

// This pass generates x86-64 assembly from IR.

char *regs[] = {"r10", "r11", "rbx", "r12", "r13", "r14", "r15"};
char *regs8[] = {"r10b", "r11b", "bl", "r12b", "r13b", "r14b", "r15b"};
char *regs32[] = {"r10d", "r11d", "ebx", "r12d", "r13d", "r14d", "r15d"};

int num_regs = sizeof(regs) / sizeof(*regs);

static char *argregs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static char *argregs8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argregs32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};

static char *backslash_escape(char *s, int len) {
  static char escaped[256] = {
          ['\b'] = 'b', ['\f'] = 'f',  ['\n'] = 'n',  ['\r'] = 'r',
          ['\t'] = 't', ['\\'] = '\\', ['\''] = '\'', ['"'] = '"',
  };

  StringBuilder *sb = new_sb();
  for (int i = 0; i < len; i++) {
    uint8_t c = s[i];
    char esc = escaped[c];
    if (esc) {
      sb_add(sb, '\\');
      sb_add(sb, esc);
    } else if (isgraph(c) || c == ' ') {
      sb_add(sb, c);
    } else {
      sb_append(sb, format("\\%03o", c));
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

static char *argreg(int r, int size) {
  if (size == 1)
    return argregs8[r];
  if (size == 4)
    return argregs32[r];
  assert(size == 8);
  return argregs[r];
}

void emit_code(Function *fn) {
  char *ret = format(".Lend%d", nlabel++);

  printf(".text\n");
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
        emit("mov %s, %s", argregs[i], regs[ir->args[i]]);

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
      if (ir->is_imm)
        emit("xor %s, %d", regs[lhs], rhs);
      else
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
    case IR_STORE_ARG:
      emit("mov [rbp-%d], %s", lhs, argreg(rhs, ir->size));
      break;
    case IR_ADD:
      if (ir->is_imm)
        emit("add %s, %d", regs[lhs], rhs);
      else
        emit("add %s, %s", regs[lhs], regs[rhs]);
      break;
    case IR_SUB:
      if (ir->is_imm)
        emit("sub %s, %d", regs[lhs], rhs);
      else
        emit("sub %s, %s", regs[lhs], regs[rhs]);
      break;
    case IR_MUL:
      if (!ir->is_imm) {
        emit("mov rax, %s", regs[rhs]);
        emit("imul %s", regs[lhs]);
        emit("mov %s, rax", regs[lhs]);
        break;
      }

      if (__builtin_popcount(rhs) == 1) {
        emit("shl %s, %d", regs[lhs], __builtin_ctz(rhs));
        break;
      }

      emit("mov rax, %d", rhs);
      emit("imul %s", regs[lhs]);
      emit("mov %s, rax", regs[lhs]);
      break;
    case IR_DIV:
      emit("mov rax, %s", regs[lhs]);
      emit("cqo");
      emit("idiv %s", regs[rhs]);
      emit("mov %s, rax", regs[lhs]);
      break;
    case IR_MOD:
      emit("mov rax, %s", regs[lhs]);
      emit("cqo");
      emit("idiv %s", regs[rhs]);
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

static void emit_data(Var *var) {
  if (var->ty->is_extern)
    return;

  if (var->data) {
    char *data = backslash_escape(var->data, var->ty->size);
    printf(".data\n");
    printf("%s:\n", var->name);
    emit(".ascii \"%s\"", data);
    return;
  }

  printf(".bss\n");
  printf("%s:\n", var->name);
  emit(".zero %d", var->ty->size);
}

void gen_x86(Program *prog) {
  printf(".intel_syntax noprefix\n");

  for (int i = 0; i < prog->gvars->len; i++)
    emit_data(prog->gvars->data[i]);

  for (int i = 0; i < prog->funcs->len; i++)
    emit_code(prog->funcs->data[i]);
}
