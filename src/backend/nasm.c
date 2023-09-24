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

#include <inttypes.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../ir/query.h"
#include "nasm.h"
#include "common/symbols.h"
#include "x86/codegen.h"
#include "x86/isa.h"

#define INDENT "    "
#define OPERAND_BUFFER_SIZE 32

struct state {
    FILE *f;
    int label;
};

static void initialize_state(struct state *state, FILE *f) {
    state->f = f;
    state->label = 0;
}

static size_t format_operand_extern(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "%s", extern_symbol_names[operand->n]);
}

static size_t format_operand_imm8(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "%d", (int)operand->n);
}

static size_t format_operand_imm32(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "%d", (int)operand->n);
}

static size_t format_operand_label(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, ".l%08d", (int)operand->n);
}

static size_t format_operand_local(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "%s", local_symbol_names[operand->n]);
}

static size_t format_operand_mem8_reg(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "byte [%s + %s + %d]", x86_reg64_names[operand->r1], x86_reg64_names[operand->r2], (int)operand->n);
}

static size_t format_operand_mem64_extern(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "qword [%s]", extern_symbol_names[operand->n]);
}

static size_t format_operand_mem64_local(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "qword [%s]", local_symbol_names[operand->n]);
}

static size_t format_operand_mem64_rel(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "qword [REL %" PRIu64 "]", operand->address);
}

static size_t format_operand_reg8(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "%s", x86_reg8_names[operand->r1]);
}

static size_t format_operand_reg32(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "%s", x86_reg32_names[operand->r1]);
}

static size_t format_operand_reg64(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "%s", x86_reg64_names[operand->r1]);
}

static void format_operand(char *buf, size_t bufsize, const struct x86_operand *operand) {
    size_t retsize = 0;
    
    switch(operand->type) {
    case X86_OPERAND_EXTERN:
        retsize = format_operand_extern(buf, bufsize, operand);
        break;
    case X86_OPERAND_IMM8:
        retsize = format_operand_imm8(buf, bufsize, operand);
        break;
    case X86_OPERAND_IMM32:
        retsize = format_operand_imm32(buf, bufsize, operand);
        break;
    case X86_OPERAND_LABEL:
        retsize = format_operand_label(buf, bufsize, operand);
        break;
    case X86_OPERAND_LOCAL:
        retsize = format_operand_local(buf, bufsize, operand);
        break;
    case X86_OPERAND_MEM8_REG:
        retsize = format_operand_mem8_reg(buf, bufsize, operand);
        break;
    case X86_OPERAND_MEM64_EXTERN:
        retsize = format_operand_mem64_extern(buf, bufsize, operand);
        break;
    case X86_OPERAND_MEM64_LOCAL:
        retsize = format_operand_mem64_local(buf, bufsize, operand);
        break;
    case X86_OPERAND_MEM64_REL:
        retsize = format_operand_mem64_rel(buf, bufsize, operand);
        break;
    case X86_OPERAND_REG8:
        retsize = format_operand_reg8(buf, bufsize, operand);
        break;
    case X86_OPERAND_REG32:
        retsize = format_operand_reg32(buf, bufsize, operand);
        break;
    case X86_OPERAND_REG64:
        retsize = format_operand_reg64(buf, bufsize, operand);
        break;
    }
    
    if(retsize >= bufsize) {
        fprintf(stderr, "Error: (NASM backend) operand truncated\n");
        exit(EXIT_FAILURE);
    }
}

static void emit_instr_align(struct state *state, const struct x86_instr *instr) {
    fprintf(state->f, INDENT "align %d, nop\n", instr->n);
}

static void emit_instr_add(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    char src[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    format_operand(src, sizeof(src), instr->src);
    
    fprintf(state->f, INDENT "add %s, %s\n", dst, src);
}

static void emit_instr_and(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    char src[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    format_operand(src, sizeof(src), instr->src);
    
    fprintf(state->f, INDENT "and %s, %s\n", dst, src);
}

static void emit_instr_call(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    
    fprintf(state->f, INDENT "call %s\n", dst);
    fprintf(state->f, "\n");
}

static void emit_instr_cmp(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    char src[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    format_operand(src, sizeof(src), instr->src);
    
    fprintf(state->f, INDENT "cmp %s, %s\n", dst, src);
}

static void emit_instr_jl(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    
    fprintf(state->f, INDENT "jl %s\n", dst);
    fprintf(state->f, "\n");
}

static void emit_instr_jmp(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    
    fprintf(state->f, INDENT "jmp %s\n", dst);
    fprintf(state->f, "\n");
}

static void emit_instr_jns(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    
    fprintf(state->f, INDENT "jns %s\n", dst);
    fprintf(state->f, "\n");
}

static void emit_instr_jnz(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    
    fprintf(state->f, INDENT "jnz %s\n", dst);
    fprintf(state->f, "\n");
}

static void emit_instr_jz(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    
    fprintf(state->f, INDENT "jz %s\n", dst);
    fprintf(state->f, "\n");
}

static void emit_instr_label(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    
    fprintf(state->f, "%s:\n", dst);
}

static void emit_instr_mov(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    char src[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    format_operand(src, sizeof(src), instr->src);
    
    fprintf(state->f, INDENT "mov %s, %s\n", dst, src);
}

static void emit_instr_movzx(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    char src[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    format_operand(src, sizeof(src), instr->src);
    
    fprintf(state->f, INDENT "movzx %s, %s\n", dst, src);
}

static void emit_instr_or(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    char src[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    format_operand(src, sizeof(src), instr->src);
    
    fprintf(state->f, INDENT "or %s, %s\n", dst, src);
}

static void emit_instr_pop(struct state *state, const struct x86_instr *instr) {
    char dst[OPERAND_BUFFER_SIZE];
    format_operand(dst, sizeof(dst), instr->dst);
    
    fprintf(state->f, INDENT "pop %s\n", dst);
}

static void emit_instr_push(struct state *state, const struct x86_instr *instr) {
    char src[OPERAND_BUFFER_SIZE];
    format_operand(src, sizeof(src), instr->src);
    
    fprintf(state->f, INDENT "push %s\n", src);
}

static void emit_instr_ret(struct state *state, const struct x86_instr *instr) {
    fprintf(state->f, INDENT "ret\n");
    fprintf(state->f, "\n");
}

static void emit_instr_segfault(struct state *state, const struct x86_instr *instr) {
    /* hlt is a privileged instruction */
    fprintf(state->f, INDENT "hlt\n");
    fprintf(state->f, "\n");
}

static void emit_code(struct state *state, const struct x86_instr *instr) {
    while(instr != NULL) {
        switch(instr->op) {
        case X86_INSTR_ALIGN:
            emit_instr_align(state, instr);
            break;
        case X86_INSTR_ADD:
            emit_instr_add(state, instr);
            break;
        case X86_INSTR_AND:
            emit_instr_and(state, instr);
            break;
        case X86_INSTR_CALL:
            emit_instr_call(state, instr);
            break;
        case X86_INSTR_CMP:
            emit_instr_cmp(state, instr);
            break;
        case X86_INSTR_JL:
            emit_instr_jl(state, instr);
            break;
        case X86_INSTR_JMP:
            emit_instr_jmp(state, instr);
            break;
        case X86_INSTR_JNS:
            emit_instr_jns(state, instr);
            break;
        case X86_INSTR_JNZ:
            emit_instr_jnz(state, instr);
            break;
        case X86_INSTR_JZ:
            emit_instr_jz(state, instr);
            break;
        case X86_INSTR_LABEL:
            emit_instr_label(state, instr);
            break;
        case X86_INSTR_MOV:
            emit_instr_mov(state, instr);
            break;
        case X86_INSTR_MOVZX:
            emit_instr_movzx(state, instr);
            break;
        case X86_INSTR_OR:
            emit_instr_or(state, instr);
            break;
        case X86_INSTR_POP:
            emit_instr_pop(state, instr);
            break;
        case X86_INSTR_PUSH:
            emit_instr_push(state, instr);
            break;
        case X86_INSTR_RET:
            emit_instr_ret(state, instr);
            break;
        case X86_INSTR_SEGFAULT:
            emit_instr_segfault(state, instr);
        }
        
        instr = instr->next;
    }
}

static void emit_header(struct state *state, const struct node *root) {
    fprintf(state->f, "; generated by bfc (https://github.com/phaubertin)\n");
    fprintf(state->f, "\n");
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        fprintf(state->f, INDENT "extern %s\n", extern_symbol_names[idx]);
    }
    fprintf(state->f, "\n");
}

static void emit_global_function_start(struct state *state, local_symbol symbol) {
    const char *name = local_symbol_names[symbol];
        
    fprintf(state->f, INDENT "global %s:function (%s.end - %s)\n", name, name, name);
    fprintf(state->f, "%s:\n", name);
}

static void emit_global_function_end(struct state *state) {
    fprintf(state->f, ".end:\n");
    fprintf(state->f, "\n");
}

static void emit_local_decl(struct state *state, local_symbol symbol) {
    fprintf(state->f, "%s:\n", local_symbol_names[symbol]);
}

static void emit_text(struct state *state, const struct node *root) {
    fprintf(state->f, INDENT "section .text\n");
    fprintf(state->f, "\n");

    struct x86_function *func = generate_code_for_x86(root);

    while(func != NULL) {
        bool is_global = func->symbol == LOCAL_START || func->symbol == LOCAL_MAIN;

        if(is_global) {
            emit_global_function_start(state, func->symbol);
        } else {
            emit_local_decl(state, func->symbol);
        }

        emit_code(state, func->instrs);

        if(is_global) {
            emit_global_function_end(state);
        }
        
        struct x86_function *next = func->next;
        x86_function_free(func);

        func = next;
    }
}

static void emit_rodata(struct state *state, const struct node *root) {
    fprintf(state->f, INDENT "section .rodata\n");
    fprintf(state->f, "\n");
    if(tree_has_node_type(root, NODE_CHECK_RIGHT)) {
        emit_local_decl(state, LOCAL_MSG_RIGHT);
        fprintf(state->f, INDENT "db \"Error: memory position out of bounds (overflow - too far right)\", 10, 0\n");
    }
    if(tree_has_node_type(root, NODE_CHECK_LEFT)) {
        emit_local_decl(state, LOCAL_MSG_LEFT);
        fprintf(state->f, INDENT "db \"Error: memory position out of bounds (underflow - too far left)\", 10, 0\n");
    }
    if(tree_has_node_type(root, NODE_IN)) {
        /* no end of line for this one because we are calling perror() instead of fprintf() */
        emit_local_decl(state, LOCAL_MSG_FERR);
        fprintf(state->f, INDENT "db \"Error when reading input\", 0\n");
        emit_local_decl(state, LOCAL_MSG_EOI);
        fprintf(state->f, INDENT "db \"Error: reached end of input\", 10, 0\n");
    }
    fprintf(state->f, "\n");
}

static void emit_data(struct state *state) {
    fprintf(state->f, INDENT "section .data\n");
    fprintf(state->f, "\n");
    emit_local_decl(state, LOCAL_M);
    fprintf(state->f, INDENT "dq marray\n");
    fprintf(state->f, "\n");
}

static void emit_bss(struct state *state) {
    fprintf(state->f, INDENT "section .bss\n");
    fprintf(state->f, "\n");
    fprintf(state->f, "marray:\n");
    fprintf(state->f, INDENT "resb 30000\n");
}

void nasm_generate(FILE *f, const struct node *root) {
    struct state state;
    initialize_state(&state, f);
    
    emit_header(&state, root);
    emit_text(&state, root);
    emit_rodata(&state, root);
    emit_data(&state);
    emit_bss(&state);
}
