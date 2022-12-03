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
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "options.h"
#include "../backend/codegen_c.h"
#include "../frontend/parser.h"
#include "../interpreter/slow.h"
#include "../interpreter/tree.h"
#include "../ir/node.h"
#include "../optimizations/optimizations.h"

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

static void usage(enum app app, int argc, char *argv[]) {
    const char *argv0;
    
    if(argc > 0) {
        argv0 = basename(argv[0]);
    } else if (app == APP_BFC) {
        argv0 = "bfc";
    } else {
        argv0 = "bf";
    }

    /* TODO update usage message with options */
    fprintf(stderr, "USAGE: %s [options ...] program_file\n", argv0);
    exit(EXIT_FAILURE);
}

static void set_defaults(struct options *options, enum app app) {
    if(app == APP_BFC) {
        options->action = ACTION_COMPILE;
    } else {
        options->action = ACTION_TREE;
    }
    options->optimization_level = 3;
}

int run_app(enum app app, int argc, char *argv[]) {
    struct options options;
    
    set_defaults(&options, app);
    
    if(! parse_options(&options, argc, argv)) {
        usage(app, argc, argv);
    }
    
    if(options.action == ACTION_SLOW) {
        slow_interpreter_run_program(options.filename);
        return EXIT_SUCCESS;
    }
    
    struct node *program = read_program(options.filename);
    
    if(options.optimization_level > 0) {
        struct node *optimized = run_optimizations(program);
        node_free(program);
        program = optimized;
    }
    
    if(options.action == ACTION_COMPILE) {
        codegen_c_generate(stdout, program);
    } else {
        tree_interpreter_run_program(program);
    }
    
    node_free(program);
    
    return EXIT_SUCCESS;
}
