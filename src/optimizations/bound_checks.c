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
#include <stddef.h>
#include "../ir/builder.h"
#include "bound_checks.h"

/* This pass inserts the checks that ensure all accesses to the memory array
 * are within bounds. */

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
    
    /* Since static loops do not affect the position of the data pointer, we can
     * just recursively propagate up the minimum and maximum offsets, and then
     * the parent loop can take these offsets into accounts when it inserts its
     * own checks. This reduces the total number of checks. */
    while(node != NULL) {
        switch(node->type) {
        case NODE_STATIC_LOOP:
            get_static_loop_body_offsets(&child_offset, node->body, node->offset);
            update_minmax(access_offset, child_offset.min);
            update_minmax(access_offset, child_offset.max);
            break;
        case NODE_ADD:
        case NODE_SET:
        case NODE_IN:
        case NODE_OUT:
            update_minmax(access_offset, node->offset);
            break;
        case NODE_ADD2:
            update_minmax(access_offset, node->offset);
            update_minmax(access_offset, node->n);
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
    
    /* The base offset is an offset that is known to be safe to access. When
     * entering a loop, this is the loop offset, since it was just accessed to
     * determine whether the loop should be entered or not.
     * 
     * Since this offset is known to be safe to access, only accesses to the
     * right of this offset need a right (i.e. upper bound) check and only
     * accesses left of it need a left (i.e. lower bound) check. */
    int base_offset = loop_offset;
    
    while(node != NULL) {
        struct builder segment_builder;
        builder_initialize_empty(&segment_builder);
    
        struct minmax access_offset;
        access_offset.min = base_offset;
        access_offset.max = base_offset;
        
        /* Since non-static loops affect the position of the data pointer, we
         * need to split the loop body into segments at loops. We insert at most
         * one right and one left check at the beginning of each loop body, and
         * then at most one right and one left check just after each non-static
         * loop.
         * 
         * The shift_offset variable keep tracks of how much we get shifted by
         * NODE_RIGHT nodes. */
        int shift_offset = 0;
        
        while(node != NULL && node->type != NODE_LOOP) {
            struct minmax child_offset;
            
            switch(node->type) {
            case NODE_STATIC_LOOP:
                builder_append_node(&segment_builder, node_clone(node));
                get_static_loop_body_offsets(&child_offset, node->body, node->offset);
                update_minmax(&access_offset, child_offset.min + shift_offset);
                update_minmax(&access_offset, child_offset.max + shift_offset);
                break;
            case NODE_RIGHT:
                builder_append_node(&segment_builder, node_clone(node));
                shift_offset += node->n;
                break;
            case NODE_ADD:
            case NODE_SET:
            case NODE_IN:
            case NODE_OUT:
                builder_append_node(&segment_builder, node_clone(node));
                update_minmax(&access_offset, node->offset + shift_offset);
                break;
            case NODE_ADD2:
                builder_append_node(&segment_builder, node_clone(node));
                update_minmax(&access_offset, node->offset + shift_offset);
                update_minmax(&access_offset, node->n + shift_offset);
                break;
            case NODE_LOOP:
            case NODE_CHECK_RIGHT:
            case NODE_CHECK_LEFT:
                break;
            }
            
            node = node->next;
        }
        
        if(node == NULL) {
            /* At the end of the loop body, we need to make sure it is safe to
             * access the loop offset because the program is about to do so
             * to see if another iteration is needed. */
            update_minmax(&access_offset, loop_offset + shift_offset);
        } else {
            /* At this point, if node is not NULL, then it points to a loop node
             * because of the condition on the inner loop. We need to make sure
             * it is safe to access that loop's offset. */
            update_minmax(&access_offset, node->offset + shift_offset);
        }
        
        /* Insert the checks. */
        if(access_offset.max > base_offset) {
            builder_append_node(&builder, node_new_check_right(access_offset.max));
        }
        if(access_offset.min < base_offset) {
            builder_append_node(&builder, node_new_check_left(access_offset.min));
        }
        
        /* Now that the checks are inserted, the loop body segment can be added. */
        builder_append_tree(&builder, builder_get_first(&segment_builder));
        
        if(node != NULL) {
            builder_append_node(
                &builder,
                node_new_loop(
                    insert_bound_checks_recursive(node->body, loop_level + 1, node->offset),
                    node->offset
                )
            );
            /* When we get back from a nested loop, that loop's offset is known
             * to be safe to access (and this loop's offset might not be because
             * we have no idea how the nested loop has affected the data pointer). */
            base_offset = node->offset;
            node = node->next;
        }
    }
    
    return builder_get_first(&builder);
}

struct node *insert_bound_checks(struct node *node) {
    return insert_bound_checks_recursive(node, 0, 0);
}
