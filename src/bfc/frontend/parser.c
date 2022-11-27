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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "../ir/builder.h"
#include "parser.h"

struct position {
    int line;
    int column;
};

static void copy_position(struct position *dest, const struct position *src) {
    dest->line = src->line;
    dest->column = src->column;
}

struct state {
    FILE *f;
    int lookahead;
    struct position position;
};

static void read_char(struct state *state) {
    state->lookahead = fgetc(state->f);
    
    if(ferror(state->f)) {
        fprintf(stderr, "Error reading file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

static void initialize_state(struct state *state, FILE *f) {
    state->f = f;
    state->position.line = 1;
    state->position.column = 1;
    read_char(state);
}

static void consume(struct state *state) {
    switch(state->lookahead) {
    case EOF:
        fprintf(stderr, "Error (bug): attempted to read past end of file\n");
        exit(EXIT_FAILURE);
        break;
    case '\n':
        ++state->position.line;
        state->position.column = 1;
        break;
    default:
        ++state->position.column;
    }
    
    read_char(state);
}

static struct node *parse_instructions(struct state *state, int loop_level, const struct position *loop_start) {
    struct builder builder;
    builder_initialize_empty(&builder);
    
    while(state->lookahead != EOF) {
        switch(state->lookahead) {
        case '+':
            builder_append_node(&builder, node_new_add(1));
            consume(state);
            break;
        case '-':
            builder_append_node(&builder, node_new_add(-1));
            consume(state);
            break;
        case '>':
            builder_append_node(&builder, node_new_right(1));
            consume(state);
            break;
        case '<':
            builder_append_node(&builder, node_new_right(-1));
            consume(state);
            break;
        case '.':
            builder_append_node(&builder, node_new_out());
            consume(state);
            break;
        case ',':
            builder_append_node(&builder, node_new_in());
            consume(state);
            break;
        case '[':
            {
                /* Capture position and then consume to reflect the correct
                 * position of the '[' instruction. */
                struct position nested_start;
                copy_position(&nested_start, &state->position);
                
                consume(state);

                /* Consume and then parse the loop body because the recursively
                 * called instance of this function expects the '[' character to
                 * have been consumed. */
                struct node *body = parse_instructions(state, loop_level + 1, &nested_start);
                builder_append_node(&builder, node_new_loop(body));
            }
            break;
        case ']':
            /* If loop level is zero, it means we are not inside a loop body. If
             * we encounter a closing ']' in this situation, it means there is
             * at least one superfluous ']' in the program. */
            if(loop_level == 0) {
                fprintf(stderr,
                    "Error: found unmatched ']' on line %d column %d\n",
                    state->position.line,
                    state->position.column
                );
                exit(EXIT_FAILURE);
            }
            consume(state);
            return builder_get_first(&builder);
        default:
            consume(state);
        }
    }
    
    /* If we reach the end of the program but are not at loop nesting level 0,
     * it means we are inside a loop body and this loop is missing its closing
     * ']'. */
    if(loop_level != 0) {
        fprintf(stderr,
            "Error: found unmatched '[' on line %d column %d\n",
            loop_start->line,
            loop_start->column
        );
        exit(EXIT_FAILURE);
    }
    
    return builder_get_first(&builder);
}

struct node *parse_program(FILE *f) {
    struct state state;
    initialize_state(&state, f);
    return parse_instructions(&state, 0, NULL);
}
