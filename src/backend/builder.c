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
#include "builder.h"

void x86_builder_initialize_empty(struct x86_builder *builder) {
    builder->head = NULL;
    builder->tail = NULL;
}

void x86_builder_append_instr(struct x86_builder *builder, struct x86_instr *instr) {
    instr->next = NULL;
    if(builder->tail == NULL) {
        builder->head = instr;
        builder->tail = instr;
    } else {
        builder->tail->next = instr;
        builder->tail = instr;
    }
}

void x86_builder_append_tree(struct x86_builder *builder, struct x86_instr *instr) {
    if(instr == NULL) {
        /* empty tree */
        return;
    }
    
    struct x86_instr *first = instr;
    struct x86_instr *last = instr;
    
    while(instr != NULL) {
        last = instr;
        instr = instr->next;
    }
    
    if(builder->tail == NULL) {
        builder->head = first;
        builder->tail = last;
    } else {
        builder->tail->next = first;
        builder->tail = last;
    }
}

struct x86_instr *x86_builder_get_first(const struct x86_builder *builder) {
    return builder->head;
}