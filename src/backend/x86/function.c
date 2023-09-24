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
#include "function.h"

struct x86_function *x86_function_create(local_symbol symbol, struct x86_instr *instrs) {
    struct x86_function *func = malloc(sizeof(struct x86_function));
    
    if(func == NULL) {
        fprintf(stderr, "Error: memory allocation (x86 function)\n");
        exit(EXIT_FAILURE);
    }
    
    func->symbol = symbol;
    func->instrs = instrs;
    func->next = NULL;

    return func;
}

void x86_function_free(struct x86_function *func) {
    if(func != NULL) {
        x86_instr_free_tree(func->instrs);
    }
    
    free(func);
}

struct x86_function *x86_function_find_by_local_symbol(struct x86_function *head, local_symbol symbol) {
    struct x86_function *func = head;
    
    while(func != NULL && func->symbol != symbol) {
        func = func->next;
    }
    
    return func;
}
