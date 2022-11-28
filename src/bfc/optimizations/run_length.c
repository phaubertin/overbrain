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
#include "../ir/builder.h"
#include "run_length.h"

static struct node *optimize_sequence(struct builder *builder, struct node *node) {
    node_type type = node->type;
    int n = 0;
    
    while(node != NULL && node->type == type) {
        n += node->n;
        node = node->next;
    }
    
    if(n != 0) {
        switch(type) {
        case NODE_ADD:
            builder_append_node(builder, node_new_add(n, 0));
            break;
        case NODE_RIGHT:
            builder_append_node(builder, node_new_right(n));
            break;
        default:
            break;
        }
    }
    
    return node;
}

struct node *run_length_optimize(struct node *node) {
    struct builder builder;
    builder_initialize_empty(&builder);
    
    while(node != NULL) {
        switch(node->type) {
        case NODE_ADD:
        case NODE_RIGHT:
            node = optimize_sequence(&builder, node);
            break;
        case NODE_LOOP:
        {
            struct node *body = run_length_optimize(node->body);
            
            /* Maybe we optimized the whole body away. */
            if(body != NULL) {
                builder_append_node(&builder, node_new_loop(body));
            }
        }
            node = node->next;
            break;
        default:
            builder_append_node(&builder, node_clone(node));
            node = node->next;
        }
    }
    
    return builder_get_first(&builder);
}
