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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROGRAM_SIZE (16 * 1024 * 1024)
#define MEMORY_SIZE 30000

static struct {
    size_t size;
    char bytes[PROGRAM_SIZE];
} program;

static struct {
    int instr_position;
    int mem_position;
} state;

static unsigned char memory[MEMORY_SIZE];

static void usage(int argc, char *argv[]) {
    const char *argv0;
    
    if(argc > 0) {
        argv0 = argv[0];
    } else {
        argv0 = "bf";
    }
    
    fprintf(stderr, "USAGE: %s program_file\n", argv0);
    exit(EXIT_FAILURE);
}

static void read_program(const char *filename) {
    FILE *file = fopen(filename, "r");
    
    if(file == NULL) {
        fprintf(stderr, "Error opening file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    errno = 0;
    
    program.size = fread(program.bytes, 1, sizeof(program), file);
    
    if(errno != 0) {
        fprintf(stderr, "Error reading file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    fclose(file);
}

static void skip_instructions(int loop_level) {
    int start = state.instr_position;
    
    while(state.instr_position < program.size) {
        char c = program.bytes[state.instr_position++];
        
        switch(c) {
        case '[':
            skip_instructions(loop_level + 1);
            break;
        case ']':
            if(loop_level < 1) {
                fprintf(stderr, "Error: found unmatched ']' at position %d\n", state.instr_position - 1);
                exit(EXIT_FAILURE);
            }
            return;
        }
    }
    
    if(loop_level != 0) {
        fprintf(stderr, "Error: found unmatched '[' at position %d\n", start - 1);
        exit(EXIT_FAILURE);
    }
}

static void run_instructions(int loop_level) {
    int start = state.instr_position;
    
    while(state.instr_position < program.size) {
        char c = program.bytes[state.instr_position++];
        
        switch(c) {
        case '+':
            ++memory[state.mem_position];
            break;
        case '-':
            --memory[state.mem_position];
            break;
        case '>':
            ++state.mem_position;
            
            if(state.mem_position >= sizeof(memory)) {
                fprintf(stderr, "Error: memory position out of bounds (overflow)\n");
                exit(EXIT_FAILURE);
            }
            break;
        case '<':
            --state.mem_position;
            
            if(state.mem_position < 0) {
                fprintf(stderr, "Error: memory position out of bounds (underflow)\n");
                exit(EXIT_FAILURE);
            }
            break;
        case '.':
            putc(memory[state.mem_position], stdout);
            break;
        case ',':
            errno = 0;
            int input = fgetc(stdin);
            
            if(input == EOF) {
                if(errno == 0) {
                    fprintf(stderr, "Error: reached end of input\n");
                } else {
                    fprintf(stderr, "Error when reading input: %s\n", strerror(errno));
                }
                exit(EXIT_FAILURE);
            }
            memory[state.mem_position] = input;
            break;
        case '[':
            if(memory[state.mem_position] == 0) {
                skip_instructions(loop_level + 1);
            } else {
                run_instructions(loop_level + 1);
            }
            break;
        case ']':
            if(loop_level < 1) {
                fprintf(stderr, "Error: found unmatched ']' at position %d\n", state.instr_position - 1);
                exit(EXIT_FAILURE);
            }
            if(memory[state.mem_position] == 0) {
                return;
            }
            state.instr_position = start;
            break;
        }
    }
    
    if(loop_level != 0) {
        fprintf(stderr, "Error: found unmatched '[' at position %d\n", start - 1);
        exit(EXIT_FAILURE);
    }
}

static void run_program(void) {
    run_instructions(0);
}

int main(int argc, char *argv[]) {
    if(argc != 2) {
        usage(argc, argv);
    }
    
    read_program(argv[1]);
    
    run_program();

    return EXIT_SUCCESS;
}
