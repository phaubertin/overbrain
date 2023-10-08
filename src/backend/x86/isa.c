/*
 * Copyright (C) 2023 Philippe Aubertin.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "isa.h"

char *x86_reg8_names[] = {
    [X86_REG_AL] = "al",
    [X86_REG_CL] = "cl",
    [X86_REG_DL] = "dl",
    [X86_REG_BL] = "bl",
    [X86_REG_SPL] = "spl",
    [X86_REG_BPL] = "bpl",
    [X86_REG_SIL] = "sil",
    [X86_REG_DIL] = "dil",
    [X86_REG_R8B] = "r8b",
    [X86_REG_R9B] = "r9b",
    [X86_REG_R10B] = "r10b",
    [X86_REG_R11B] = "r11b",
    [X86_REG_R12B] = "r12b",
    [X86_REG_R13B] = "r13b",
    [X86_REG_R14B] = "r14b",
    [X86_REG_R15B] = "r15b"
};

char *x86_reg32_names[] = {
    [X86_REG_EAX] = "eax",
    [X86_REG_ECX] = "ecx",
    [X86_REG_EDX] = "edx",
    [X86_REG_EBX] = "ebx",
    [X86_REG_ESP] = "esp",
    [X86_REG_EBP] = "ebp",
    [X86_REG_ESI] = "esi",
    [X86_REG_EDI] = "edi",
    [X86_REG_R8D] = "r8d",
    [X86_REG_R9D] = "r9d",
    [X86_REG_R10D] = "r10d",
    [X86_REG_R11D] = "r11d",
    [X86_REG_R12D] = "r12d",
    [X86_REG_R13D] = "r13d",
    [X86_REG_R14D] = "r14d",
    [X86_REG_R15D] = "r15d"
};

char *x86_reg64_names[] = {
    [X86_REG_RAX] = "rax",
    [X86_REG_RCX] = "rcx",
    [X86_REG_RDX] = "rdx",
    [X86_REG_RBX] = "rbx",
    [X86_REG_RSP] = "rsp",
    [X86_REG_RBP] = "rbp",
    [X86_REG_RSI] = "rsi",
    [X86_REG_RDI] = "rdi",
    [X86_REG_R8] = "r8",
    [X86_REG_R9] = "r9",
    [X86_REG_R10] = "r10",
    [X86_REG_R11] = "r11",
    [X86_REG_R12] = "r12",
    [X86_REG_R13] = "r13",
    [X86_REG_R14] = "r14",
    [X86_REG_R15] = "r15"
};

static struct x86_operand *oper_new(x86_operand_type type) {
    struct x86_operand *operand = malloc(sizeof(struct x86_operand));
    
    if(operand == NULL) {
        fprintf(stderr, "Error: memory allocation (operand)\n");
        exit(EXIT_FAILURE);
    }
    
    memset(operand, 0, sizeof(struct x86_operand));
    
    operand->type = type;
    
    return operand;
}

struct x86_operand *x86_operand_new_extern(extern_symbol symbol) {
    struct x86_operand *operand = oper_new(X86_OPERAND_EXTERN);
    operand->n = symbol;
    return operand;
}

struct x86_operand *x86_operand_new_imm8(int n) {
    struct x86_operand *operand = oper_new(X86_OPERAND_IMM8);
    operand->n = n;
    return operand;
}

struct x86_operand *x86_operand_new_imm32(int n) {
    struct x86_operand *operand = oper_new(X86_OPERAND_IMM32);
    operand->n = n;
    return operand;
}

struct x86_operand *x86_operand_new_label(int n) {
    struct x86_operand *operand = oper_new(X86_OPERAND_LABEL);
    operand->n = n;
    return operand;
}

struct x86_operand *x86_operand_new_local(local_symbol symbol) {
    struct x86_operand *operand = oper_new(X86_OPERAND_LOCAL);
    operand->n = symbol;
    return operand;
}

struct x86_operand *x86_operand_new_mem8_reg(x86_reg64 r1, x86_reg64 r2, int n) {
    struct x86_operand *operand = oper_new(X86_OPERAND_MEM8_REG);
    operand->r1 = r1;
    operand->r2 = r2;
    operand->n = n;
    return operand;
}

struct x86_operand *x86_operand_new_mem64_extern(extern_symbol symbol) {
    struct x86_operand *operand = oper_new(X86_OPERAND_MEM64_EXTERN);
    operand->n = symbol;
    return operand;
}

struct x86_operand *x86_operand_new_mem64_label(int n) {
    struct x86_operand *operand = oper_new(X86_OPERAND_MEM64_LABEL);
    operand->n = n;
    return operand;
}

struct x86_operand *x86_operand_new_mem64_local(local_symbol symbol) {
    struct x86_operand *operand = oper_new(X86_OPERAND_MEM64_LOCAL);
    operand->n = symbol;
    return operand;
}

struct x86_operand *x86_operand_new_mem64_rel(uint64_t address) {
    struct x86_operand *operand = oper_new(X86_OPERAND_MEM64_REL);
    operand->address = address;
    return operand;
}

struct x86_operand *x86_operand_new_reg8(x86_reg8 r) {
    struct x86_operand *operand = oper_new(X86_OPERAND_REG8);
    operand->r1 = r;
    return operand;
}

struct x86_operand *x86_operand_new_reg32(x86_reg32 r) {
    struct x86_operand *operand = oper_new(X86_OPERAND_REG32);
    operand->r1 = r;
    return operand;
}

struct x86_operand *x86_operand_new_reg64(x86_reg64 r) {
    struct x86_operand *operand = oper_new(X86_OPERAND_REG64);
    operand->r1 = r;
    return operand;
}

void x86_operand_free(struct x86_operand *operand) {
    free(operand);
}

bool x86_operand_is_64bit(const struct x86_operand *oper) {
    switch(oper->type) {
    case X86_OPERAND_MEM64_EXTERN:
    case X86_OPERAND_MEM64_LABEL:
    case X86_OPERAND_MEM64_LOCAL:
    case X86_OPERAND_REG64:
        return true;
    default:
        return false;
    }
}

static void check_single_operand_type(
    struct x86_operand *operand,
    const x86_operand_type types[],
    size_t size,
    const char *description
) {
    for(int idx = 0; idx < size / sizeof(x86_operand_type); ++idx) {
        if(operand->type == types[idx]) {
            return;
        }
    }
    
    fprintf(stderr, "Error: wrong/unsupported operand type for %s instruction\n", description);
    exit(EXIT_FAILURE);
}

struct two_operand_types {
    x86_operand_type dst;
    x86_operand_type src;
};

static void check_both_operand_types(
    struct x86_operand *dst,
    struct x86_operand *src,
    const struct two_operand_types types[],
    size_t size,
    const char *description
) {
    for(int idx = 0; idx < size / sizeof(struct two_operand_types); ++idx) {
        if(dst->type == types[idx].dst && src->type == types[idx].src) {
            return;
        }
    }
    
    fprintf(stderr, "Error: wrong/unsupported combination of operand types for %s instruction\n", description);
    exit(EXIT_FAILURE);
}

static struct x86_instr *x86_instr_new(x86_instr_op op) {
    struct x86_instr *instr = malloc(sizeof(struct x86_instr));
    
    if(instr == NULL) {
        fprintf(stderr, "Error: memory allocation (instr)\n");
        exit(EXIT_FAILURE);
    }
    
    memset(instr, 0, sizeof(struct x86_instr));
    
    instr->op = op;
    
    return instr;
}

struct x86_instr *x86_instr_new_align(int n) {
    struct x86_instr *instr = x86_instr_new(X86_INSTR_ALIGN);
    instr->n = n;
    return instr;
}

struct x86_instr *x86_instr_new_add(struct x86_operand *dst, struct x86_operand *src) {
    const struct two_operand_types supported[] = {
        {X86_OPERAND_MEM8_REG, X86_OPERAND_IMM8},
        {X86_OPERAND_MEM8_REG, X86_OPERAND_REG8},
        {X86_OPERAND_REG8, X86_OPERAND_REG8},
        {X86_OPERAND_REG32, X86_OPERAND_IMM32},
        {X86_OPERAND_REG32, X86_OPERAND_REG32},
        {X86_OPERAND_REG64, X86_OPERAND_IMM32},
        {X86_OPERAND_REG64, X86_OPERAND_REG64}
    };
    check_both_operand_types(dst, src, supported, sizeof(supported), "add");
    
    struct x86_instr *instr = x86_instr_new(X86_INSTR_ADD);
    instr->dst = dst;
    instr->src = src;
    return instr;
}

struct x86_instr *x86_instr_new_and(struct x86_operand *dst, struct x86_operand *src) {
    const struct two_operand_types supported[] = {
        {X86_OPERAND_MEM8_REG, X86_OPERAND_IMM8},
        {X86_OPERAND_MEM8_REG, X86_OPERAND_REG8},
        {X86_OPERAND_REG8, X86_OPERAND_REG8},
        {X86_OPERAND_REG32, X86_OPERAND_IMM32},
        {X86_OPERAND_REG32, X86_OPERAND_REG32},
        {X86_OPERAND_REG64, X86_OPERAND_IMM32},
        {X86_OPERAND_REG64, X86_OPERAND_REG64}
    };
    check_both_operand_types(dst, src, supported, sizeof(supported), "and");
    
    struct x86_instr *instr = x86_instr_new(X86_INSTR_AND);
    instr->dst = dst;
    instr->src = src;
    return instr;
}

struct x86_instr *x86_instr_new_call(struct x86_operand *target) {
    const x86_operand_type supported[] = {X86_OPERAND_EXTERN, X86_OPERAND_LOCAL};
    check_single_operand_type(target, supported, sizeof(supported), "call");

    struct x86_instr *instr = x86_instr_new(X86_INSTR_CALL);
    instr->dst = target;
    return instr;
}

struct x86_instr *x86_instr_new_cmp(struct x86_operand *dst, struct x86_operand *src) {
    const struct two_operand_types supported[] = {
        {X86_OPERAND_MEM8_REG, X86_OPERAND_IMM8},
        {X86_OPERAND_MEM8_REG, X86_OPERAND_REG8},
        {X86_OPERAND_REG8, X86_OPERAND_REG8},
        {X86_OPERAND_REG32, X86_OPERAND_IMM32},
        {X86_OPERAND_REG32, X86_OPERAND_REG32},
        {X86_OPERAND_REG64, X86_OPERAND_IMM32},
        {X86_OPERAND_REG64, X86_OPERAND_REG64}
    };
    check_both_operand_types(dst, src, supported, sizeof(supported), "cmp");
    
    struct x86_instr *instr = x86_instr_new(X86_INSTR_CMP);
    instr->dst = dst;
    instr->src = src;
    return instr;
}

struct x86_instr *x86_instr_new_jl(struct x86_operand *target) {
    const x86_operand_type supported[] = {X86_OPERAND_LABEL};
    check_single_operand_type(target, supported, sizeof(supported), "conditional jump (jl)");
    
    struct x86_instr *instr = x86_instr_new(X86_INSTR_JL);
    instr->dst = target;
    return instr;
}

struct x86_instr *x86_instr_new_jmp(struct x86_operand *target) {
    const x86_operand_type supported[] = {X86_OPERAND_LABEL, X86_OPERAND_MEM64_REL};
    check_single_operand_type(target, supported, sizeof(supported), "jump (jmp)");
    
    struct x86_instr *instr = x86_instr_new(X86_INSTR_JMP);
    instr->dst = target;
    return instr;
}

struct x86_instr *x86_instr_new_jns(struct x86_operand *target) {
    const x86_operand_type supported[] = {X86_OPERAND_LABEL};
    check_single_operand_type(target, supported, sizeof(supported), "conditional jump (jns)");
    
    struct x86_instr *instr = x86_instr_new(X86_INSTR_JNS);
    instr->dst = target;
    return instr;
}

struct x86_instr *x86_instr_new_jnz(struct x86_operand *target) {
    const x86_operand_type supported[] = {X86_OPERAND_LABEL};
    check_single_operand_type(target, supported, sizeof(supported), "conditional jump (jnz)");
    
    struct x86_instr *instr = x86_instr_new(X86_INSTR_JNZ);
    instr->dst = target;
    return instr;
}

struct x86_instr *x86_instr_new_jz(struct x86_operand *target) {
    const x86_operand_type supported[] = {X86_OPERAND_LABEL};
    check_single_operand_type(target, supported, sizeof(supported), "conditional jump (jz)");
    
    struct x86_instr *instr = x86_instr_new(X86_INSTR_JZ);
    instr->dst = target;
    return instr;
}

struct x86_instr *x86_instr_new_label(int n) {
    struct x86_instr *instr = x86_instr_new(X86_INSTR_LABEL);
    instr->dst = x86_operand_new_label(n);
    return instr;
}

struct x86_instr *x86_instr_new_lea(struct x86_operand *dst, struct x86_operand *src) {
        const struct two_operand_types supported[] = {
        {X86_OPERAND_REG64, X86_OPERAND_MEM64_LABEL},
        {X86_OPERAND_REG64, X86_OPERAND_MEM64_LOCAL},
    };
    check_both_operand_types(dst, src, supported, sizeof(supported), "lea");
    
    struct x86_instr *instr = x86_instr_new(X86_INSTR_LEA);
    instr->dst = dst;
    instr->src = src;
    return instr;
}

struct x86_instr *x86_instr_new_mov(struct x86_operand *dst, struct x86_operand *src) {
    const struct two_operand_types supported[] = {
        {X86_OPERAND_MEM8_REG, X86_OPERAND_REG8},
        {X86_OPERAND_MEM8_REG, X86_OPERAND_IMM8},
        {X86_OPERAND_REG8, X86_OPERAND_MEM8_REG},
        {X86_OPERAND_REG32, X86_OPERAND_IMM32},
        {X86_OPERAND_REG32, X86_OPERAND_REG32},
        {X86_OPERAND_REG64, X86_OPERAND_MEM64_EXTERN},
        {X86_OPERAND_REG64, X86_OPERAND_MEM64_LOCAL},
        {X86_OPERAND_REG64, X86_OPERAND_REG64},
    };
    check_both_operand_types(dst, src, supported, sizeof(supported), "mov");
    
    struct x86_instr *instr = x86_instr_new(X86_INSTR_MOV);
    instr->dst = dst;
    instr->src = src;
    return instr;
}

struct x86_instr *x86_instr_new_movzx(struct x86_operand *dst, struct x86_operand *src) {
    const struct two_operand_types supported[] = {
        {X86_OPERAND_REG32, X86_OPERAND_MEM8_REG},
    };
    check_both_operand_types(dst, src, supported, sizeof(supported), "movzx");
    
    struct x86_instr *instr = x86_instr_new(X86_INSTR_MOVZX);
    instr->dst = dst;
    instr->src = src;
    return instr;
}

struct x86_instr *x86_instr_new_or(struct x86_operand *dst, struct x86_operand *src) {
    const struct two_operand_types supported[] = {
        {X86_OPERAND_MEM8_REG, X86_OPERAND_IMM8},
        {X86_OPERAND_MEM8_REG, X86_OPERAND_REG8},
        {X86_OPERAND_REG8, X86_OPERAND_REG8},
        {X86_OPERAND_REG32, X86_OPERAND_IMM32},
        {X86_OPERAND_REG32, X86_OPERAND_REG32},
        {X86_OPERAND_REG64, X86_OPERAND_IMM32},
        {X86_OPERAND_REG64, X86_OPERAND_REG64}
    };
    check_both_operand_types(dst, src, supported, sizeof(supported), "or");
    
    struct x86_instr *instr = x86_instr_new(X86_INSTR_OR);
    instr->dst = dst;
    instr->src = src;
    return instr;
}

struct x86_instr *x86_instr_new_pop(struct x86_operand *dst) {
    const x86_operand_type supported[] = {X86_OPERAND_REG64};
    check_single_operand_type(dst, supported, sizeof(supported), "pop");

    struct x86_instr *instr = x86_instr_new(X86_INSTR_POP);
    instr->dst = dst;
    return instr;
}

struct x86_instr *x86_instr_new_push(struct x86_operand *src) {
    const x86_operand_type supported[] = {
        X86_OPERAND_IMM32,
        X86_OPERAND_MEM64_REL,
        X86_OPERAND_REG64
    };
    check_single_operand_type(src, supported, sizeof(supported), "push");
    
    struct x86_instr *instr = x86_instr_new(X86_INSTR_PUSH);
    instr->src = src;
    return instr;
}

struct x86_instr *x86_instr_new_ret(void) {
    return x86_instr_new(X86_INSTR_RET);
}

struct x86_instr *x86_instr_new_segfault(void) {
    return x86_instr_new(X86_INSTR_SEGFAULT);
}

void x86_instr_free_node(struct x86_instr *instr) {
    free(instr);
}

void x86_instr_free_tree(struct x86_instr *instr) {
    while(instr != NULL) {
        struct x86_instr *next = instr->next;
        x86_instr_free_node(instr);
        instr = next;
    }
}
