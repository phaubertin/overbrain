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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../ir/query.h"
#include "../common/symbols.h"
#include "builder.h"
#include "codegen.h"

#define REGM        X86_REG_RBX
#define REGP        X86_REG_R13
#define REGP32      X86_REG_R13D
#define REG8TEMP    X86_REG_AL
#define REG64TEMP   X86_REG_RAX
#define REG32ARG1   X86_REG_EDI
#define REG64ARG1   X86_REG_RDI
#define REG32ARG2   X86_REG_ESI
#define REG64ARG2   X86_REG_RSI
#define REG64ARG3   X86_REG_RDX
#define REG64ARG4   X86_REG_RCX
#define REG64ARG5   X86_REG_R8
#define REG64ARG6   X86_REG_R9
#define REG8RETVAL  X86_REG_AL
#define REG32RETVAL X86_REG_EAX
#define REG64RETVAL X86_REG_RAX

struct state {
    int label;
};

static void initialize_state(struct state *state) {
    state->label = 0;
}

static void generate_node_add(struct x86_builder *builder, struct state *state, const struct node *node) {
    x86_builder_append_instr(builder, x86_instr_new_add(
        x86_operand_new_mem8_reg(REGM, REGP, node->offset),
        x86_operand_new_imm8(node->n)
    ));
}

static void generate_node_set(struct x86_builder *builder, struct state *state, const struct node *node) {
    x86_builder_append_instr(builder, x86_instr_new_mov(
        x86_operand_new_mem8_reg(REGM, REGP, node->offset),
        x86_operand_new_imm8(node->n)
    ));
}
static void generate_node_add2(
    struct x86_builder *builder,
    struct state *state,
    const struct node *node,
    const struct node *prev
) {
    /* peephole optimization: if the previous node was also an add2 node with the same source, we
     * don't need to load the register again since it already contains the right value. */
    if(prev == NULL || prev->type != NODE_ADD2 || prev->n != node->n) {
        x86_builder_append_instr(builder, x86_instr_new_mov(
            x86_operand_new_reg8(REG8TEMP),
            x86_operand_new_mem8_reg(REGM, REGP, node->n)
        ));
    }
    x86_builder_append_instr(builder, x86_instr_new_add(
        x86_operand_new_mem8_reg(REGM, REGP, node->offset),
        x86_operand_new_reg8(REG8TEMP)
    ));
}

static void generate_node_right(struct x86_builder *builder, struct state *state, const struct node *node) {
    x86_builder_append_instr(builder, x86_instr_new_add(
        x86_operand_new_reg64(REGP),
        x86_operand_new_imm32(node->n)
    ));
}

static void generate_node_in(struct x86_builder *builder, struct state *state, const struct node *node) {
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

static void generate_node_out(struct x86_builder *builder, struct state *state, const struct node *node) {
    x86_builder_append_instr(builder, x86_instr_new_movzx(
        x86_operand_new_reg32(REG32ARG1),
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

static bool needs_loop_test(const struct x86_builder *builder, int loop_offset) {
    /* peephole optimization: if the start or end of a loop is immediately
     * preceeded by an add instruction that affects the loop location, there is
     * no need to add instructions to set the zero flag (ZF) according to the
     * value at that location since it is already set appropriately.
     * 
     *  some_loop:
     *      ; ... other instructions maybe ...
     *      add byte [REGM + REGP + 42], -1
     *      mov REG8TEMP, byte [REGM + REGP + 42]   < not
     *      or REG8TEMP, REG8TEMP                   < needed
     *      jnz some_loop
     * 
     * */
    const struct x86_instr *instr = x86_builder_get_last(builder);
    
    if(instr == NULL) {
        return true;
    }
    
    if(instr->op != X86_INSTR_ADD) {
        return true;
    }
    
    const struct x86_operand *dst = instr->dst;
    
    if(dst->type != X86_OPERAND_MEM8_REG) {
        return true;
    }
    
    return (dst->r1 != REGM) || (dst->r2 != REGP) || (dst->n != loop_offset);
}

static void add_loop_test(struct x86_builder *builder, const struct node *node) {
    if(needs_loop_test(builder, node->offset)) {
        x86_builder_append_instr(builder, x86_instr_new_mov(
            x86_operand_new_reg8(REG8TEMP),
            x86_operand_new_mem8_reg(REGM, REGP, node->offset)
        ));
        x86_builder_append_instr(builder, x86_instr_new_or(
            x86_operand_new_reg8(REG8TEMP),
            x86_operand_new_reg8(REG8TEMP)
        ));
    }
}

/* forward declaration because mutually recursive with generate_node_loop() */
static void generate_code_recursive(struct x86_builder *builder, struct state *state, const struct node *node);

static void generate_node_loop(struct x86_builder *builder, struct state *state, const struct node *node) {
    int start = state->label++;
    int end = state->label++;
    
    add_loop_test(builder, node);
    x86_builder_append_instr(builder, x86_instr_new_jz(
        x86_operand_new_label(end)
    ));
    
    x86_builder_append_instr(builder, x86_instr_new_align(16));
    
    x86_builder_append_instr(builder, x86_instr_new_label(start));
    
    generate_code_recursive(builder, state, node->body);
    
    add_loop_test(builder, node);
    x86_builder_append_instr(builder, x86_instr_new_jnz(
        x86_operand_new_label(start)
    ));
    
    x86_builder_append_instr(builder, x86_instr_new_label(end));
}

static void generate_node_check_right(struct x86_builder *builder, struct state *state, const struct node *node) {
    int skip = state->label++;
    
    x86_builder_append_instr(builder, x86_instr_new_mov(
        x86_operand_new_reg64(REG64TEMP),
        x86_operand_new_reg64(REGP)
    ));
    x86_builder_append_instr(builder, x86_instr_new_add(
        x86_operand_new_reg64(REG64TEMP),
        x86_operand_new_imm32(node->offset)
    ));
    x86_builder_append_instr(builder, x86_instr_new_cmp(
        x86_operand_new_reg64(REG64TEMP),
        x86_operand_new_imm32(30000)
    ));
    x86_builder_append_instr(builder, x86_instr_new_jl(
        x86_operand_new_label(skip)
    ));
    
    x86_builder_append_instr(builder, x86_instr_new_call(
        x86_operand_new_local(LOCAL_FAIL_TOO_FAR_RIGHT)
    ));
    
    x86_builder_append_instr(builder, x86_instr_new_label(skip));
}

static void generate_node_check_left(struct x86_builder *builder, struct state *state, const struct node *node) {
    int skip = state->label++;
    
    x86_builder_append_instr(builder, x86_instr_new_mov(
        x86_operand_new_reg64(REG64TEMP),
        x86_operand_new_reg64(REGP)
    ));
    x86_builder_append_instr(builder, x86_instr_new_add(
        x86_operand_new_reg64(REG64TEMP),
        x86_operand_new_imm32(node->offset)
    ));
    x86_builder_append_instr(builder, x86_instr_new_jns(
        x86_operand_new_label(skip)
    ));
    
    x86_builder_append_instr(builder, x86_instr_new_call(
        x86_operand_new_local(LOCAL_FAIL_TOO_FAR_LEFT)
    ));
    
    x86_builder_append_instr(builder, x86_instr_new_label(skip));
}


static void generate_code_recursive(struct x86_builder *builder, struct state *state, const struct node *node) {
    const struct node *prev = NULL;

    while(node != NULL) {
        switch(node->type) {
        case NODE_ADD:
            generate_node_add(builder, state, node);
            break;
        case NODE_ADD2:
            generate_node_add2(builder, state, node, prev);
            break;
        case NODE_SET:
            generate_node_set(builder, state, node);
            break;   
        case NODE_RIGHT:
            generate_node_right(builder, state, node);
            break;
        case NODE_IN:
            generate_node_in(builder, state, node);
            break;
        case NODE_OUT:
            generate_node_out(builder, state, node);
            break;
        case NODE_LOOP:
        case NODE_STATIC_LOOP:
            generate_node_loop(builder, state, node);
            break;
        case NODE_CHECK_RIGHT:
            generate_node_check_right(builder, state, node);
            break;
        case NODE_CHECK_LEFT:
            generate_node_check_left(builder, state, node);
            break;
        }
        prev = node;
        node = node->next;
    }
}

static struct x86_instr *generate_main(const struct node *node) {
    struct x86_builder builder;
    x86_builder_initialize_empty(&builder);
    
    x86_builder_append_instr(&builder, x86_instr_new_push(
        x86_operand_new_reg64(X86_REG_RBP)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_push(
        x86_operand_new_reg64(REGP)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_push(
        x86_operand_new_reg64(REGM)
    ));
    
    x86_builder_append_instr(&builder, x86_instr_new_mov(
        x86_operand_new_reg64(REGM),
        x86_operand_new_mem64_local(LOCAL_M)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_mov(
        x86_operand_new_reg32(REGP32),
        x86_operand_new_imm32(0)
    ));

    struct state state;
    initialize_state(&state);   
    generate_code_recursive(&builder, &state, node);
    
    x86_builder_append_instr(&builder, x86_instr_new_pop(
        x86_operand_new_reg64(REGM)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_pop(
        x86_operand_new_reg64(REGP)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_pop(
        x86_operand_new_reg64(X86_REG_RBP)
    ));
    
    x86_builder_append_instr(&builder, x86_instr_new_mov(
        x86_operand_new_reg32(REG32RETVAL),
        x86_operand_new_imm32(EXIT_SUCCESS)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_ret());
    
    return x86_builder_get_first(&builder);
}

static struct x86_instr *generate_start(void) {
    struct x86_builder builder;
    x86_builder_initialize_empty(&builder);
    
    const int label_return = 1;
    
    x86_builder_append_instr(&builder, x86_instr_new_mov(
        x86_operand_new_reg32(X86_REG_EBP),
        x86_operand_new_imm32(0)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_mov(
        x86_operand_new_reg64(REG64ARG6),
        x86_operand_new_reg64(REG64ARG3)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_pop(
        x86_operand_new_reg64(REG64ARG2)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_mov(
        x86_operand_new_reg64(REG64ARG3),
        x86_operand_new_reg64(X86_REG_RSP)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_and(
        x86_operand_new_reg64(X86_REG_RSP),
        x86_operand_new_imm32(INT64_C(~0xf))
    ));
    x86_builder_append_instr(&builder, x86_instr_new_push(
        x86_operand_new_reg64(X86_REG_RAX)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_push(
        x86_operand_new_reg64(X86_REG_RSP)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_mov(
        x86_operand_new_reg64(REG64ARG4),
        x86_operand_new_label(label_return)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_mov(
        x86_operand_new_reg64(REG64ARG5),
        x86_operand_new_reg64(REG64ARG4)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_lea(
        x86_operand_new_reg64(REG64ARG1),
        x86_operand_new_mem64_local(LOCAL_MAIN)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_call(
        x86_operand_new_extern(EXTERN_LIBC_START_MAIN)
    ));
    
    /* __libc_start_main should not return, crash if it does */
    x86_builder_append_instr(&builder, x86_instr_new_segfault());
    
    x86_builder_append_instr(&builder, x86_instr_new_label(label_return));
    x86_builder_append_instr(&builder, x86_instr_new_ret());
    
    return x86_builder_get_first(&builder);
}

static struct x86_instr *generate_fail_too_far(local_symbol message) {
    struct x86_builder builder;
    x86_builder_initialize_empty(&builder);

    x86_builder_append_instr(&builder, x86_instr_new_push(
        x86_operand_new_reg64(X86_REG_RBP)
    ));
    
    x86_builder_append_instr(&builder, x86_instr_new_mov(
        x86_operand_new_reg64(REG64ARG1),
        x86_operand_new_mem64_extern(EXTERN_STDERR)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_lea(
        x86_operand_new_reg64(REG64ARG2),
        x86_operand_new_mem64_local(message)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_call(
        x86_operand_new_extern(EXTERN_FPRINTF)
    ));
    
    x86_builder_append_instr(&builder, x86_instr_new_mov(
        x86_operand_new_reg32(REG32ARG1),
        x86_operand_new_imm32(EXIT_FAILURE)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_call(
        x86_operand_new_extern(EXTERN_EXIT)
    ));

    return x86_builder_get_first(&builder);
}

static struct x86_instr *generate_check_input(void) {
    struct x86_builder builder;
    x86_builder_initialize_empty(&builder);
    
    const int label_eoi = 1;
    const int label_die = 2;
    const int label_done = 3;
    
    x86_builder_append_instr(&builder, x86_instr_new_push(
        x86_operand_new_reg64(X86_REG_RBP)
    ));

    x86_builder_append_instr(&builder, x86_instr_new_cmp(
        x86_operand_new_reg32(REG32ARG1),
        x86_operand_new_imm32(EOF)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_jnz(
        x86_operand_new_label(label_done)
    ));
    
    x86_builder_append_instr(&builder, x86_instr_new_mov(
        x86_operand_new_reg64(REG64ARG1),
        x86_operand_new_mem64_extern(EXTERN_STDIN)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_call(
        x86_operand_new_extern(EXTERN_FERROR)
    ));
    
    x86_builder_append_instr(&builder, x86_instr_new_or(
        x86_operand_new_reg32(REG32RETVAL),
        x86_operand_new_reg32(REG32RETVAL)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_jz(
        x86_operand_new_label(label_eoi)
    ));
    
    x86_builder_append_instr(&builder, x86_instr_new_lea(
        x86_operand_new_reg64(REG64ARG1),
        x86_operand_new_mem64_local(LOCAL_MSG_FERR)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_call(
        x86_operand_new_extern(EXTERN_PERROR)
    ));
    
    x86_builder_append_instr(&builder, x86_instr_new_jmp(
        x86_operand_new_label(label_die)
    ));
    
    x86_builder_append_instr(&builder, x86_instr_new_label(label_eoi));
    x86_builder_append_instr(&builder, x86_instr_new_mov(
        x86_operand_new_reg64(REG64ARG1),
        x86_operand_new_mem64_extern(EXTERN_STDERR)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_lea(
        x86_operand_new_reg64(REG64ARG2),
        x86_operand_new_mem64_local(LOCAL_MSG_EOI)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_call(
        x86_operand_new_extern(EXTERN_FPRINTF)
    ));
    
    x86_builder_append_instr(&builder, x86_instr_new_label(label_die));
    x86_builder_append_instr(&builder, x86_instr_new_mov(
        x86_operand_new_reg32(REG32ARG1),
        x86_operand_new_imm32(EXIT_FAILURE)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_call(
        x86_operand_new_extern(EXTERN_EXIT)
    ));
    
    x86_builder_append_instr(&builder, x86_instr_new_label(label_done));
    x86_builder_append_instr(&builder, x86_instr_new_pop(
        x86_operand_new_reg64(X86_REG_RBP)
    ));
    x86_builder_append_instr(&builder, x86_instr_new_ret());
    
    return x86_builder_get_first(&builder);
}

struct x86_function *generate_code_for_x86(const struct node *node) {
    struct x86_function *head = x86_function_create(
        LOCAL_START,
        generate_start()
    );

    struct x86_function *current = x86_function_create(
        LOCAL_MAIN,
        generate_main(node)
    );
    head->next = current;

    if(tree_has_node_type(node, NODE_CHECK_RIGHT)) {
        struct x86_function *next = x86_function_create(
            LOCAL_FAIL_TOO_FAR_RIGHT,
            generate_fail_too_far(LOCAL_MSG_RIGHT)
        );
        current->next = next;
        current = next;
    }
    
    if(tree_has_node_type(node, NODE_CHECK_LEFT)) {
        struct x86_function *next = x86_function_create(
            LOCAL_FAIL_TOO_FAR_LEFT,
            generate_fail_too_far(LOCAL_MSG_LEFT)
        );
        current->next = next;
        current = next;
    }
    
    if(tree_has_node_type(node, NODE_IN)) {
        struct x86_function *next = x86_function_create(
            LOCAL_CHECK_INPUT,
            generate_check_input()
        );
        current->next = next;
        current = next;
    }

    return head;
}
