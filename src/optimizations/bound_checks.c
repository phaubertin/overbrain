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

static void get_static_loop_body_offsets(
    struct minmax *access_offset,
    struct node *node,
    int loop_offset
) {
    access_offset->min = loop_offset;
    access_offset->max = loop_offset;
    
    struct minmax child_offset;
    
    while(node != NULL) {
        switch(node->type) {
        case NODE_STATIC_LOOP:
            get_static_loop_body_offsets(&child_offset, node->body, node->offset);
            update_minmax(access_offset, child_offset.min);
            update_minmax(access_offset, child_offset.max);
            break;
        case NODE_ADD:
        case NODE_IN:
        case NODE_OUT:
            update_minmax(access_offset, node->offset);
            break;
        case NODE_RIGHT:
        case NODE_LOOP:
            /* a static loop cannot contain either of these */
            break;
        case NODE_CHECK_RIGHT:
        case NODE_CHECK_LEFT:
            /* these haven't been inserted yet */
            break;
        }

        node = node->next;
    }
}

static struct node *insert_bound_checks_recursive(
    struct node *node,
    int loop_level,
    int loop_offset
) {
    struct builder builder;
    builder_initialize_empty(&builder);
    
    int base_offset = loop_offset;
    
    while(node != NULL) {
        struct builder fragment_builder;
        builder_initialize_empty(&fragment_builder);
    
        struct minmax access_offset;
        access_offset.min = base_offset;
        access_offset.max = base_offset;
        
        struct minmax child_offset;
        
        int shift_offset = 0;
        
        while(node != NULL && node->type != NODE_LOOP) {
            switch(node->type) {
            case NODE_STATIC_LOOP:
                builder_append_node(&fragment_builder, node_clone(node));
                get_static_loop_body_offsets(&child_offset, node->body, node->offset);
                update_minmax(&access_offset, child_offset.min + shift_offset);
                update_minmax(&access_offset, child_offset.max + shift_offset);
                break;
            case NODE_RIGHT:
                shift_offset += node->n;
                builder_append_node(&fragment_builder, node_clone(node));
                break;
            case NODE_ADD:
            case NODE_IN:
            case NODE_OUT:
                update_minmax(&access_offset, node->offset + shift_offset);
                builder_append_node(&fragment_builder, node_clone(node));
                break;
            case NODE_LOOP:
            case NODE_CHECK_RIGHT:
            case NODE_CHECK_LEFT:
                break;
            }
            
            node = node->next;
        }
        
        if(node == NULL) {
            update_minmax(&access_offset, loop_offset + shift_offset);
        } else {
            /* loop node */
            update_minmax(&access_offset, node->offset + shift_offset);
        }
        
        if(access_offset.max > base_offset) {
            builder_append_node(&builder, node_new_check_right(access_offset.max));
        }
        if(access_offset.min < base_offset) {
            builder_append_node(&builder, node_new_check_left(access_offset.min));
        }
        
        builder_append_tree(&builder, builder_get_first(&fragment_builder));
        
        if(node != NULL) {
            builder_append_node(
                &builder,
                node_new_loop(
                    insert_bound_checks_recursive(node->body, loop_level + 1, node->offset),
                    node->offset
                )
            );
            base_offset = node->offset;
            node = node->next;
        }
    }
    
    return builder_get_first(&builder);
}

struct node *insert_bound_checks(struct node *node) {
    return insert_bound_checks_recursive(node, 0, 0);
}
