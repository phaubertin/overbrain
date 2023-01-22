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
#include "jit.h"
#include "common/symbols.h"
#include "x86/codegen.h"
#include "x86/encoder.h"
#include "x86/isa.h"

struct jit_compiled_program {
    jit_main main;
};

/* TODO delete this */
static void hello(void) {
    printf("Hello World! (hardcoded)\n");
}

static jit_compiled_program *allocate_compiled_program(void) {
    jit_compiled_program *compiled = malloc(sizeof(jit_compiled_program));
    
    if(compiled == NULL) {
        fprintf(stderr, "Error: memory allocation (JIT context)\n");
        exit(EXIT_SUCCESS);
    }
    
    return compiled;
}

jit_compiled_program *jit_compiled_program_create(const struct node *program) {
    jit_compiled_program *compiled = allocate_compiled_program();
    
    /* TODO do stuff */
    compiled->main = hello;
        
    return compiled;
}

void jit_compiled_program_free(jit_compiled_program *compiled) {
    free(compiled);
}

jit_main jit_compiled_program_get_main(const jit_compiled_program *compiled) {
    return compiled->main;
}
