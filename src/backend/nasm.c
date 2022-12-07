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
#include "nasm.h"

#define INDENT "    "

#define LABEL_FMT ".l%08d"

#define REGM "rbx"
#define REGP "r13"
#define REGTEMP "rax"
#define REGTEMP8 "al"

struct state {
    FILE *f;
    int label;
};

static void initialize_state(struct state *state, FILE *f) {
    state->f = f;
    state->label = 0;
}

static bool has_right_bound_check(const struct node *node) {
    while(node != NULL) {
        switch(node->type) {
        case NODE_CHECK_RIGHT:
            return true;
        case NODE_LOOP:
        case NODE_STATIC_LOOP:
            if(has_right_bound_check(node->body)) {
                return true;
            }
            break;
        default:
            break;
        }
        node = node->next;
    }
    return false;
}

static void emit_fail_too_far_right_decl(struct state *state, const struct node *root) {
    if(! has_right_bound_check(root)) {
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

static bool has_left_bound_check(const struct node *node) {
    while(node != NULL) {
        switch(node->type) {
        case NODE_CHECK_LEFT:
            return true;
        case NODE_LOOP:
        case NODE_STATIC_LOOP:
            if(has_left_bound_check(node->body)) {
                return true;
            }
            break;
        default:
            break;
        }
        node = node->next;
    }
    return false;
}

static void emit_fail_too_far_left_decl(struct state *state, const struct node *root) {
    if(! has_left_bound_check(root)) {
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

static bool has_in_node(const struct node *node) {
    while(node != NULL) {
        if(node->type == NODE_IN) {
            return true;
        }
        
        if(node->type == NODE_LOOP && has_in_node(node->body)) {
            return true;
        }
        
        node = node->next;
    }
    return false;
}

static void emit_check_input_decl(struct state *state, const struct node *root) {
    if(! has_in_node(root)) {
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
    fprintf(state->f, "\n");
    
    fprintf(state->f, INDENT "section .rodata\n");
    fprintf(state->f, "\n");
    fprintf(state->f, "r:\n");
    fprintf(state->f, INDENT "db \"r\", 0\n");
    fprintf(state->f, "w:\n");
    fprintf(state->f, INDENT "db \"w\", 0\n");
    if(has_right_bound_check(root)) {
        fprintf(state->f, "msg_right:\n");
        fprintf(state->f, INDENT "db \"Error: memory position out of bounds (overflow - too far right)\", 10, 0\n");
    }
    if(has_left_bound_check(root)) {
        fprintf(state->f, "msg_left:\n");
        fprintf(state->f, INDENT "db \"Error: memory position out of bounds (underflow - too far left)\", 10, 0\n");
    }
    if(has_in_node(root)) {
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
    
    emit_fail_too_far_right_decl(state, root);
    emit_fail_too_far_left_decl(state, root);
    emit_check_input_decl(state, root);
    
    fprintf(state->f, INDENT "global main:function (main.end - main)\n");
    fprintf(state->f, "main:\n");
    fprintf(state->f, INDENT "push rbp\n");
    fprintf(state->f, "\n");
    fprintf(state->f, INDENT "mov " REGM ", m\n");
    fprintf(state->f, INDENT "mov " REGP ", 0\n");
    fprintf(state->f, "\n");
}

static void emit_node_add(struct state *state, const struct node *node) {
    fprintf(state->f, INDENT "add byte [" REGM " + " REGP " + %d], %d\n", node->offset, node->n);
}

static void emit_node_right(struct state *state, const struct node *node) {
    fprintf(state->f, INDENT "add " REGP ", %d\n", node->n);
}

static void emit_node_in(struct state *state, const struct node *node) {
    fprintf(state->f, INDENT "mov rdi, qword [stdin]\n");
    fprintf(state->f, INDENT "call fgetc\n");
    fprintf(state->f, "\n");
    fprintf(state->f, INDENT "mov byte [" REGM " + " REGP " + %d], al\n", node->offset);
    fprintf(state->f, INDENT "mov rdi, rax\n");
    fprintf(state->f, INDENT "call check_input\n");
    fprintf(state->f, "\n");
}

static void emit_node_out(struct state *state, const struct node *node) {
    fprintf(state->f, INDENT "movzx rdi, byte [" REGM " + " REGP " + %d]\n", node->offset);
    fprintf(state->f, INDENT "mov rsi, qword [stdout]\n");
    fprintf(state->f, INDENT "call putc\n");
    fprintf(state->f, "\n");
}

/* forward declaration since this function is mutually recursive with emit_node_loop() */
static void generate_code(struct state *state, const struct node *node);

static void emit_node_loop(struct state *state, const struct node *node) {
    int label = state->label++;
    
    fprintf(state->f, "\n");
    fprintf(state->f, INDENT "mov " REGTEMP8 ", byte [" REGM " + " REGP " + %d]\n", node->offset);
    fprintf(state->f, INDENT "or " REGTEMP8 ", " REGTEMP8 "\n");
    fprintf(state->f, INDENT "jz " LABEL_FMT "end\n", label);    
    
    fprintf(state->f, "\n");
    fprintf(state->f, LABEL_FMT "start:\n", label);
    
    generate_code(state, node->body);
    
    fprintf(state->f, "\n");
    fprintf(state->f, INDENT "mov " REGTEMP8 ", byte [" REGM " + " REGP " + %d]\n", node->offset);
    fprintf(state->f, INDENT "or " REGTEMP8 ", " REGTEMP8 "\n");
    fprintf(state->f, INDENT "jnz " LABEL_FMT "start\n", label);
    
    fprintf(state->f, "\n");
    fprintf(state->f, LABEL_FMT "end:\n", label);
}

static void emit_node_check_right(struct state *state, const struct node *node) {
    int label = state->label++;
    
    fprintf(state->f, INDENT "mov " REGTEMP ", " REGP "\n");
    fprintf(state->f, INDENT "add " REGTEMP ", %d\n", node->offset);
    fprintf(state->f, INDENT "cmp " REGTEMP ", %d\n", 30000);
    fprintf(state->f, INDENT "jl " LABEL_FMT "\n", label);
    fprintf(state->f, "\n");
    fprintf(state->f, INDENT "call fail_too_far_right\n");
    fprintf(state->f, "\n");
    fprintf(state->f, LABEL_FMT ":\n", label);
}

static void emit_node_check_left(struct state *state, const struct node *node) {
    int label = state->label++;
    
    fprintf(state->f, INDENT "mov " REGTEMP ", " REGP "\n");
    fprintf(state->f, INDENT "add " REGTEMP ", %d\n", node->offset);
    fprintf(state->f, INDENT "jns " LABEL_FMT "\n", label);
    fprintf(state->f, "\n");
    fprintf(state->f, INDENT "call fail_too_far_left\n");
    fprintf(state->f, "\n");
    fprintf(state->f, LABEL_FMT ":\n", label);
}

static void generate_code(struct state *state, const struct node *node) {
    while(node != NULL) {
        switch(node->type) {
        case NODE_ADD:
            emit_node_add(state, node);
            break;
        case NODE_RIGHT:
            emit_node_right(state, node);
            break;
        case NODE_IN:
            emit_node_in(state, node);
            break;
        case NODE_OUT:
            emit_node_out(state, node);
            break;
        case NODE_LOOP:
        case NODE_STATIC_LOOP:
            emit_node_loop(state, node);
            break;
        case NODE_CHECK_RIGHT:
            emit_node_check_right(state, node);
            break;
        case NODE_CHECK_LEFT:
            emit_node_check_left(state, node);
            break;
        }
        node = node->next;
    }
}

static void generate_footer(struct state *state, const struct node *root) {
    fprintf(state->f, ".end:\n");
    fprintf(state->f, INDENT "pop rbp\n");
    fprintf(state->f, "\n");
    fprintf(state->f, INDENT "mov rax, 0\n");
    fprintf(state->f, INDENT "ret\n");
}

void nasm_generate(FILE *f, const struct node *root) {
    struct state state;
    initialize_state(&state, f);
    generate_header(&state, root);
    generate_code(&state, root);
    generate_footer(&state, root);
}