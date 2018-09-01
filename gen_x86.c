#include "9cc.h"

// This pass generates x86-64 assembly from IR.

char *regs[] = {"r10", "r11", "rbx", "r12", "r13", "r14", "r15"};
char *regs8[] = {"r10b", "r11b", "bl", "r12b", "r13b", "r14b", "r15b"};
char *regs32[] = {"r10d", "r11d", "ebx", "r12d", "r13d", "r14d", "r15d"};

int num_regs = sizeof(regs) / sizeof(*regs);

static char *argregs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static char *argregs8[] = {"dil", "sil", "dl", "cl", "r8b", "r9b"};
static char *argregs32[] = {"edi", "esi", "edx", "ecx", "r8d", "r9d"};

__attribute__((format(printf, 1, 2))) static void p(char *fmt, ...);
__attribute__((format(printf, 1, 2))) static void emit(char *fmt, ...);

static void p(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  printf("\n");
}

static void emit(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  printf("\t");
  vprintf(fmt, ap);
  printf("\n");
}

static void emit_cmp(char *insn, IR *ir) {
  int r0 = ir->r0->rn;
  int r1 = ir->r1->rn;
  int r2 = ir->r2->rn;

  emit("cmp %s, %s", regs[r1], regs[r2]);
  emit("%s %s", insn, regs8[r0]);
  emit("movzb %s, %s", regs[r0], regs8[r0]);
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

static void emit_ir(IR *ir, char *ret) {
  int r0 = ir->r0 ? ir->r0->rn : 0;
  int r1 = ir->r1 ? ir->r1->rn : 0;
  int r2 = ir->r2 ? ir->r2->rn : 0;

  switch (ir->op) {
  case IR_IMM:
    emit("mov %s, %d", regs[r0], ir->imm);
    break;
  case IR_BPREL:
    emit("lea %s, [rbp%d]", regs[r0], ir->var->offset);
    break;
  case IR_MOV:
    emit("mov %s, %s", regs[r0], regs[r2]);
    break;
  case IR_RETURN:
    emit("mov rax, %s", regs[r2]);
    emit("jmp %s", ret);
    break;
  case IR_CALL:
    for (int i = 0; i < ir->nargs; i++)
      emit("mov %s, %s", argregs[i], regs[ir->args[i]->rn]);

    emit("push r10");
    emit("push r11");
    emit("mov rax, 0");
    emit("call %s", ir->name);
    emit("pop r11");
    emit("pop r10");
    emit("mov %s, rax", regs[r0]);
    break;
  case IR_LABEL_ADDR:
    emit("lea %s, %s", regs[r0], ir->name);
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
    emit("and %s, %s", regs[r0], regs[r2]);
    break;
  case IR_OR:
    emit("or %s, %s", regs[r0], regs[r2]);
    break;
  case IR_XOR:
    emit("xor %s, %s", regs[r0], regs[r2]);
    break;
  case IR_SHL:
    emit("mov cl, %s", regs8[r2]);
    emit("shl %s, cl", regs[r0]);
    break;
  case IR_SHR:
    emit("mov cl, %s", regs8[r2]);
    emit("shr %s, cl", regs[r0]);
    break;
  case IR_JMP:
    if (ir->bbarg)
      emit("mov %s, %s", regs[ir->bb1->param->rn], regs[ir->bbarg->rn]);
    emit("jmp .L%d", ir->bb1->label);
    break;
  case IR_BR:
    emit("cmp %s, 0", regs[r2]);
    emit("jne .L%d", ir->bb1->label);
    emit("jmp .L%d", ir->bb2->label);
    break;
  case IR_LOAD:
    emit("mov %s, [%s]", reg(r0, ir->size), regs[r2]);
    if (ir->size == 1)
      emit("movzb %s, %s", regs[r0], regs8[r0]);
    break;
  case IR_LOAD_SPILL:
    emit("mov %s, [rbp%d]", regs[r0], ir->var->offset);
    break;
  case IR_STORE:
    emit("mov [%s], %s", regs[r1], reg(r2, ir->size));
    break;
  case IR_STORE_ARG:
    emit("mov [rbp%d], %s", ir->var->offset, argreg(ir->imm, ir->size));
    break;
  case IR_STORE_SPILL:
    emit("mov [rbp%d], %s", ir->var->offset, regs[r1]);
    break;
  case IR_ADD:
    emit("add %s, %s", regs[r0], regs[r2]);
    break;
  case IR_SUB:
    emit("sub %s, %s", regs[r0], regs[r2]);
    break;
  case IR_MUL:
    emit("mov rax, %s", regs[r2]);
    emit("imul %s", regs[r0]);
    emit("mov %s, rax", regs[r0]);
    break;
  case IR_DIV:
    emit("mov rax, %s", regs[r0]);
    emit("cqo");
    emit("idiv %s", regs[r2]);
    emit("mov %s, rax", regs[r0]);
    break;
  case IR_MOD:
    emit("mov rax, %s", regs[r0]);
    emit("cqo");
    emit("idiv %s", regs[r2]);
    emit("mov %s, rdx", regs[r0]);
    break;
  case IR_NOP:
    break;
  default:
    assert(0 && "unknown operator");
  }
}

void emit_code(Function *fn) {
  // Assign an offset from RBP to each local variable.
  int off = 0;
  for (int i = 0; i < fn->lvars->len; i++) {
    Var *var = fn->lvars->data[i];
    off += var->ty->size;
    off = roundup(off, var->ty->align);
    var->offset = -off;
  }

  // Emit assembly
  char *ret = format(".Lend%d", nlabel++);

  p(".text");
  p(".global %s", fn->name);
  p("%s:", fn->name);
  emit("push rbp");
  emit("mov rbp, rsp");
  emit("sub rsp, %d", roundup(off, 16));
  emit("push r12");
  emit("push r13");
  emit("push r14");
  emit("push r15");

  for (int i = 0; i < fn->bbs->len; i++) {
    BB *bb = fn->bbs->data[i];
    p(".L%d:", bb->label);
    for (int i = 0; i < bb->ir->len; i++) {
      IR *ir = bb->ir->data[i];
      emit_ir(ir, ret);
    }
  }

  p("%s:", ret);
  emit("pop r15");
  emit("pop r14");
  emit("pop r13");
  emit("pop r12");
  emit("mov rsp, rbp");
  emit("pop rbp");
  emit("ret");
}

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

static void emit_data(Var *var) {
  if (var->data) {
    p(".data");
    p("%s:", var->name);
    emit(".ascii \"%s\"", backslash_escape(var->data, var->ty->size));
    return;
  }

  p(".bss");
  p("%s:", var->name);
  emit(".zero %d", var->ty->size);
}

void gen_x86(Program *prog) {
  p(".intel_syntax noprefix");

  for (int i = 0; i < prog->gvars->len; i++)
    emit_data(prog->gvars->data[i]);

  for (int i = 0; i < prog->funcs->len; i++)
    emit_code(prog->funcs->data[i]);
}
