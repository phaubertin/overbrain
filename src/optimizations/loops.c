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
#include "loops.h"

static struct node *fallback(struct node *loop) {
    return node_new_static_loop(optimize_loops(loop->body), loop->offset);
}

static struct node *generate_single_offset(struct node *loop, int loop_increment) {
    if((loop_increment & 1) == 0) {
        return fallback(loop);
    }

    return node_new_set(0, loop->offset);
}

static struct node *generate_multi_offset(struct node *loop, int loop_increment) {
    if(loop_increment != -1) {
        return fallback(loop);
    }

    struct builder builder;
    builder_initialize_empty(&builder);

    bool needs_loop = false;

    for(struct node *node = loop->body; node != NULL; node = node->next) {
        if(node->offset == loop->offset) {
            continue;
        }

        if(node->n != 1) {
            needs_loop = true;
            continue;
        }

        builder_append_node(&builder, node_new_add2(node->offset, loop->offset));
    }

    if(!needs_loop) {
        builder_append_node(&builder, node_new_set(0, loop->offset));
    } else {
        struct builder body_builder;
        builder_initialize_empty(&body_builder);

        builder_append_node(&body_builder, node_new_add(-1, loop->offset));

        for(struct node *node = loop->body; node != NULL; node = node->next) {
            if(node->offset == loop->offset || node->n == 1) {
                continue;
            }

            builder_append_node(&body_builder, node_clone(node));
        }

        builder_append_node(
            &builder,
            node_new_static_loop(builder_get_first(&body_builder), loop->offset)
        );
    }

    return builder_get_first(&builder);
}

static struct node *process_static_loop(struct node *loop) {
    bool single_offset = true;
    int loop_increment = 0;

    for(struct node *node = loop->body; node != NULL; node = node->next) {
        if(node->type != NODE_ADD) {
            return fallback(loop);
        }

        if(node->offset == loop->offset) {
            loop_increment += node->n;
        } else {
            single_offset = false;
        }
    }

    if(single_offset) {
        return generate_single_offset(loop, loop_increment);
    } else {
        return generate_multi_offset(loop, loop_increment);
    }
}

struct node *optimize_loops(struct node *node) {
    struct builder builder;
    builder_initialize_empty(&builder);

    while(node != NULL) {
        switch(node->type) {
        case NODE_LOOP:
             builder_append_node(&builder, node_new_loop(optimize_loops(node->body), node->offset));
             break;
        case NODE_STATIC_LOOP:
            builder_append_tree(&builder, process_static_loop(node));
            break;
        default:
            builder_append_node(&builder, node_clone(node));
        }
        node = node->next;
    }
    
    return builder_get_first(&builder);
}
