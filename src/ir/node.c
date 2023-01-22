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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "builder.h"
#include "node.h"
#include "query.h"

static struct node *node_new(node_type type) {
    struct node *node = malloc(sizeof(struct node));
    
    if(node == NULL) {
        fprintf(stderr, "Error: memory allocation (node)\n");
        exit(EXIT_FAILURE);
    }
    
    memset(node, 0, sizeof(struct node));
    
    node->type = type;
    
    return node;
}

struct node *node_new_add(int n, int offset) {
    struct node *node = node_new(NODE_ADD);
    node->n = n;
    node->offset = offset;
    return node;
}

struct node *node_new_right(int n) {
    struct node *node = node_new(NODE_RIGHT);
    node->n = n;
    return node;
}

struct node *node_new_in(int offset) {
    struct node *node = node_new(NODE_IN);
    node->offset = offset;
    return node;
}

struct node *node_new_out(int offset) {
    struct node *node = node_new(NODE_OUT);
    node->offset = offset;
    return node;
}

struct node *node_new_loop(struct node *body, int offset) {
    struct node *node = node_new(NODE_LOOP);
    node->body = body;
    node->offset = offset;
    return node;
}

struct node *node_new_static_loop(struct node *body, int offset) {
    struct node *node = node_new(NODE_STATIC_LOOP);
    node->body = body;
    node->offset = offset;
    return node;
}

struct node *node_new_check_right(int offset) {
    struct node *node = node_new(NODE_CHECK_RIGHT);
    node->offset = offset;
    return node;
}

struct node *node_new_check_left(int offset) {
    struct node *node = node_new(NODE_CHECK_LEFT);
    node->offset = offset;
    return node;
}

struct node *node_clone(struct node *node) {
    struct node *clone = node_new(node->type);
    
    clone->n = node->n;
    clone->offset = node->offset;
    
    if(node_is_loop(node)) {
        clone->body = node_clone_tree(node->body);
    }
    
    return clone;
}

struct node *node_clone_tree(struct node *root) {
    struct builder builder;
    builder_initialize_empty(&builder);
    
    struct node *node = root;
    
    while(node != NULL) {
        builder_append_node(&builder, node_clone(node));
        node = node->next;
    }
    
    return builder_get_first(&builder);
}

void node_free(struct node *node) {
    while(node != NULL) {
        if(node_is_loop(node)) {
            node_free(node->body);
        }
        
        struct node *next = node->next;
        free(node);
        node = next;
    }
}
