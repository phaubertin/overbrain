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

#include <errno.h> 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tree.h"

#define MEMORY_SIZE 30000

static struct {
    int ptr;
    unsigned char memory[MEMORY_SIZE];
} state;

static void fail_too_far_right(void) {
    fprintf(stderr, "Error: memory position out of bounds (overflow - too far right)\n");
    exit(EXIT_FAILURE);
}

static void fail_too_far_left(void) {
    fprintf(stderr, "Error: memory position out of bounds (underflow - too far left)\n");
    exit(EXIT_FAILURE);
}

static void check_input(int inp) {
    if(inp == EOF) {
        if(ferror(stdin)) {
            fprintf(stderr, "Error when reading input: %s\n", strerror(errno));
        } else {
            fprintf(stderr, "Error: reached end of input\n");
        }
        exit(EXIT_FAILURE);
    }
}

/* forward declaration because mutually recursive with run_body() */
static void run_loop(const struct node *node, int loop_offset);

static void run_body(const struct node *node) {
    while(node != NULL) {
        int inp;
        
        switch(node->type) {
        case NODE_ADD:
            state.memory[state.ptr + node->offset] += node->n;
            break;
        case NODE_RIGHT:
            state.ptr += node->n;
            break;
        case NODE_IN:
            inp = fgetc(stdin);
            check_input(inp);
            state.memory[state.ptr + node->offset] = inp;
            break;
        case NODE_OUT:
            putc(state.memory[state.ptr + node->offset], stdout);
            break;
        case NODE_LOOP:
        case NODE_STATIC_LOOP:
            run_loop(node->body, node->offset);
            break;
        case NODE_CHECK_RIGHT:
            if(state.ptr + node->offset > MEMORY_SIZE) {
                fail_too_far_right();
            }
        case NODE_CHECK_LEFT:
            if(state.ptr + node->offset < 0) {
                fail_too_far_left();
            }
            break;
        }
        
        node = node->next;
    }
}

static void run_loop(const struct node *body, int loop_offset) {
    while(state.memory[state.ptr + loop_offset]) {
        run_body(body);
    };
}

void tree_interpreter_run_program(const struct node *program) {
    run_body(program);
}
