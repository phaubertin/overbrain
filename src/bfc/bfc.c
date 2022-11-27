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
 
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "backend/codegen_c.h"
#include "frontend/parser.h"
#include "ir/node.h"
#include "optimizations/optimizations.h"

static struct node *read_program(const char *filename) {
    FILE *file = fopen(filename, "r");
    
    if(file == NULL) {
        fprintf(stderr, "Error opening file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    struct node *program = parse_program(file);
    
    fclose(file);
    
    return program;
}

static void usage(int argc, char *argv[]) {
    const char *argv0;
    
    if(argc > 0) {
        argv0 = argv[0];
    } else {
        argv0 = "bfc";
    }
    
    fprintf(stderr, "USAGE: %s program_file\n", argv0);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        usage(argc, argv);
    }
    
    struct node *program = read_program(argv[1]);
    
    struct node *optimized = run_optimizations(program);
    
    /* From this point, we only need the optimized tree. */
    node_free(program);
    
    codegen_c_generate(stdout, optimized);
    
    node_free(optimized);

    return EXIT_SUCCESS;
}
