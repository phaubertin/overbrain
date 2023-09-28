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

#define _BSD_SOURCE /* For MAP_ANONYMOUS */
#include <sys/mman.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "jit.h"
#include "common/symbols.h"
#include "x86/builder.h"
#include "x86/codegen.h"
#include "x86/encoder.h"
#include "x86/isa.h"

#define MSIZE           30000
#define PLT_ENTRY_SIZE  8
#define GOT_ENTRY_SIZE  (sizeof(uintptr_t))

static const char msg_right[] = "Error: memory position out of bounds (overflow - too far right)\n";
static const char msg_left[] = "Error: memory position out of bounds (underflow - too far left)\n";
/* no end of line (\n) for this one because we are calling perror() instead of fprintf() */
static const char msg_ferr[] = "Error when reading input";
static const char msg_eoi[] = "Error: reached end of input\n";

struct section {
    uintptr_t offset;
    size_t size;
};

enum section_index {
    SECTION_PLT = 0,
    SECTION_TEXT,
    SECTION_RODATA,
    SECTION_GOT,
    SECTION_DATA,
    SECTION_BSS,
    NUM_SECTIONS
};

struct jit_compiled_program {
    jit_main main;
    unsigned char *data;
    struct section sections[NUM_SECTIONS];
};

static jit_compiled_program *allocate_compiled_program(void) {
    jit_compiled_program *compiled = malloc(sizeof(jit_compiled_program));
    
    if(compiled == NULL) {
        fprintf(stderr, "Error: memory allocation (JIT context)\n");
        exit(EXIT_FAILURE);
    }

    memset(compiled, 0, sizeof(jit_compiled_program));
    
    return compiled;
}

typedef enum {
    EXTERN_TYPE_UNUSED = 0,
    EXTERN_TYPE_FUNCTION,
    EXTERN_TYPE_DATA
} extern_type;

struct extern_function {
    extern_type type;
};

typedef enum {
    LOCAL_TYPE_UNUSED = 0,
    LOCAL_TYPE_REFERENCED
} local_type;

struct local_function {
    local_type type;
    size_t size;
    struct x86_encoder_function *encoder_func;
};

static void enumerate_references(
    struct local_function *local_functions,
    struct extern_function *extern_functions,
    const struct x86_function *code
) {
    for(const struct x86_function *func = code; func != NULL; func = func->next) {
        for(const struct x86_instr *instr = func->instrs; instr != NULL; instr = instr->next) {
            if(instr->dst != NULL) {
                switch(instr->dst->type) {
                case X86_OPERAND_EXTERN:
                    extern_functions[instr->dst->n].type = EXTERN_TYPE_FUNCTION;
                    break;
                case X86_OPERAND_MEM64_EXTERN:
                    extern_functions[instr->dst->n].type = EXTERN_TYPE_DATA;
                    break;
                case X86_OPERAND_LOCAL:
                case X86_OPERAND_MEM64_LOCAL:
                    local_functions[instr->dst->n].type = LOCAL_TYPE_REFERENCED;
                    break;
                default:
                    break;
                }
            }
            
            if(instr->src != NULL) {
                switch(instr->src->type) {
                case X86_OPERAND_MEM64_EXTERN:
                    extern_functions[instr->src->n].type = EXTERN_TYPE_DATA;
                    break;
                case X86_OPERAND_LOCAL:
                case X86_OPERAND_MEM64_LOCAL:
                    local_functions[instr->src->n].type = LOCAL_TYPE_REFERENCED;
                    break;
                default:
                    break;
                }
            }
        }
    }
}

static int count_externs_with_type(
    const struct extern_function *extern_functions,
    extern_type type
) {
    int n = 0;
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_functions[idx].type == type) {
            ++n;
        }
    }
    
    return n;
}

static size_t compute_local_function_sizes(
    struct local_function *local_functions,
    const struct x86_function *code,
    const jit_compiled_program *compiled
) {
    uintptr_t start = compiled->sections[SECTION_TEXT].offset;
    uintptr_t offset = start;
    
    for(const struct x86_function *func = code; func != NULL; func = func->next) {
        struct local_function *local_func = &local_functions[func->symbol];
        
        local_func->encoder_func = x86_encoder_function_create(func->instrs, offset);
        local_func->size = x86_encoder_compute_function_size(local_func->encoder_func);
        
        offset += local_func->size;
    }
    
    return offset - start;
}

static size_t compute_rodata_size(const struct local_function *local_functions) {
    size_t size = 0;
    
    if(local_functions[LOCAL_MSG_EOI].type) {
        size += sizeof(msg_eoi);
    }
    
    if(local_functions[LOCAL_MSG_FERR].type) {
        size += sizeof(msg_ferr);
    }
    
    if(local_functions[LOCAL_MSG_LEFT].type) {
        size += sizeof(msg_left);
    }
    
    if(local_functions[LOCAL_MSG_RIGHT].type) {
        size += sizeof(msg_right);
    }
    
    return size;
}

static uintptr_t section_end(const struct section *section) {
    return section->offset + section->size;
}

static void compute_section_sizes(
    jit_compiled_program *compiled,
    struct local_function *local_functions,
    const struct extern_function *extern_functions,
    const struct x86_function *code
) {
    const int num_extern_functions = count_externs_with_type(extern_functions, EXTERN_TYPE_FUNCTION);
    const int num_extern_data = count_externs_with_type(extern_functions, EXTERN_TYPE_DATA);
    
    compiled->sections[SECTION_PLT].offset = 0;
    compiled->sections[SECTION_PLT].size = num_extern_functions * PLT_ENTRY_SIZE;
    
    uintptr_t plt_end = section_end(&compiled->sections[SECTION_PLT]);
    /* align on 16 bytes */
    compiled->sections[SECTION_TEXT].offset = (plt_end + 15) & ~(uintptr_t)15;
    compiled->sections[SECTION_TEXT].size =
        compute_local_function_sizes(local_functions, code, compiled);

    compiled->sections[SECTION_RODATA].offset = section_end(&compiled->sections[SECTION_TEXT]);
    compiled->sections[SECTION_RODATA].size = compute_rodata_size(local_functions);
    
    uintptr_t rodata_end = section_end(&compiled->sections[SECTION_RODATA]);
    long int pagesize = sysconf(_SC_PAGESIZE);

    if(pagesize < 0) {
        fprintf(stderr, "Error: sysconf(_SC_PAGESIZE) failed\n");
        exit(EXIT_FAILURE);
    }

    /* align on next page boundary */
    compiled->sections[SECTION_GOT].offset = (rodata_end + pagesize - 1) & ~(uintptr_t)(pagesize - 1);
    compiled->sections[SECTION_GOT].size = (num_extern_functions + num_extern_data) * GOT_ENTRY_SIZE;

    compiled->sections[SECTION_DATA].offset = section_end(&compiled->sections[SECTION_GOT]);
    compiled->sections[SECTION_DATA].size = sizeof(uintptr_t);

    compiled->sections[SECTION_BSS].offset = section_end(&compiled->sections[SECTION_DATA]);
    compiled->sections[SECTION_BSS].size = MSIZE;
}

static void allocate_memory(jit_compiled_program *compiled) {
    compiled->data = mmap(
        NULL,
        section_end(&compiled->sections[NUM_SECTIONS - 1]),
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if(compiled->data == MAP_FAILED) {
        fprintf(stderr, "Error: memory allocation (mmap() for JIT data)\n");
        exit(EXIT_FAILURE);
    }
}

static struct x86_instr *generate_instructions_for_plt(
    const jit_compiled_program *compiled,
    const struct extern_function *extern_functions

) {
    struct x86_builder builder;
    x86_builder_initialize_empty(&builder);

    uintptr_t *got = (uintptr_t *)compiled->sections[SECTION_GOT].offset;
    
    int plt_index = 0;
    int got_index = 0;
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_functions[idx].type == EXTERN_TYPE_UNUSED) {
            continue;
        }

        if(extern_functions[idx].type == EXTERN_TYPE_FUNCTION) {        
            x86_builder_append_instr(&builder, x86_instr_new_jmp(
                x86_operand_new_mem64_rel((uintptr_t)&got[got_index])
            ));

            x86_builder_append_instr(&builder, x86_instr_new_align(PLT_ENTRY_SIZE));
            
            ++plt_index;
        }

        ++got_index;
    }
    
    return x86_builder_get_first(&builder);
}

static void write_process_linkage_table(
    const jit_compiled_program *compiled,
    const struct extern_function *extern_functions
) {
    uintptr_t offset = compiled->sections[SECTION_PLT].offset;
    size_t size = compiled->sections[SECTION_PLT].size;
    
    struct x86_instr *instrs = generate_instructions_for_plt(compiled, extern_functions);
    
    x86_encoder_function *func = x86_encoder_function_create(instrs, offset);
    
    if(x86_encoder_compute_function_size(func) != size) {
        fprintf(stderr, "Error: PLT generation (wrong size)\n");
        exit(EXIT_FAILURE);
    }

    x86_encoder_context dummy_context; 
    unsigned char *plt = compiled->data + offset;
    encode_for_x86(plt, size, func, &dummy_context);

    x86_encoder_function_free(func);
    x86_instr_free_tree(instrs);
}

static void initialize_encoder_context(
    x86_encoder_context *context,
    const struct jit_compiled_program *compiled,
    const struct x86_function *code,
    const struct local_function *local_functions,
    const struct extern_function *extern_functions
) {
    /* external symbols */
    
    const char *plt = (const char *)compiled->sections[SECTION_PLT].offset;
    const uintptr_t *got = (uintptr_t *)compiled->sections[SECTION_GOT].offset;
    
    int plt_offset = 0;
    int got_index = 0;
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_functions[idx].type == EXTERN_TYPE_UNUSED) {
            continue;
        }

        if(extern_functions[idx].type == EXTERN_TYPE_FUNCTION) {        
            context->externs[idx] = (intptr_t)&plt[plt_offset];
            plt_offset += PLT_ENTRY_SIZE;
        }

        if(extern_functions[idx].type == EXTERN_TYPE_DATA) {        
            context->externs[idx] = (intptr_t)&got[got_index];
        }

        ++got_index;
    }
    
    /* local symbols - functions */
    for(const struct x86_function *func = code; func != NULL; func = func->next) {
        struct x86_encoder_function *encoder_func = local_functions[func->symbol].encoder_func;
        context->locals[func->symbol] = x86_encoder_function_get_address(encoder_func);
    }
    
    /* local symbols - data */
    
    const char *rodata = (const char *)compiled->sections[SECTION_RODATA].offset;
    int rodata_index = 0;
    
    if(local_functions[LOCAL_MSG_EOI].type) {
        context->locals[LOCAL_MSG_EOI] = (intptr_t)&rodata[rodata_index];
        rodata_index += sizeof(msg_eoi);
    }
    
    if(local_functions[LOCAL_MSG_FERR].type) {
        context->locals[LOCAL_MSG_FERR] = (intptr_t)&rodata[rodata_index];
        rodata_index += sizeof(msg_ferr);
    }
    
    if(local_functions[LOCAL_MSG_LEFT].type) {
        context->locals[LOCAL_MSG_LEFT] = (intptr_t)&rodata[rodata_index];
        rodata_index += sizeof(msg_left);
    }
    
    if(local_functions[LOCAL_MSG_RIGHT].type) {
        context->locals[LOCAL_MSG_RIGHT] = (intptr_t)&rodata[rodata_index];
        rodata_index += sizeof(msg_right);
    }
    
    context->locals[LOCAL_M] = compiled->sections[SECTION_DATA].offset;
}

static void write_text_section(
    const struct jit_compiled_program *compiled,
    const struct x86_function *code,
    const struct local_function *local_functions,
    const struct extern_function *extern_functions
) {
    x86_encoder_context context;
    initialize_encoder_context(&context, compiled, code, local_functions, extern_functions);

    unsigned char *const text = compiled->data + compiled->sections[SECTION_TEXT].offset;
    int offset = 0;

    for(const struct x86_function *func = code; func != NULL; func = func->next) {
        size_t size = local_functions[func->symbol].size;

        encode_for_x86(&text[offset], size, local_functions[func->symbol].encoder_func, &context);
        
        offset += size;
    }
}

static void write_rodata_section(
    const struct jit_compiled_program *compiled,
    const struct local_function *local_functions
) {
    unsigned char *dest = compiled->data + compiled->sections[SECTION_RODATA].offset;
    
    if(local_functions[LOCAL_MSG_EOI].type) {
        memcpy(dest, msg_eoi, sizeof(msg_eoi));
        dest += sizeof(msg_eoi);
    }
    
    if(local_functions[LOCAL_MSG_FERR].type) {
        memcpy(dest, msg_ferr, sizeof(msg_ferr));
        dest += sizeof(msg_ferr);
    }
    
    if(local_functions[LOCAL_MSG_LEFT].type) {
        memcpy(dest, msg_left, sizeof(msg_left));
        dest += sizeof(msg_left);
    }
    
    if(local_functions[LOCAL_MSG_RIGHT].type) {
        memcpy(dest, msg_right, sizeof(msg_right));
        dest += sizeof(msg_right);
    }
}

static void write_got_section(
    const struct jit_compiled_program *compiled,
    const struct extern_function *extern_functions
) {
    uintptr_t *got = (uintptr_t *)(compiled->data + compiled->sections[SECTION_GOT].offset);

    int got_index = 0;
    
    for(extern_symbol symbol = 0; symbol < NUM_EXTERN_SYMBOLS; ++symbol) {
        if(extern_functions[symbol].type == EXTERN_TYPE_UNUSED) {
            continue;
        }

        switch(symbol) {
        case EXTERN_EXIT:
            got[got_index] = (uintptr_t)exit;
            break;
        case EXTERN_FERROR:
            got[got_index] = (uintptr_t)ferror;
            break;
        case EXTERN_FGETC:
            got[got_index] = (uintptr_t)fgetc;
            break;
        case EXTERN_FPRINTF:
            got[got_index] = (uintptr_t)fprintf;
            break;
        case EXTERN_LIBC_START_MAIN:
            /* Called by _start which isn't used in JIT context. */
            break;
        case EXTERN_PERROR:
            got[got_index] = (uintptr_t)perror;
            break;
        case EXTERN_PUTC:
            got[got_index] = (uintptr_t)putc;
            break;
        case EXTERN_STDERR:
            got[got_index] = (uintptr_t)stderr;
            break;
        case EXTERN_STDIN:
            got[got_index] = (uintptr_t)stdin;
            break;
        case EXTERN_STDOUT:
            got[got_index] = (uintptr_t)stdout;
            break;
        }
        
        ++got_index;
    }
}

static void write_data_section(const struct jit_compiled_program *compiled) {
    unsigned char **m = (unsigned char **)(compiled->data + compiled->sections[SECTION_DATA].offset);
    *m = compiled->data + compiled->sections[SECTION_BSS].offset;
}

static void protect_and_make_executable(jit_compiled_program *compiled) {
    int status = mprotect(
        compiled->data,
        section_end(&compiled->sections[SECTION_RODATA]),
        PROT_READ | PROT_EXEC
    );

    if(status < 0) {
        fprintf(stderr, "Error: mprotect() failed\n");
        exit(EXIT_FAILURE);
    }
}

static void cleanup_code(
    struct x86_function *func,
    struct local_function *local_functions
) {
    
    while(func != NULL) {
        x86_encoder_function_free(local_functions[func->symbol].encoder_func);
        
        struct x86_function *next = func->next;
        x86_function_free(func);

        func = next;
    }
}

typedef void (*nullfuncptr)(void);

static nullfuncptr get_local_function_address(
    local_symbol symbol,
    const jit_compiled_program *compiled,
    const struct local_function *local_functions
) {
    const x86_encoder_function *func = local_functions[symbol].encoder_func;
    void *retval = compiled->data + x86_encoder_function_get_address(func);

    /* A just-in-time compiler has to cast an object pointer to a function pointer at some point,
     * right? */
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-pedantic"
#endif
    return (nullfuncptr)retval;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}

jit_compiled_program *jit_compiled_program_create(const struct node *program) {
    jit_compiled_program *compiled = allocate_compiled_program();

    struct x86_function *code = generate_code_for_x86(program);

    struct local_function local_functions[NUM_LOCAL_SYMBOLS];
    struct extern_function extern_functions[NUM_EXTERN_SYMBOLS];
    memset(local_functions, 0, sizeof(local_functions));
    memset(extern_functions, 0, sizeof(extern_functions));

    enumerate_references(local_functions, extern_functions, code);

    compute_section_sizes(compiled, local_functions, extern_functions, code);

    allocate_memory(compiled);

    write_process_linkage_table(compiled, extern_functions);

    write_text_section(compiled, code, local_functions, extern_functions);

    write_rodata_section(compiled, local_functions);

    write_got_section(compiled, extern_functions);

    write_data_section(compiled);

    protect_and_make_executable(compiled);
    
    compiled->main = (jit_main)get_local_function_address(LOCAL_MAIN, compiled, local_functions);

    cleanup_code(code, local_functions);

    return compiled;
}

void jit_compiled_program_free(jit_compiled_program *compiled) {
    free(compiled);
}

jit_main jit_compiled_program_get_main(const jit_compiled_program *compiled) {
    return compiled->main;
}
