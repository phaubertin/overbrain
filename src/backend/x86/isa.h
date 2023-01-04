/*
 * Copyright (C) 2022 Philippe Aubertin.
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of other contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BFC_X86_ISA_H
#define BFC_X86_ISA_H

#include <stdbool.h>
#include <stdint.h>
#include "../common/symbols.h"

typedef enum {
    X86_REG_AL = 0,
    X86_REG_CL = 1,
    X86_REG_DL = 2,
    X86_REG_BL = 3,
    X86_REG_SPL = 4,
    X86_REG_BPL = 5,
    X86_REG_SIL = 6,
    X86_REG_DIL = 7,
    X86_REG_R8B = 8,
    X86_REG_R9B = 9,
    X86_REG_R10B = 10,
    X86_REG_R11B = 11,
    X86_REG_R12B = 12,
    X86_REG_R13B = 13,
    X86_REG_R14B = 14,
    X86_REG_R15B = 15
} x86_reg8;

extern char *x86_reg8_names[];

typedef enum {
    X86_REG_EAX = 0,
    X86_REG_ECX = 1,
    X86_REG_EDX = 2,
    X86_REG_EBX = 3,
    X86_REG_ESP = 4,
    X86_REG_EBP = 5,
    X86_REG_ESI = 6,
    X86_REG_EDI = 7,
    X86_REG_R8D = 8,
    X86_REG_R9D = 9,
    X86_REG_R10D = 10,
    X86_REG_R11D = 11,
    X86_REG_R12D = 12,
    X86_REG_R13D = 13,
    X86_REG_R14D = 14,
    X86_REG_R15D = 15
} x86_reg32;

extern char *x86_reg32_names[];

typedef enum {
    X86_REG_RAX = 0,
    X86_REG_RCX = 1,
    X86_REG_RDX = 2,
    X86_REG_RBX = 3,
    X86_REG_RSP = 4,
    X86_REG_RBP = 5,
    X86_REG_RSI = 6,
    X86_REG_RDI = 7,
    X86_REG_R8 = 8,
    X86_REG_R9 = 9,
    X86_REG_R10 = 10,
    X86_REG_R11 = 11,
    X86_REG_R12 = 12,
    X86_REG_R13 = 13,
    X86_REG_R14 = 14,
    X86_REG_R15 = 15
} x86_reg64;

extern char *x86_reg64_names[];

typedef enum {
    X86_INSTR_ALIGN,
    X86_INSTR_ADD,
    X86_INSTR_AND,
    X86_INSTR_CALL,
    X86_INSTR_CMP,
    X86_INSTR_JL,
    X86_INSTR_JMP,
    X86_INSTR_JNS,
    X86_INSTR_JNZ,
    X86_INSTR_JZ,
    X86_INSTR_LABEL,
    X86_INSTR_MOV,
    X86_INSTR_MOVZX,
    X86_INSTR_OR,
    X86_INSTR_POP,
    X86_INSTR_PUSH,
    X86_INSTR_RET
} x86_instr_op;

typedef enum {
    X86_OPERAND_EXTERN,
    X86_OPERAND_IMM8,
    X86_OPERAND_IMM32,
    X86_OPERAND_LABEL,
    X86_OPERAND_LOCAL,
    X86_OPERAND_MEM8_REG,
    X86_OPERAND_MEM64_EXTERN,
    X86_OPERAND_MEM64_IMM,
    X86_OPERAND_MEM64_LOCAL,
    X86_OPERAND_MEM64_REL,
    X86_OPERAND_REG8,
    X86_OPERAND_REG32,
    X86_OPERAND_REG64
} x86_operand_type;

struct x86_operand {
    x86_operand_type type;
    int r1;
    int r2;
    int n;
};

struct x86_operand *x86_operand_new_extern(extern_symbol symbol);

struct x86_operand *x86_operand_new_imm8(int n);

struct x86_operand *x86_operand_new_imm32(int n);

struct x86_operand *x86_operand_new_label(int n);

struct x86_operand *x86_operand_new_local(local_symbol symbol);

struct x86_operand *x86_operand_new_mem8_reg(x86_reg64 r1, x86_reg64 r2, int n);

struct x86_operand *x86_operand_new_mem64_extern(extern_symbol symbol);

struct x86_operand *x86_operand_new_mem64_imm(int n);

struct x86_operand *x86_operand_new_mem64_local(local_symbol symbol);

struct x86_operand *x86_operand_new_mem64_rel(int n);

struct x86_operand *x86_operand_new_reg8(x86_reg8 r);

struct x86_operand *x86_operand_new_reg32(x86_reg32 r);

struct x86_operand *x86_operand_new_reg64(x86_reg64 r);

void x86_operand_free(struct x86_operand *oper);

bool x86_operand_is_64bit(const struct x86_operand *oper);

bool x86_operand_is_register(const struct x86_operand *oper);

bool x86_operand_is_memory(const struct x86_operand *oper);

bool x86_operand_is_immediate(const struct x86_operand *oper);

struct x86_instr {
    x86_instr_op op;
    int n;
    struct x86_operand *dst;
    struct x86_operand *src;
    struct x86_instr *next;
};

struct x86_instr *x86_instr_new_align(int n);

struct x86_instr *x86_instr_new_add(struct x86_operand *dst, struct x86_operand *src);

struct x86_instr *x86_instr_new_and(struct x86_operand *dst, struct x86_operand *src);

struct x86_instr *x86_instr_new_call(struct x86_operand *target);

struct x86_instr *x86_instr_new_cmp(struct x86_operand *dst, struct x86_operand *src);

struct x86_instr *x86_instr_new_jl(struct x86_operand *target);

struct x86_instr *x86_instr_new_jmp(struct x86_operand *target);

struct x86_instr *x86_instr_new_jns(struct x86_operand *target);

struct x86_instr *x86_instr_new_jnz(struct x86_operand *target);

struct x86_instr *x86_instr_new_jz(struct x86_operand *target);

struct x86_instr *x86_instr_new_label(int n);

struct x86_instr *x86_instr_new_mov(struct x86_operand *dst, struct x86_operand *src);

struct x86_instr *x86_instr_new_movzx(struct x86_operand *dst, struct x86_operand *src);

struct x86_instr *x86_instr_new_or(struct x86_operand *dst, struct x86_operand *src);

struct x86_instr *x86_instr_new_pop(struct x86_operand *dst);

struct x86_instr *x86_instr_new_push(struct x86_operand *src);

struct x86_instr *x86_instr_new_ret(void);

void x86_instr_free_node(struct x86_instr *instr);

void x86_instr_free_tree(struct x86_instr *instr);

#endif
