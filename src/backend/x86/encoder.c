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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "encoder.h"

struct x86_encoder_function {
    uint64_t address;
    int num_labels;
    uint64_t *labels;
    const struct x86_instr *instrs;
};

struct state {
    unsigned char *buf;
    size_t bufsize;
    size_t length;
    const x86_encoder_function *func;
    const x86_encoder_context *ctx;
    uint64_t address;
};

static void update_state_address(struct state *state) {
    state->address = state->func->address + state->length;
}

static void initialize_state(
    struct state *state,
    unsigned char *buf,
    size_t bufsize,
    const x86_encoder_function *func,
    const x86_encoder_context *ctx
) {
    state->buf = buf;
    state->bufsize = bufsize;
    state->length = 0;
    state->func = func;
    state->ctx = ctx;
    update_state_address(state);
}

static void write_byte(struct state *state, unsigned char byte) {
    if(state->buf != NULL) {
        if(state->length >= state->bufsize) {
            fprintf(stderr, "Error: instruction buffer overflow\n");
            exit(EXIT_FAILURE);
        }
    
        state->buf[state->length] = byte;
    }
    
    ++state->length;
}

static void write_word(struct state *state, int value) {
    write_byte(state, (value >>  0) & 0xff);
    write_byte(state, (value >>  8) & 0xff);
    write_byte(state, (value >> 16) & 0xff);
    write_byte(state, (value >> 24) & 0xff);
}

static void encode_instr_align(struct state *state, const struct x86_instr *instr) {
    uint64_t address = state->address;
    
    while((address & (instr->n - 1)) != 0) {
        write_byte(state, 0x90);    /* nop */
        ++address;
    }
}

static void encode_rex_prefix_for_mod_rm(
    struct state *state,
    const struct x86_operand *mod_rm,
    int reg
) {
    int prefix = 0x40;
    
    if(x86_operand_is_64bit(mod_rm)) {
        /* REX.W */
        prefix |= 8;
    }
    
    if(reg > 7) {
        /* REX.R */
        prefix |= 4;
    }
    
    if(mod_rm->r2 > 7) {
        /* REX.X */
        prefix |= 2;
    }
    
    if(mod_rm->r1 > 7) {
        /* REX.B */
        prefix |= 1;
    }
    
    if(prefix != 0x40) {
        write_byte(state, prefix);
    }
}

static int rel32(const struct state *state, const struct x86_operand *operand, uint64_t address) {
    switch(operand->type) {
    case X86_OPERAND_EXTERN:
    case X86_OPERAND_MEM64_EXTERN:
        return state->ctx->externs[operand->n] - address;
    case X86_OPERAND_LOCAL:
    case X86_OPERAND_MEM64_LOCAL:
        return state->ctx->locals[operand->n] - address;
    case X86_OPERAND_LABEL:
    case X86_OPERAND_MEM64_LABEL:
        return state->func->labels[operand->n] - address;
    case X86_OPERAND_MEM64_REL:
        return operand->address - address;
    default:
        fprintf(stderr, "Error: unsupported operand type (rel32)\n");
        exit(EXIT_FAILURE);
    }
}

static void encode_mod_rm_sib_disp(
    struct state *state,
    const struct x86_operand *mod_rm,
    int reg
) {
    int r1 = mod_rm->r1 & 7;
    int r2 = mod_rm->r2 & 7;
    int rreg = reg & 7;
    
    switch(mod_rm->type) {
    case X86_OPERAND_MEM8_REG:
        /* ModR/M byte */
        write_byte(state, 0x84 | (rreg << 3));
        /* SIB byte */
        write_byte(state, (r2 << 3) | r1);
        /* displacement */
        write_word(state, mod_rm->n);
        break;
    case X86_OPERAND_MEM64_EXTERN:
    case X86_OPERAND_MEM64_LABEL:
    case X86_OPERAND_MEM64_LOCAL:
        /* ModR/M byte */
        write_byte(state, 0x05 | (rreg << 3));
        /* displacement - assumes opcode is a single byte + REX prefix */
        write_word(state, rel32(state, mod_rm, state->address + 7));
        break;
    case X86_OPERAND_MEM64_REL:
        /* ModR/M byte */
        write_byte(state, 0x05 | (rreg << 3));
        /* displacement - assumes opcode is a single byte */
        write_word(state, rel32(state, mod_rm, state->address + 6));
        break;
    default:
        /* ModR/M byte */
        write_byte(state, 0xc0 | (rreg << 3) | r1);
        break;
    }
}

static bool is_in_imm8_range(int value) {
    return value >= -128 && value <= 127;
}

static void encode_alu_instr(
    struct state *state,
    int instr_num,
    const struct x86_operand *dst,
    const struct x86_operand *src
) {
    switch(src->type) {
    case X86_OPERAND_IMM8:
        encode_rex_prefix_for_mod_rm(state, dst, instr_num);
        write_byte(state, 0x80);
        encode_mod_rm_sib_disp(state, dst, instr_num);
        write_byte(state, src->n);
        break;
    case X86_OPERAND_IMM32:
        encode_rex_prefix_for_mod_rm(state, dst, instr_num);
        
        if(is_in_imm8_range(src->n)) {
            write_byte(state, 0x83);
            encode_mod_rm_sib_disp(state, dst, instr_num);
            write_byte(state, src->n);
        } else {
            /* There is a more compact encoding when the destination register is
             * eax/rax (e.g. "48 3d 30 75 00 00" (6 bytes) instead of "48 81 f8
             * 30 75 00 00" (7 bytes) for cmp rax, 0x7530). On the CPU where it
             * was tested (an AMD Phenom II X4 965), using that encoding led to
             * a ~5-10% slowdown. */
            write_byte(state, 0x81);
            encode_mod_rm_sib_disp(state, dst, instr_num);
            write_word(state, src->n);
        }
        break;
    case X86_OPERAND_REG8:
        encode_rex_prefix_for_mod_rm(state, dst, src->r1);
        write_byte(state, 0x08 * instr_num);
        encode_mod_rm_sib_disp(state, dst, src->r1);
        break;
    case X86_OPERAND_REG32:
    case X86_OPERAND_REG64:
        encode_rex_prefix_for_mod_rm(state, dst, src->r1);
        write_byte(state, 0x08 * instr_num + 1);
        encode_mod_rm_sib_disp(state, dst, src->r1);
        break;
    default:
        fprintf(stderr, "Error: unsupported source operand type (ALU op)\n");
        exit(EXIT_FAILURE);
        break;
    }
}

static void encode_instr_add(struct state *state, const struct x86_instr *instr) {
    encode_alu_instr(state, 0, instr->dst, instr->src);
}

static void encode_instr_and(struct state *state, const struct x86_instr *instr) {
    encode_alu_instr(state, 4, instr->dst, instr->src);
}

static void encode_instr_call(struct state *state, const struct x86_instr *instr) {
    write_byte(state, 0xe8);
    write_word(state, rel32(state, instr->dst, state->address + 5));
}

static void encode_instr_cmp(struct state *state, const struct x86_instr *instr) {
    encode_alu_instr(state, 7, instr->dst, instr->src);
}

static void encode_instr_jl(struct state *state, const struct x86_instr *instr) {
    int rel8 = rel32(state, instr->dst, state->address + 2);
    
    if(is_in_imm8_range(rel8)) {
        write_byte(state, 0x7c);
        write_byte(state, rel8);
    } else {
        write_byte(state, 0x0f);
        write_byte(state, 0x8c);
        write_word(state, rel32(state, instr->dst, state->address + 6));
    }
}

static void encode_instr_jmp(struct state *state, const struct x86_instr *instr) {
    if(instr->dst->type == X86_OPERAND_MEM64_REL) {
        write_byte(state, 0xff);
        encode_mod_rm_sib_disp(state, instr->dst, 4);
    } else {
        int rel8 = rel32(state, instr->dst, state->address + 2);
        
        if(is_in_imm8_range(rel8)) {
            write_byte(state, 0xeb);
            write_byte(state, rel8);
        } else {        
            write_byte(state, 0xe9);
            write_word(state, rel32(state, instr->dst, state->address + 5));
        }
    }
}

static void encode_instr_jns(struct state *state, const struct x86_instr *instr) {
    int rel8 = rel32(state, instr->dst, state->address + 2);

    if(is_in_imm8_range(rel8)) {
        write_byte(state, 0x79);
        write_byte(state, rel8);
    } else {    
        write_byte(state, 0x0f);
        write_byte(state, 0x89);
        write_word(state, rel32(state, instr->dst, state->address + 6));
    }
}

static void encode_instr_jnz(struct state *state, const struct x86_instr *instr) {
    int rel8 = rel32(state, instr->dst, state->address + 2);
    
    if(is_in_imm8_range(rel8)) {
        write_byte(state, 0x75);
        write_byte(state, rel8);
    } else {
        write_byte(state, 0x0f);
        write_byte(state, 0x85);
        write_word(state, rel32(state, instr->dst, state->address + 6));
    }
}

static void encode_instr_jz(struct state *state, const struct x86_instr *instr) {
    int rel8 = rel32(state, instr->dst, state->address + 2);
    
    if(is_in_imm8_range(rel8)) {
        write_byte(state, 0x74);
        write_byte(state, rel8);
    } else {
        write_byte(state, 0x0f);
        write_byte(state, 0x84);
        write_word(state, rel32(state, instr->dst, state->address + 6));
    }
}

static void encode_instr_lea(struct state *state, const struct x86_instr *instr) {
    encode_rex_prefix_for_mod_rm(state, instr->src, instr->dst->r1);
    write_byte(state, 0x8d);
    encode_mod_rm_sib_disp(state, instr->src, instr->dst->r1);
}

static void encode_instr_mov(struct state *state, const struct x86_instr *instr) {
    switch(instr->dst->type) {
    case X86_OPERAND_MEM8_REG:
        switch(instr->src->type) {
        case X86_OPERAND_REG8:
            encode_rex_prefix_for_mod_rm(state, instr->dst, instr->src->r1);
            write_byte(state, 0x88);
            encode_mod_rm_sib_disp(state, instr->dst, instr->src->r1);
            break;
        case X86_OPERAND_IMM8:
            encode_rex_prefix_for_mod_rm(state, instr->dst, 0);
            write_byte(state, 0xc6);
            encode_mod_rm_sib_disp(state, instr->dst, 0);
            write_byte(state, instr->src->n);
            break;
        default:
            fprintf(stderr, "Error: unsupported source operand type (mov)\n");
            exit(EXIT_FAILURE);
            break;
        }
        break;
    case X86_OPERAND_REG8:
        encode_rex_prefix_for_mod_rm(state, instr->src, instr->dst->r1);
        write_byte(state, 0x8a);
        encode_mod_rm_sib_disp(state, instr->src, instr->dst->r1);
        break;
    case X86_OPERAND_REG32:
    case X86_OPERAND_REG64:
        switch(instr->src->type) {
        case X86_OPERAND_IMM32:
            encode_rex_prefix_for_mod_rm(state, instr->dst, 0);
            if(instr->dst->type == X86_OPERAND_REG32) {
                write_byte(state, 0xb8 | (instr->dst->r1 & 7));
                write_word(state, instr->src->n);
            } else {
                write_byte(state, 0xc7);
                encode_mod_rm_sib_disp(state, instr->dst, 0);
                write_word(state, instr->src->n);
            }
            break;
        case X86_OPERAND_MEM64_EXTERN:
        case X86_OPERAND_MEM64_LOCAL:
            encode_rex_prefix_for_mod_rm(state, instr->src, instr->dst->r1);
            write_byte(state, 0x8b);
            encode_mod_rm_sib_disp(state, instr->src, instr->dst->r1);
            break;
        case X86_OPERAND_REG32:
        case X86_OPERAND_REG64:
            encode_rex_prefix_for_mod_rm(state, instr->dst, instr->src->r1);
            write_byte(state, 0x89);
            encode_mod_rm_sib_disp(state, instr->dst, instr->src->r1);
            break;
        default:
            fprintf(stderr, "Error: unsupported source operand type (mov)\n");
            exit(EXIT_FAILURE);
            break;
        }
        break;
    default:
        fprintf(stderr, "Error: unsupported destination operand type (mov)\n");
        exit(EXIT_FAILURE);
        break;
    }
}

static void encode_instr_movzx(struct state *state, const struct x86_instr *instr) {
    encode_rex_prefix_for_mod_rm(state, instr->src, instr->dst->r1);
    write_byte(state, 0x0f);
    write_byte(state, 0xb6);
    encode_mod_rm_sib_disp(state, instr->src, instr->dst->r1);
}

static void encode_instr_or(struct state *state, const struct x86_instr *instr) {
    encode_alu_instr(state, 1, instr->dst, instr->src);
}

static void encode_instr_pop(struct state *state, const struct x86_instr *instr) {
    if(instr->dst->r1 > 7) {
        /* REX.B */
        write_byte(state, 0x41);
    }
    
    int r = instr->dst->r1 & 7;
    write_byte(state, 0x58 | r);
}

static void encode_instr_push(struct state *state, const struct x86_instr *instr) {
    if(instr->src->type == X86_OPERAND_MEM64_REL) {
        write_byte(state, 0xff);
        encode_mod_rm_sib_disp(state, instr->src, 6);
    } else if(instr->src->type == X86_OPERAND_IMM32) {
        write_byte(state, 0x68);
        write_word(state, instr->src->n);
    } else {
        if(instr->src->r1 > 7) {
            /* REX.B */
            write_byte(state, 0x41);
        }
        
        int r = instr->src->r1 & 7;
        write_byte(state, 0x50 | r);
    }
}

static void encode_instr_ret(struct state *state, const struct x86_instr *instr) {
    write_byte(state, 0xc3);
}

static void encode_instr_segfault(struct state *state, const struct x86_instr *instr) {
    /* This is the encoding for hlt, which is a privileged instruction. */
    write_byte(state, 0xf4);
}

static void x86_encode_instruction(
    struct state *state,
    const struct x86_instr *instr
) {
    switch(instr->op) {
    case X86_INSTR_ALIGN:
        encode_instr_align(state, instr);
        break;
    case X86_INSTR_ADD:
        encode_instr_add(state, instr);
        break;
    case X86_INSTR_AND:
        encode_instr_and(state, instr);
        break;
    case X86_INSTR_CALL:
        encode_instr_call(state, instr);
        break;
    case X86_INSTR_CMP:
        encode_instr_cmp(state, instr);
        break;
    case X86_INSTR_JL:
        encode_instr_jl(state, instr);
        break;
    case X86_INSTR_JMP:
        encode_instr_jmp(state, instr);
        break;
    case X86_INSTR_JNS:
        encode_instr_jns(state, instr);
        break;
    case X86_INSTR_JNZ:
        encode_instr_jnz(state, instr);
        break;
    case X86_INSTR_JZ:
        encode_instr_jz(state, instr);
        break;
    case X86_INSTR_LABEL:
        /* nothing to encode */
        break;
    case X86_INSTR_LEA:
        encode_instr_lea(state, instr);
        break;
    case X86_INSTR_MOV:
        encode_instr_mov(state, instr);
        break;
    case X86_INSTR_MOVZX:
        encode_instr_movzx(state, instr);
        break;
    case X86_INSTR_OR:
        encode_instr_or(state, instr);
        break;
    case X86_INSTR_POP:
        encode_instr_pop(state, instr);
        break;
    case X86_INSTR_PUSH:
        encode_instr_push(state, instr);
        break;
    case X86_INSTR_RET:
        encode_instr_ret(state, instr);
        break;
    case X86_INSTR_SEGFAULT:
        encode_instr_segfault(state, instr);
        break;
    }
    
    update_state_address(state);
}

static size_t count_labels(const struct x86_instr *instrs) {
    size_t num_labels = 0;
    
    for(const struct x86_instr *instr = instrs; instr != NULL; instr = instr->next) {
        if(instr->op == X86_INSTR_LABEL && instr->dst->n >= num_labels) {
            num_labels = instr->dst->n + 1;
        }
    }
    
    return num_labels;
}

static void resolve_labels(x86_encoder_function *func) {
    memset(func->labels, 0, func->num_labels * sizeof(uint64_t));
    
    /* There are two forms for encoding jump/branch instructions with an immediate
     * value: a two-byte form with an 8-bit immediate value and a 5 or 6-byte
     * form with a 32-bit immediate value. We use the two-byte form wherever we
     * can and the longer form when the target is out of range for an 8-bit value.
     * 
     * Changing the form of a jump instruction changes the address of the labels
     * that follow that instruction. In turn, these address changes may change
     * the form of other jump instructions for which the target label was out of
     * range but is now in range. For this reason, we re-compute the label
     * addresses in a loop until they don't change anymore. */
    while(true) {
        x86_encoder_context dummy_context;
    
        struct state state;
        initialize_state(&state, NULL, 0, func, &dummy_context);
    
        bool nochange = true;
        
        for(const struct x86_instr *instr = func->instrs; instr != NULL; instr = instr->next) {
            if(instr->op == X86_INSTR_LABEL && func->labels[instr->dst->n] != state.address) {
                func->labels[instr->dst->n] = state.address;
                nochange = false;
            }
            
            x86_encode_instruction(&state, instr);
        }
        
        if(nochange) {
            break;
        }
    };
    
    for(const struct x86_instr *instr = func->instrs; instr != NULL; instr = instr->next) {
        if(instr->dst != NULL && instr->dst->type == X86_OPERAND_LABEL && func->labels[instr->dst->n] == 0) {
            fprintf(stderr, "Error: instruction destination operand references undefined label (index %d)\n", instr->dst->n);
            exit(EXIT_FAILURE); 
        }
        
        if(instr->src != NULL && instr->src->type == X86_OPERAND_LABEL && func->labels[instr->src->n] == 0) {
            fprintf(stderr, "Error: instruction source operand references undefined label (index %d)\n", instr->dst->n);
            exit(EXIT_FAILURE); 
        }
    }    
}

x86_encoder_function *x86_encoder_function_create(struct x86_instr *instrs, uint64_t address) {
    x86_encoder_function *func = malloc(sizeof(x86_encoder_function));
    
    if(func == NULL) {
        fprintf(stderr, "Error: memory allocation (x86 encoder function)\n");
        exit(EXIT_FAILURE);
    }
    
    func->instrs = instrs;
    func->address = address;
    func->num_labels = count_labels(instrs);
    
    if(func->num_labels == 0) {
        func->labels = NULL;
        return func;
    }
    
    func->labels = malloc(func->num_labels * sizeof(uint64_t));
    
    if(func->labels == NULL) {
        fprintf(stderr, "Error: memory allocation (label array for x86 encoder)\n");
        exit(EXIT_FAILURE);
    }
    
    resolve_labels(func);
    
    return func;
}

void x86_encoder_function_free(x86_encoder_function *func) {
    if(func != NULL) {
        free(func->labels);
    }
    
    free(func);
}

uint64_t x86_encoder_function_get_address(const x86_encoder_function *func) {
    return func->address;
}

void x86_encoder_context_set_extern(x86_encoder_context *ctx, int symbol, uint64_t value) {
    ctx->externs[symbol] = value;
}

void x86_encoder_context_set_local(x86_encoder_context *ctx, int symbol, uint64_t value) {
    ctx->locals[symbol] = value;
}

size_t x86_encoder_compute_function_size(const x86_encoder_function *func) {
    x86_encoder_context dummy_context;
    return encode_for_x86(NULL, 0, func, &dummy_context);
}

size_t encode_for_x86(
    unsigned char *buf,
    size_t bufsize,
    const x86_encoder_function *func,
    x86_encoder_context *ctx
) {
    struct state state;
    initialize_state(&state, buf, bufsize, func, ctx);
    
    for(const struct x86_instr *instr = func->instrs; instr != NULL; instr = instr->next) {
        x86_encode_instruction(&state, instr);
    }
    
    return state.length;
}
