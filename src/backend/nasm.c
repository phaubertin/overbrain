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

#include <stdbool.h>
#include <stdlib.h>
#include "../ir/query.h"
#include "codegen.h"
#include "nasm.h"
#include "symbols.h"
#include "x86.h"

#define INDENT "    "

#define REGM "rbx"
#define REGP "r13"
#define REGTEMP "rax"
#define REGTEMP8 "al"

#define OPERAND_BUFFER_SIZE 32

struct state {
    FILE *f;
    int label;
};

static void initialize_state(struct state *state, FILE *f) {
    state->f = f;
    state->label = 0;
}

static void generate_header(struct state *state, const struct node *root) {
    fprintf(state->f, "; generated by bfc (https://github.com/phaubertin)\n");
    fprintf(state->f, "\n");
    
    fprintf(state->f, INDENT "extern exit\n");
    fprintf(state->f, INDENT "extern ferror\n");
    fprintf(state->f, INDENT "extern fgetc\n");
    fprintf(state->f, INDENT "extern fprintf\n");
    fprintf(state->f, INDENT "extern perror\n");
    fprintf(state->f, INDENT "extern putc\n");
    fprintf(state->f, INDENT "extern stderr\n");
    fprintf(state->f, INDENT "extern stdin\n");
    fprintf(state->f, INDENT "extern stdout\n");
    fprintf(state->f, INDENT "extern __libc_start_main\n");
    fprintf(state->f, "\n");
    
    fprintf(state->f, INDENT "section .rodata\n");
    fprintf(state->f, "\n");
    fprintf(state->f, "r:\n");
    fprintf(state->f, INDENT "db \"r\", 0\n");
    fprintf(state->f, "w:\n");
    fprintf(state->f, INDENT "db \"w\", 0\n");
    if(tree_has_node_type(root, NODE_CHECK_RIGHT)) {
        fprintf(state->f, "msg_right:\n");
        fprintf(state->f, INDENT "db \"Error: memory position out of bounds (overflow - too far right)\", 10, 0\n");
    }
    if(tree_has_node_type(root, NODE_CHECK_LEFT)) {
        fprintf(state->f, "msg_left:\n");
        fprintf(state->f, INDENT "db \"Error: memory position out of bounds (underflow - too far left)\", 10, 0\n");
    }
    if(tree_has_node_type(root, NODE_IN)) {
        /* no end of line for this one because we are calling ferror() instead of fprintf() */
        fprintf(state->f, "msg_ferr:\n");
        fprintf(state->f, INDENT "db \"Error when reading input\", 0\n");
        fprintf(state->f, "msg_eoi:\n");
        fprintf(state->f, INDENT "db \"Error: reached end of input\", 10, 0\n");
    }
    fprintf(state->f, "\n");
    
    fprintf(state->f, INDENT "section .bss\n");
    fprintf(state->f, "\n");
    fprintf(state->f, "m:\n");
    fprintf(state->f, INDENT "resb 30000\n");
    fprintf(state->f, "\n");
    
    fprintf(state->f, INDENT "section .text\n");
    fprintf(state->f, "\n");
    
    fprintf(state->f, INDENT "global main:function (main.end - main)\n");
    fprintf(state->f, "main:\n");
    fprintf(state->f, INDENT "push rbp\n");
    fprintf(state->f, "\n");
    fprintf(state->f, INDENT "mov " REGM ", m\n");
    fprintf(state->f, INDENT "mov " REGP ", 0\n");
    fprintf(state->f, "\n");
}

static size_t format_operand_extern(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "%s", extern_symbol_names[operand->n]);
}

static size_t format_operand_imm8(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "%d", operand->n);
}

static size_t format_operand_imm64(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "%d", operand->n);
}

static size_t format_operand_label(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, ".l%08d", operand->n);
}

static size_t format_operand_local(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "%s", local_symbol_names[operand->n]);
}

static size_t format_operand_mem8_imm(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "byte [%d]", operand->n);
}

static size_t format_operand_mem8_reg(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "byte [%s + %s + %d]", x86_reg64_names[operand->r1], x86_reg64_names[operand->r2], operand->n);
}

static size_t format_operand_mem64_extern(char *buf, size_t bufsize, const struct x86_operand *operand) {
    return snprintf(buf, bufsize, "qword [%s]", extern_symbol_names[operand->n]);
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
    case X86_OPERAND_IMM64:
        retsize = format_operand_imm64(buf, bufsize, operand);
        break;
    case X86_OPERAND_LABEL:
        retsize = format_operand_label(buf, bufsize, operand);
        break;
    case X86_OPERAND_LOCAL:
        retsize = format_operand_local(buf, bufsize, operand);
        break;
    case X86_OPERAND_MEM8_IMM:
        retsize = format_operand_mem8_imm(buf, bufsize, operand);
        break;
    case X86_OPERAND_MEM8_REG:
        retsize = format_operand_mem8_reg(buf, bufsize, operand);
        break;
    case X86_OPERAND_MEM64_EXTERN:
        retsize = format_operand_mem64_extern(buf, bufsize, operand);
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

static void generate_code(struct state *state, const struct node *node) {
    struct x86_instr *instructions = generate_code_for_x86(node);
    
    struct x86_instr *instr = instructions;
    
    while(instr != NULL) {
        switch(instr->op) {
        case X86_INSTR_ALIGN:
            emit_instr_align(state, instr);
            break;
        case X86_INSTR_ADD:
            emit_instr_add(state, instr);
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
        }
        
        instr = instr->next;
    }
    
    x86_instr_free_tree(instructions);
}

static void emit_fail_too_far_right_decl(struct state *state, const struct node *root) {
    if(! tree_has_node_type(root, NODE_CHECK_RIGHT)) {
        return;
    }
    
    fprintf(state->f, "fail_too_far_right:\n");
    fprintf(state->f, INDENT "push rbp\n");
    fprintf(state->f, INDENT "\n");
    fprintf(state->f, INDENT "mov rdi, qword [stderr]\n");
    fprintf(state->f, INDENT "mov rsi, msg_right\n");
    fprintf(state->f, INDENT "call fprintf\n");
    fprintf(state->f, "\n");
    fprintf(state->f, INDENT "mov rdi, 1\n");
    fprintf(state->f, INDENT "call exit\n");
    fprintf(state->f, "\n");
}

static void emit_fail_too_far_left_decl(struct state *state, const struct node *root) {
    if(! tree_has_node_type(root, NODE_CHECK_LEFT)) {
        return;
    }
    
    fprintf(state->f, "fail_too_far_left:\n");
    fprintf(state->f, INDENT "push rbp\n");
    fprintf(state->f, INDENT "\n");
    fprintf(state->f, INDENT "mov rdi, qword [stderr]\n");
    fprintf(state->f, INDENT "mov rsi, msg_left\n");
    fprintf(state->f, INDENT "call fprintf\n");
    fprintf(state->f, "\n");
    fprintf(state->f, INDENT "mov rdi, 1\n");
    fprintf(state->f, INDENT "call exit\n");
    fprintf(state->f, "\n");
}

static void emit_check_input_decl(struct state *state, const struct node *root) {
    if(! tree_has_node_type(root, NODE_IN)) {
        return;
    }
    
    fprintf(state->f, "check_input:\n");
    fprintf(state->f, INDENT "push rbp\n");
    fprintf(state->f, INDENT "\n");
    
    fprintf(state->f, INDENT "cmp edi, -1\n");
    fprintf(state->f, INDENT "jnz .done\n");
    fprintf(state->f, INDENT "\n");
    
    fprintf(state->f, INDENT "mov rdi, qword [stdin]\n");
    fprintf(state->f, INDENT "call ferror\n");
    fprintf(state->f, INDENT "or rax, rax\n");
    fprintf(state->f, INDENT "jz .eoi\n");
    fprintf(state->f, INDENT "\n");
    
    fprintf(state->f, INDENT "mov rdi, msg_ferr\n");
    fprintf(state->f, INDENT "call perror\n");
    fprintf(state->f, INDENT "\n");
    
    fprintf(state->f, INDENT "jmp .die\n");
    fprintf(state->f, INDENT "\n");
    
    fprintf(state->f, ".eoi:\n");
    fprintf(state->f, INDENT "mov rdi, qword [stderr]\n");
    fprintf(state->f, INDENT "mov rsi, msg_eoi\n");
    fprintf(state->f, INDENT "call fprintf\n");
    fprintf(state->f, INDENT "\n");
    
    fprintf(state->f, ".die:\n");
    fprintf(state->f, INDENT "mov rdi, 1\n");
    fprintf(state->f, INDENT "call exit\n");
    fprintf(state->f, INDENT "\n");
    
    fprintf(state->f, ".done:\n");
    fprintf(state->f, INDENT "pop rbp\n");
    fprintf(state->f, INDENT "ret\n");
    fprintf(state->f, "\n");
}

static void emit_start(struct state *state) {
    fprintf(state->f, INDENT "global _start\n");    
    fprintf(state->f, "_start:\n");
    fprintf(state->f, INDENT "mov ebp, 0\n");
    fprintf(state->f, INDENT "mov r9, rdx\n");
    fprintf(state->f, INDENT "pop rsi\n");
    fprintf(state->f, INDENT "mov rdx, rsp\n");
    fprintf(state->f, INDENT "and rsp, ~0xf\n");
    fprintf(state->f, INDENT "push rax\n");
    fprintf(state->f, INDENT "push rsp\n");
    fprintf(state->f, INDENT "mov rcx, .return\n");
    fprintf(state->f, INDENT "mov r8, rcx\n");
    fprintf(state->f, INDENT "mov rdi, main\n");
    fprintf(state->f, INDENT "call __libc_start_main\n");
    
    fprintf(state->f, ".crash:\n");
    fprintf(state->f, INDENT "mov rax, [0]\n");
    
    fprintf(state->f, ".return:\n");
    fprintf(state->f, INDENT "ret\n");
}

static void generate_footer(struct state *state, const struct node *root) {
    fprintf(state->f, ".end:\n");
    fprintf(state->f, INDENT "pop rbp\n");
    fprintf(state->f, "\n");
    fprintf(state->f, INDENT "mov rax, 0\n");
    fprintf(state->f, INDENT "ret\n");
    fprintf(state->f, "\n");
    
    emit_fail_too_far_right_decl(state, root);
    emit_fail_too_far_left_decl(state, root);
    emit_check_input_decl(state, root);
    emit_start(state);
}

void nasm_generate(FILE *f, const struct node *root) {
    struct state state;
    initialize_state(&state, f);
    generate_header(&state, root);
    generate_code(&state, root);
    generate_footer(&state, root);
}
