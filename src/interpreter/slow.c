#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "slow.h"

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

static void read_program(FILE *file) {
    errno = 0;
    
    program.size = fread(program.bytes, 1, sizeof(program), file);
    
    if(errno != 0) {
        fprintf(stderr, "Error reading file: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    if(! feof(file)) {
        fprintf(stderr, "Error: program is too long\n");
        exit(EXIT_FAILURE);
    }
}

static void check_end_of_program(int loop_level, int loop_start_position) {
    /* If we reach the end of the program but are not at loop nesting level 0,
     * it means we are inside a loop body and this loop is missing its closing
     * ']'. */
    if(loop_level != 0) {
        fprintf(stderr, "Error: found unmatched '[' at position %d\n", loop_start_position);
        exit(EXIT_FAILURE);
    }
}

static void check_loop_end(int loop_level, int loop_end_position) {
    /* If loop level is zero, it means we are not inside a loop body. If we
     * encounter a closing ']' in this situation, it means there is at least one
     * superfluous ']' in the program. */
    if(loop_level == 0) {
        fprintf(stderr, "Error: found unmatched ']' at position %d\n", loop_end_position);
        exit(EXIT_FAILURE);
    }
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
            check_loop_end(loop_level, state.instr_position - 1);
            return;
        }
    }
    
    check_end_of_program(loop_level, start - 1);
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
            {
                int input = fgetc(stdin);
                
                if(input == EOF) {
                    if(ferror(stdin)) {
                        fprintf(stderr, "Error when reading input: %s\n", strerror(errno));
                    } else {
                        fprintf(stderr, "Error: reached end of input\n");
                    }
                    exit(EXIT_FAILURE);
                }
                memory[state.mem_position] = input;
            }
            break;
        case '[':
            if(memory[state.mem_position] == 0) {
                skip_instructions(loop_level + 1);
            } else {
                run_instructions(loop_level + 1);
            }
            break;
        case ']':
            check_loop_end(loop_level, state.instr_position - 1);
            if(memory[state.mem_position] == 0) {
                return;
            }
            state.instr_position = start;
            break;
        }
    }
    
    check_end_of_program(loop_level, start - 1);
}

static void run_program(void) {
    run_instructions(0);
}

void slow_interpreter_run_program(FILE *f) {
    read_program(f);
    run_program();
}
