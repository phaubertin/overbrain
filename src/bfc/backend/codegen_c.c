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
#include "codegen_c.h"

static int indentation_width(int indentation_level) {
    return 4 * indentation_level;
}

#define INDENTFMT                       "%*s"
#define INDENTARGS(indentation_level)   indentation_width(indentation_level), ""

struct state {
    FILE *f;
};

static void initialize_state(struct state *state, FILE *f) {
    state->f = f;
}

static bool has_positive_right_node(const struct node *node) {
    while(node != NULL) {
        if(node->type == NODE_ADD && node->n > 0) {
            return true;
        }
        
        if(node->type == NODE_LOOP && has_positive_right_node(node->body)) {
            return true;
        }
        
        node = node->next;
    }
    return false;
}

static void emit_fail_too_far_right_decl(struct state *state, const struct node *root) {
    if(! has_positive_right_node(root)) {
        return;
    }
    
    fprintf(state->f, "static void fail_too_far_right(void) {\n");
    
    fprintf(state->f, INDENTFMT "fprintf(stderr, \"Error: memory position out of bounds (overflow - too far right)\\n\");\n", INDENTARGS(1));
    fprintf(state->f, INDENTFMT "exit(EXIT_FAILURE);\n", INDENTARGS(1));
    
    fprintf(state->f, "}\n");
    fprintf(state->f, "\n");
}

static bool has_negative_right_node(const struct node *node) {
    while(node != NULL) {
        if(node->type == NODE_ADD && node->n < 0) {
            return true;
        }
        
        if(node->type == NODE_LOOP && has_negative_right_node(node->body)) {
            return true;
        }
        
        node = node->next;
    }
    return false;
}

static void emit_fail_too_far_left_decl(struct state *state, const struct node *root) {
    if(! has_negative_right_node(root)) {
        return;
    }
    
    fprintf(state->f, "static void fail_too_far_left(void) {\n");
    
    fprintf(state->f, INDENTFMT "fprintf(stderr, \"Error: memory position out of bounds (underflow - too far left)\\n\");\n", INDENTARGS(1));
    fprintf(state->f, INDENTFMT "exit(EXIT_FAILURE);\n", INDENTARGS(1));
    
    fprintf(state->f, "}\n");
    fprintf(state->f, "\n");
}

static bool has_in_node(const struct node *node) {
    while(node != NULL) {
        if(node->type == NODE_IN) {
            return true;
        }
        
        if(node->type == NODE_LOOP && has_in_node(node->body)) {
            return true;
        }
        
        node = node->next;
    }
    return false;
}

static void emit_check_input_decl(struct state *state, const struct node *root) {
    if(! has_in_node(root)) {
        return;
    }
    
    fprintf(state->f, "static void check_input(int inp) {\n");
    
    fprintf(state->f, INDENTFMT "if(inp == EOF) {\n", INDENTARGS(1));
    
    fprintf(state->f, INDENTFMT "if(ferror(stdin)) {\n", INDENTARGS(2));
    
    fprintf(state->f, INDENTFMT "fprintf(stderr, \"Error when reading input: %%s\\n\", strerror(errno));\n", INDENTARGS(3));
    
    fprintf(state->f, INDENTFMT "} else {\n", INDENTARGS(2));
    
    fprintf(state->f, INDENTFMT "fprintf(stderr, \"Error: reached end of input\\n\");\n", INDENTARGS(3));
    
    fprintf(state->f, INDENTFMT "}\n", INDENTARGS(2));
    fprintf(state->f, INDENTFMT "exit(EXIT_FAILURE);\n", INDENTARGS(2));
    
    fprintf(state->f, INDENTFMT "}\n", INDENTARGS(1));
    
    fprintf(state->f, "}\n");
    fprintf(state->f, "\n");
}

static void generate_header(struct state *state, const struct node *root) {
    fprintf(state->f, "#include <errno.h>\n");
    fprintf(state->f, "#include <stdio.h>\n");
    fprintf(state->f, "#include <stdlib.h>\n");
    fprintf(state->f, "#include <string.h>\n");
    fprintf(state->f, "\n");
    fprintf(state->f, "static char m[30000];\n");
    fprintf(state->f, "static int p = 0;\n");
    fprintf(state->f, "\n");
    
    emit_fail_too_far_right_decl(state, root);
    emit_fail_too_far_left_decl(state, root);
    emit_check_input_decl(state, root);
    
    fprintf(state->f, "int main(int args, char *argv[]) {\n");
}

static void emit_node_add(struct state *state, const struct node *node, int loop_level) {
    fprintf(state->f, INDENTFMT "/* add(%d) */\n", INDENTARGS(loop_level + 1), node->n);
    fprintf(state->f, INDENTFMT "m[p] += %d;\n", INDENTARGS(loop_level + 1), node->n);
}

static void emit_node_right(struct state *state, const struct node *node, int loop_level) {
    fprintf(state->f, INDENTFMT "/* right(%d) */\n", INDENTARGS(loop_level + 1), node->n);
    fprintf(state->f, INDENTFMT "p += %d;\n", INDENTARGS(loop_level + 1), node->n);
    
    if(node->n > 0) {
        fprintf(state->f, INDENTFMT "if(p > sizeof(m)) {\n", INDENTARGS(loop_level + 1));
        
        fprintf(state->f, INDENTFMT "fail_too_far_right();\n", INDENTARGS(loop_level + 2));
        
        fprintf(state->f, INDENTFMT "}\n", INDENTARGS(loop_level + 1));
    } else {
        fprintf(state->f, INDENTFMT "if(p < 0) {\n", INDENTARGS(loop_level + 1));
        
        fprintf(state->f, INDENTFMT "fail_too_far_left();\n", INDENTARGS(loop_level + 2));
        
        fprintf(state->f, INDENTFMT "}\n", INDENTARGS(loop_level + 1));
    }
}

static void emit_node_in(struct state *state, const struct node *node, int loop_level) {
    fprintf(state->f, INDENTFMT "/* in */\n", INDENTARGS(loop_level + 1));
    fprintf(state->f, INDENTFMT "inp = fgetc(stdin);\n", INDENTARGS(loop_level + 1));
    fprintf(state->f, INDENTFMT "check_input(inp);\n", INDENTARGS(loop_level + 1));
    fprintf(state->f, INDENTFMT "m[p] = inp;\n", INDENTARGS(loop_level + 1));
}

static void emit_node_out(struct state *state, const struct node *node, int loop_level) {
    fprintf(state->f, INDENTFMT "/* out */\n", INDENTARGS(loop_level + 1));
    fprintf(state->f, INDENTFMT "putc(m[p], stdout);\n", INDENTARGS(loop_level + 1));
}

/* forward declaration because of mutual recursion with emit_node_loop() */
static void generate_code(struct state *state, const struct node *node, int loop_level);

static void emit_node_loop(struct state *state, const struct node *node, int loop_level) {
    fprintf(state->f, INDENTFMT "/* loop */\n", INDENTARGS(loop_level + 1));
    fprintf(state->f, INDENTFMT "while(m[p]) {\n", INDENTARGS(loop_level + 1));
    generate_code(state, node->body, loop_level + 1);
    fprintf(state->f, INDENTFMT "}\n", INDENTARGS(loop_level + 1));
}

static void emit_input_decl(struct state *state, const struct node *node, int loop_level) {
    while(node != NULL) {
        if(node->type == NODE_IN) {
            fprintf(state->f, INDENTFMT "/* input decl */\n", INDENTARGS(loop_level + 1));
            fprintf(state->f, INDENTFMT "int inp;\n", INDENTARGS(loop_level + 1));
            return;
        }
        node = node->next;
    }
}

static void generate_code(struct state *state, const struct node *node, int loop_level) {
    emit_input_decl(state, node, loop_level);
    
    while(node != NULL) {
        switch(node->type) {
        case NODE_ADD:
            emit_node_add(state, node, loop_level);
            break;
        case NODE_RIGHT:
            emit_node_right(state, node, loop_level);
            break;
        case NODE_IN:
            emit_node_in(state, node, loop_level);
            break;
        case NODE_OUT:
            emit_node_out(state, node, loop_level);
            break;
        case NODE_LOOP:
            emit_node_loop(state, node, loop_level);
            break;
        }
        node = node->next;
    }
}

static void generate_footer(struct state *state) {
    fprintf(state->f, INDENTFMT "exit(EXIT_SUCCESS);\n", INDENTARGS(1));
    fprintf(state->f, "}\n");
}

void codegen_c_generate(FILE *f, const struct node *root) {
    struct state state;
    initialize_state(&state, f);
    generate_header(&state, root);
    generate_code(&state, root, 0);
    generate_footer(&state);
}
