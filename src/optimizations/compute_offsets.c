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
#include "compute_offsets.h"

/* forward declaration since this function is mutually recursive with
 * compute_offsets(). */
static struct node *loop_elimination_recursive(
    struct node *node,
    int loop_level,
    int loop_offset
);

static int compute_scanning_offset(struct node *node) {
    int offset = 0;
    
    while(node != NULL) {
        if(node->type == NODE_RIGHT) {
            offset += node->n;
        }
        node = node->next;
    }
    
    return offset;
}

static struct node *compute_offsets_in_body(
    struct node *node,
    int loop_level,
    int loop_offset,
    int scanning_offset
) {
    struct builder builder;
    builder_initialize_empty(&builder);
    
    if(scanning_offset != 0) {
        builder_append_node(&builder, node_new_right(scanning_offset));
    }
    
    int offset = loop_offset - scanning_offset;
    
    while(node != NULL) {
        switch(node->type) {
        case NODE_RIGHT:
            offset += node->n;
            break;
        case NODE_ADD:
            builder_append_node(&builder, node_new_add(node->n, node->offset + offset));
            break;
        case NODE_IN:
            builder_append_node(&builder, node_new_in(node->offset + offset));
            break;
        case NODE_OUT:
            builder_append_node(&builder, node_new_out(node->offset + offset));
            break;
        case NODE_LOOP:
            builder_append_node(
                &builder,
                loop_elimination_recursive(node->body, loop_level + 1, offset)
            );
            break;
        case NODE_STATIC_LOOP:
        case NODE_CHECK_RIGHT:
        case NODE_CHECK_LEFT:
            /* none of these exist yet */
            break;
        }
        
        node = node->next;
    }
    
    return builder_get_first(&builder);
}

static bool loop_body_is_static(struct node *node) {
    while(node != NULL) {
        switch(node->type) {
        case NODE_RIGHT:
        case NODE_LOOP:
            return false;
        default:
            break;
        }
        node = node->next;
    }
    
    return true;
}

static struct node *loop_elimination_recursive(
    struct node *node,
    int loop_level,
    int loop_offset
) {
    struct node *body = compute_offsets_in_body(
        node,
        loop_level,
        loop_offset,
        compute_scanning_offset(node)
    );
    
    if(loop_body_is_static(body)) {
        return node_new_static_loop(body, loop_offset);
    } else {
        return node_new_loop(body, loop_offset);
    }
}

struct node *compute_offsets(struct node *node) {
    return compute_offsets_in_body(node, 0, 0, 0);
}
