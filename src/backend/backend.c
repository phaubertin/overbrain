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

#define _POSIX_C_SOURCE 1 /* for fdopen() */
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "backend.h"
#include "c.h"
#include "elf64.h"
#include "nasm.h"

FILE *open_output_file(const struct options *options) {
    if(options->ofilename == NULL) {
        return stdout;
    }
    
    if(options->backend != BACKEND_ELF64) {
        FILE *f = fopen(options->ofilename, "w");
        
        if(f == NULL) {
            fprintf(stderr, "Error opening output file (fopen()): %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        
        return f;
    }

#ifndef O_BINARY
#define O_BINARY 0
#endif
    
    int fd = open(options->ofilename, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, 0777);
    
    if(fd < 0) {
        fprintf(stderr, "Error opening output file (open()): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    FILE *f = fdopen(fd, "w");
    
    if(f == NULL) {
        fprintf(stderr, "Error opening output file (fdopen()): %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    return f;
}

void close_output_file(FILE *f) {
    if(f != stdout) {
        fclose(f);
    }
}

void backend_generate(const struct node *root, const struct options *options) {
    FILE *f = open_output_file(options);
    
    switch(options->backend) {
    case BACKEND_C:
        c_generate(f, root);
        break;
    case BACKEND_ELF64:
        elf64_generate(f, root);
        break;
    case BACKEND_NASM:
        nasm_generate(f, root);
        break;
    case BACKEND_UKNOWN:
        break;
    }
    
    close_output_file(f);
}
