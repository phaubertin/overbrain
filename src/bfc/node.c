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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "node.h"

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

struct node *node_new_add(int n) {
    struct node *node = node_new(NODE_ADD);
    node->n = n;
    return node;
}

struct node *node_new_right(int n) {
    struct node *node = node_new(NODE_RIGHT);
    node->n = n;
    return node;
}

struct node *node_new_in(void) {
    return node_new(NODE_IN);
}

struct node *node_new_out(void) {
    return node_new(NODE_OUT);
}

struct node *node_new_loop(struct node *body) {
    struct node *node = node_new(NODE_LOOP);
    node->body = body;
    return node;
}

void node_free(struct node *node) {
    while(node != NULL) {
        if(node->type == NODE_LOOP) {
            node_free(node->body);
        }
        
        struct node *next = node->next;
        free(node);
        node = next;
    }
}
