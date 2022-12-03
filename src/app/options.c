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
#include <stdio.h>
#include <string.h>
#include "options.h"

typedef struct {
    const char *name;
    int value;
} enum_value;

typedef enum {
    OPTION_COMPILE,
    OPTION_NO_CHECK,
    OPTION_O0,
    OPTION_O1,
    OPTION_O2,
    OPTION_O3,
    OPTION_SLOW,
    OPTION_TREE,
    OPTION_UNKNOWN
} option_name;

static const enum_value option_names[] = {
    {"-compile",    OPTION_COMPILE},
    {"-no-check",   OPTION_NO_CHECK},
    {"-O0",         OPTION_O0},
    {"-O1",         OPTION_O1},
    {"-O2",         OPTION_O2},
    {"-O3",         OPTION_O3},
    {"-slow",       OPTION_SLOW},
    {"-tree",       OPTION_TREE},
    {NULL,          OPTION_UNKNOWN},
};

int parse_enum_value(const char *name, const enum_value *values) {
    const enum_value *current = values;
    
    while(current->name != NULL && strcmp(name, current->name) != 0) {
        ++current;
    }
    
    return current->value;
}

static int parse_option_name(const char *arg) {
    /* Allow either single or double dash for options. However, do not adjust
     * if the whole argument is two dashes (--). */
    if(strlen(arg) > 2 && arg[1] == '-') {
        ++arg;
    }
    
    return parse_enum_value(arg, option_names);
}

bool parse_options(struct options *options, int argc, char *argv[]) {
    options->no_check = false;
    
    if(argc < 2) {
        return false;
    }
    
    int index = 1;
    
    while(index < argc) {
        const char *arg = argv[index];
        int length = strlen(arg);
        
        if(length < 1) {
            return false;
        }
        
        if(arg[0] != '-') {
            break;
        }
        
        int option = parse_option_name(arg);
        
        switch(option) {
        case OPTION_COMPILE:
            options->action = ACTION_COMPILE;
            break;
        case OPTION_NO_CHECK:
            options->no_check = true;
            break;
        case OPTION_O0:
            options->optimization_level = 0;
            break;
        case OPTION_O1:
            options->optimization_level = 1;
            break;
        case OPTION_O2:
            options->optimization_level = 2;
            break;
        case OPTION_O3:
            options->optimization_level = 3;
            break;
        case OPTION_SLOW:
            options->action = ACTION_SLOW;
            break;
        case OPTION_TREE:
            options->action = ACTION_TREE;
            break;
        case OPTION_UNKNOWN:
            fprintf(stderr, "Unknown argument: %s\n", arg);
            return false;
        }
        
        ++index;
    }
    
    if(index != argc - 1) {
        return false;
    }
    
    options->filename = argv[index];
    return true;
}