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

#include <stddef.h>
#include "builder.h"
#include "codegen.h"
#include "symbols.h"


#define REGM        X86_REG_RBX
#define REGP        X86_REG_R13
#define REG8TEMP    X86_REG_AL
#define REG64TEMP   X86_REG_RAX
#define REG32ARG1   X86_REG_EDI
#define REG64ARG1   X86_REG_RDI
#define REG32ARG2   X86_REG_ESI
#define REG64ARG2   X86_REG_RSI
#define REG8RETVAL  X86_REG_AL
#define REG32RETVAL X86_REG_EAX
#define REG64RETVAL X86_REG_RAX

struct state {
    int label;
};

static void initialize_state(struct state *state) {
    state->label = 0;
}

static void lower_node_add(struct x86_builder *builder, struct state *state, const struct node *node) {
    x86_builder_append_instr(builder, x86_instr_new_add(
        x86_operand_new_mem8_reg(REGM, REGP, node->offset),
        x86_operand_new_imm8(node->n)
    ));
}

static void lower_node_right(struct x86_builder *builder, struct state *state, const struct node *node) {
    x86_builder_append_instr(builder, x86_instr_new_add(
        x86_operand_new_reg64(REGP),
        x86_operand_new_imm64(node->n)
    ));
}

static void lower_node_in(struct x86_builder *builder, struct state *state, const struct node *node) {
    x86_builder_append_instr(builder, x86_instr_new_mov(
        x86_operand_new_reg64(REG64ARG1),
        x86_operand_new_mem64_extern(EXTERN_STDIN)
    ));
    x86_builder_append_instr(builder, x86_instr_new_call(
        x86_operand_new_extern(EXTERN_FGETC)
    ));
    
    x86_builder_append_instr(builder, x86_instr_new_mov(
        x86_operand_new_mem8_reg(REGM, REGP, node->offset),
        x86_operand_new_reg8(REG8RETVAL)
    ));
    x86_builder_append_instr(builder, x86_instr_new_mov(
        x86_operand_new_reg32(REG32ARG1),
        x86_operand_new_reg32(REG32RETVAL)
    ));
    x86_builder_append_instr(builder, x86_instr_new_call(
        x86_operand_new_local(LOCAL_CHECK_INPUT)
    ));
}

static void lower_node_out(struct x86_builder *builder, struct state *state, const struct node *node) {
    x86_builder_append_instr(builder, x86_instr_new_movzx(
        x86_operand_new_reg64(REG64ARG1),
        x86_operand_new_mem8_reg(REGM, REGP, node->offset)
    ));
    x86_builder_append_instr(builder, x86_instr_new_mov(
        x86_operand_new_reg64(REG64ARG2),
        x86_operand_new_mem64_extern(EXTERN_STDOUT)
    ));
    x86_builder_append_instr(builder, x86_instr_new_call(
        x86_operand_new_extern(EXTERN_PUTC)
    ));
}

/* forward declaration because mutually recursive with lower_node_loop() */
static void lower_to_x86_recursive(struct x86_builder *builder, struct state *state, const struct node *node);

static void lower_node_loop(struct x86_builder *builder, struct state *state, const struct node *node) {
    int start = state->label++;
    int end = state->label++;
    
    x86_builder_append_instr(builder, x86_instr_new_mov(
        x86_operand_new_reg8(REG8TEMP),
        x86_operand_new_mem8_reg(REGM, REGP, node->offset)
    ));
    x86_builder_append_instr(builder, x86_instr_new_or(
        x86_operand_new_reg8(REG8TEMP),
        x86_operand_new_reg8(REG8TEMP)
    ));
    x86_builder_append_instr(builder, x86_instr_new_jz(
        x86_operand_new_label(end)
    ));
    
    x86_builder_append_instr(builder, x86_instr_new_align(16));
    
    x86_builder_append_instr(builder, x86_instr_new_label(start));
    
    lower_to_x86_recursive(builder, state, node->body);
    
    x86_builder_append_instr(builder, x86_instr_new_mov(
        x86_operand_new_reg8(REG8TEMP),
        x86_operand_new_mem8_reg(REGM, REGP, node->offset)
    ));
    x86_builder_append_instr(builder, x86_instr_new_or(
        x86_operand_new_reg8(REG8TEMP),
        x86_operand_new_reg8(REG8TEMP)
    ));
    x86_builder_append_instr(builder, x86_instr_new_jnz(
        x86_operand_new_label(start)
    ));
    
    x86_builder_append_instr(builder, x86_instr_new_label(end));
}

static void lower_node_check_right(struct x86_builder *builder, struct state *state, const struct node *node) {
    int skip = state->label++;
    
    x86_builder_append_instr(builder, x86_instr_new_mov(
        x86_operand_new_reg64(REG64TEMP),
        x86_operand_new_reg64(REGP)
    ));
    x86_builder_append_instr(builder, x86_instr_new_add(
        x86_operand_new_reg64(REG64TEMP),
        x86_operand_new_imm64(node->offset)
    ));
    x86_builder_append_instr(builder, x86_instr_new_cmp(
        x86_operand_new_reg64(REG64TEMP),
        x86_operand_new_imm64(30000)
    ));
    x86_builder_append_instr(builder, x86_instr_new_jl(
        x86_operand_new_label(skip)
    ));
    
    x86_builder_append_instr(builder, x86_instr_new_call(
        x86_operand_new_local(LOCAL_FAIL_TOO_FAR_RIGHT)
    ));
    
    x86_builder_append_instr(builder, x86_instr_new_label(skip));
}

static void lower_node_check_left(struct x86_builder *builder, struct state *state, const struct node *node) {
    int skip = state->label++;
    
    x86_builder_append_instr(builder, x86_instr_new_mov(
        x86_operand_new_reg64(REG64TEMP),
        x86_operand_new_reg64(REGP)
    ));
    x86_builder_append_instr(builder, x86_instr_new_add(
        x86_operand_new_reg64(REG64TEMP),
        x86_operand_new_imm64(node->offset)
    ));
    x86_builder_append_instr(builder, x86_instr_new_jns(
        x86_operand_new_label(skip)
    ));
    
    x86_builder_append_instr(builder, x86_instr_new_call(
        x86_operand_new_local(LOCAL_FAIL_TOO_FAR_LEFT)
    ));
    
    x86_builder_append_instr(builder, x86_instr_new_label(skip));
}


static void lower_to_x86_recursive(struct x86_builder *builder, struct state *state, const struct node *node) {
    while(node != NULL) {
        switch(node->type) {
        case NODE_ADD:
            lower_node_add(builder, state, node);
            break;
        case NODE_RIGHT:
            lower_node_right(builder, state, node);
            break;
        case NODE_IN:
            lower_node_in(builder, state, node);
            break;
        case NODE_OUT:
            lower_node_out(builder, state, node);
            break;
        case NODE_LOOP:
        case NODE_STATIC_LOOP:
            lower_node_loop(builder, state, node);
            break;
        case NODE_CHECK_RIGHT:
            lower_node_check_right(builder, state, node);
            break;
        case NODE_CHECK_LEFT:
            lower_node_check_left(builder, state, node);
            break;
        }
        node = node->next;
    }
}

struct x86_instr *generate_code_for_x86(const struct node *node) {
    struct state state;
    initialize_state(&state);
    
    struct x86_builder builder;
    x86_builder_initialize_empty(&builder);
    
    lower_to_x86_recursive(&builder, &state, node);
    
    return x86_builder_get_first(&builder);
}
