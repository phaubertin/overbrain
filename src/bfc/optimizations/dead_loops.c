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
#include <stddef.h>
#include "../ir/builder.h"
#include "dead_loops.h"

/* This optimization pass removes loops that are known to never be executed,
 * i.e. loops where the value of the current cell is known to always be zero
 * on entry. Such loops are likely to be comments that contain instruction
 * characters. */

struct node *remove_dead_loops_recursive(struct node *node, int level) {
    struct builder builder;
    builder_initialize_empty(&builder);
    
    /* Special case: at the very beginning of the program, all cells are known
     * to be zero, so a program that starts with a loop will not run that loop.
     * It is worth handling this special case since a comment is likely at the
     * start of a program.
     * 
     * is_zero indicates the current cell is zero. It is initialized differently
     * at the beginning of the program (level == 0) but is relevant at all loop
     * levels. */
    bool is_zero = (level == 0);
    
    /* all_zero true means *all* memory is known to be zero. It is only relevant
     * at the beginning of the program. */
    bool all_zero = (level == 0);
    
    while(node != NULL) {
        switch(node->type) {
        case NODE_LOOP:
            if(!is_zero) {
                struct node *body = remove_dead_loops_recursive(node->body, level + 1);
                
                if(body != NULL) {
                    builder_append_node(&builder, node_new_loop(body));
                }
            }
            /* On exiting a loop, the current cell is known to be zero. */
            is_zero = true;
            break;
        case NODE_OUT:
            /* The output instruction (.) does not modify the content of memory,
             * so is_zero is not affected by it. */
            builder_append_node(&builder, node_clone(node));
            break;
        case NODE_RIGHT:
            builder_append_node(&builder, node_clone(node));
            /* When we move the cursor, we no longer know the value of the
             * current cell, unless we know all cells are still zero. */
            is_zero = all_zero;
            break;
        default:
            builder_append_node(&builder, node_clone(node));
            is_zero = false;
            all_zero = false;
        }
        
        node = node->next;
    }

    return builder_get_first(&builder);
}

struct node *remove_dead_loops(struct node *node) {
    return remove_dead_loops_recursive(node, 0);
}
