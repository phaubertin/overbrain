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

#ifndef BFC_NODE_H
#define BFC_NODE_H

typedef enum {
    /* add a possibly negative value n to current memory cell:
     *  - for + instruction, n is 1
     *  - for - instruction, n is -1
     *  - optimization passes can produce nodes when n has other values */
    NODE_ADD,
    /* move memory position by a possibly negative value n to the right:
     *  - for > instruction, n is 1
     *  - for < instruction, n is -1
     *  - optimization passes can produce nodes when n has other values */
    NODE_RIGHT,
    /* input (,) instruction */
    NODE_IN,
    /* output (.) instruction */
    NODE_OUT,
    /* a loop with a body */
    NODE_LOOP,
    /* a loop that does not modify the data pointer */
    NODE_STATIC_LOOP,
    /* a check that the data pointer is still within upper bound */
    NODE_CHECK_RIGHT,
    /* a check that the data pointer is still within lower bound (i.e. 0) */
    NODE_CHECK_LEFT,
} node_type;

struct node {
    /* node type */
    node_type type;
    /* node value "n" for NODE_ADD and NODE_RIGHT */
    int n;
    /* offset of the operation relative to the current data pointer */
    int offset;
    /* next node, NULL to represent terminator */
    struct node *next;
    /* for NODE_LOOP nodes only: first node inside the loop body */
    struct node *body;
};

struct node *node_new_add(int n, int offset);

struct node *node_new_right(int n);

struct node *node_new_in(int offset);

struct node *node_new_out(int offset);

struct node *node_new_loop(struct node *body, int offset);

struct node *node_new_static_loop(struct node *body, int offset);

struct node *node_new_check_right(int offset);

struct node *node_new_check_left(int offset);

struct node *node_clone(struct node *node);

struct node *node_clone_tree(struct node *root);

void node_free(struct node *node);

#endif
