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
#include "bound_checks.h"

struct minmax {
    int min;
    int max;
};

static void update_minmax(struct minmax *minmax, int value) {
    if(value > minmax->max) {
        minmax->max = value;
    }
    if(value < minmax->min) {
        minmax->min = value;
    }
}

static struct node *insert_bound_checks_recursive(
    struct node *node,
    int loop_level,
    int loop_offset
) {
    struct builder builder;
    builder_initialize_empty(&builder);
    
    while(node != NULL) {
        struct builder fragment_builder;
        builder_initialize_empty(&fragment_builder);
    
        struct minmax offset;
        offset.min = 0;
        offset.max = 0;
        
        while(node != NULL && node->type != NODE_LOOP) {
            switch(node->type) {
            case NODE_STATIC_LOOP:
                update_minmax(&offset, node->offset);
                builder_append_node(
                    &fragment_builder,
                    node_new_static_loop(
                        insert_bound_checks_recursive(node->body, loop_level + 1, node->offset),
                        node->offset
                    )
                );
                break;
            case NODE_RIGHT:
                update_minmax(&offset, node->n);
                builder_append_node(&fragment_builder, node_clone(node));
                break;
            case NODE_ADD:
            case NODE_IN:
            case NODE_OUT:
                update_minmax(&offset, node->offset);
                builder_append_node(&fragment_builder, node_clone(node));
                break;
            case NODE_LOOP:
            case NODE_CHECK_BOUNDS:
                break;
            }
            
            node = node->next;
        }
        
        if(node == NULL) {
            update_minmax(&offset, loop_offset);
        } else {
            /* loop node */
            update_minmax(&offset, node->offset);
            builder_append_node(
                &fragment_builder,
                node_new_loop(
                    insert_bound_checks_recursive(node->body, loop_level + 1, node->offset),
                    node->offset
                )
            );
            node = node->next;
        }
        
        if(offset.min < 0) {
            builder_append_node(&builder, node_new_check_bounds(offset.min));
        }
        if(offset.max > 0) {
            builder_append_node(&builder, node_new_check_bounds(offset.max));
        }
        builder_append_tree(&builder, builder_get_first(&fragment_builder));
    }
    
    return builder_get_first(&builder);
}

struct node *insert_bound_checks(struct node *node) {
    return insert_bound_checks_recursive(node, 0, 0);
}
