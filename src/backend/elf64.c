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
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common/symbols.h"
#include "x86/builder.h"
#include "x86/codegen.h"
#include "x86/encoder.h"
#include "x86/isa.h"
#include "elf64.h"
#include "elf64defs.h"

#define MSIZE 30000
#define NUM_HASH_BUCKETS 3
#define NUM_PHDRS 6
#define NUM_SECTIONS 17
#define TEXT_PHDR_BASE_ADDR 0x400000
#define DATA_PHDR_BASE_ADDR 0x600000
#define SYMBOL_VERSION_ID 2
#define SHTAB_ALIGNMENT 8

const char interp[] = "/lib64/ld-linux-x86-64.so.2";
const char glibc_225[] = "GLIBC_2.2.5";
const char libcso6[] = "libc.so.6";

const char msg_right[] = "Error: memory position out of bounds (overflow - too far right)\n";
const char msg_left[] = "Error: memory position out of bounds (underflow - too far left)\n";
/* no end of line (\n) for this one because we are calling perror() instead of fprintf() */
const char msg_ferr[] = "Error when reading input";
const char msg_eoi[] = "Error: reached end of input\n";

enum dynamic_index {
    DYNAMIC_NEEDED = 0,
    DYNAMIC_HASH = 1,
    DYNAMIC_STRTAB = 2,
    DYNAMIC_SYMTAB = 3,
    DYNAMIC_STRSZ = 4,
    DYNAMIC_SYMENT = 5,
    DYNAMIC_DEBUG = 6,
    DYNAMIC_PLTGOT = 7,
    DYNAMIC_PLTRELSZ = 8,
    DYNAMIC_PLTREL = 9,
    DYNAMIC_JMPREL = 10,
    DYNAMIC_RELA = 11,
    DYNAMIC_RELASZ = 12,
    DYNAMIC_RELAENT = 13,
    DYNAMIC_VERNEED = 14,
    DYNAMIC_VERNEEDNUM = 15,
    DYNAMIC_VERSYM = 16,
    DYNAMIC_NULL = 17,
};

Elf64_Dyn dynamic[] = {
    [DYNAMIC_NEEDED] = {
        .d_tag = DT_NEEDED,
        .d_un = { .d_val = 0 }
    },
    [DYNAMIC_HASH] = {
        .d_tag = DT_HASH,
        .d_un = { .d_ptr = 0 }
    },
    [DYNAMIC_STRTAB] = {
        .d_tag = DT_STRTAB,
        .d_un = { .d_ptr = 0 }
    },
    [DYNAMIC_SYMTAB] = {
        .d_tag = DT_SYMTAB,
        .d_un = { .d_ptr = 0 }
    },
    [DYNAMIC_STRSZ] = {
        .d_tag = DT_STRSZ,
        .d_un = { .d_val = 0 }
    },
    [DYNAMIC_SYMENT] = {
        .d_tag = DT_SYMENT,
        .d_un = { .d_val = 0 }
    },
    [DYNAMIC_DEBUG] = {
        .d_tag = DT_DEBUG,
        .d_un = { .d_ptr = 0 }
    },
    [DYNAMIC_PLTGOT] = {
        .d_tag = DT_PLTGOT,
        .d_un = { .d_ptr = 0 }
    },
    [DYNAMIC_PLTRELSZ] = {
        .d_tag = DT_PLTRELSZ,
        .d_un = { .d_val = 0 }
    },
    [DYNAMIC_PLTREL] = {
        .d_tag = DT_PLTREL,
        .d_un = { .d_val = 0 }
    },
    [DYNAMIC_JMPREL] = {
        .d_tag = DT_JMPREL,
        .d_un = { .d_ptr = 0 }
    },
    [DYNAMIC_RELA] = {
        .d_tag = DT_RELA,
        .d_un = { .d_ptr = 0 }
    },
    [DYNAMIC_RELASZ] = {
        .d_tag = DT_RELASZ,
        .d_un = { .d_val = 0 }
    },
    [DYNAMIC_RELAENT] = {
        .d_tag = DT_RELAENT,
        .d_un = { .d_val = 0 }
    },
    [DYNAMIC_VERNEED] = {
        .d_tag = DT_GNU_VERNEED,
        .d_un = { .d_ptr = 0 }
    },
    [DYNAMIC_VERNEEDNUM] = {
        .d_tag = DT_GNU_VERNEEDNUM,
        .d_un = { .d_val = 0 }
    },
    [DYNAMIC_VERSYM] = {
        .d_tag = DT_GNU_VERSYM,
        .d_un = { .d_ptr = 0 }
    },
    [DYNAMIC_NULL] = {0}
};

enum section_index {
    /* SHN_UNDEF = 0,*/
    SECTION_INTERP = 1,
    SECTION_HASH = 2,
    SECTION_DYNSYM = 3,
    SECTION_DYNSTR = 4,
    SECTION_GNU_VERSYM = 5,
    SECTION_GNU_VERNEED = 6,
    SECTION_RELA_DYN = 7,
    SECTION_RELA_PLT = 8,
    SECTION_PLT = 9,
    SECTION_TEXT = 10,
    SECTION_RODATA = 11,
    SECTION_DYNAMIC = 12,
    SECTION_PLTGOT = 13,
    SECTION_DATA = 14,
    SECTION_BSS = 15,
    SECTION_SHSTRTAB = 16
};

const char *section_names[] = {
    [SHN_UNDEF] = "",
    [SECTION_INTERP] = ".interp",
    [SECTION_HASH] = ".hash",
    [SECTION_DYNSYM] = ".dynsym",
    [SECTION_DYNSTR] = ".dynstr",
    [SECTION_GNU_VERSYM] = ".gnu.version",
    [SECTION_GNU_VERNEED] = ".gnu.version_r",
    [SECTION_RELA_DYN] = ".rela.dyn",
    [SECTION_RELA_PLT] = ".rela.plt",
    [SECTION_PLT] = ".plt",
    [SECTION_TEXT] = ".text",
    [SECTION_RODATA] = ".rodata",
    [SECTION_DYNAMIC] = ".dynamic",
    [SECTION_PLTGOT] = ".got.plt",
    [SECTION_DATA] = ".data",
    [SECTION_BSS] = ".bss",
    [SECTION_SHSTRTAB] = ".shstrtab"
};
    
Elf64_Shdr sections[] = {
    [SHN_UNDEF] = {0},
    [SECTION_INTERP] = {
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_ALLOC,
        .sh_link = SHN_UNDEF,
        .sh_info = 0,
        .sh_addralign = 1,
        .sh_entsize = 0,
        .sh_size = sizeof(interp),
        /* will be computed later */
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_HASH] = {
        .sh_type = SHT_HASH,
        .sh_flags = SHF_ALLOC,
        .sh_link = SECTION_DYNSYM,
        .sh_info = 0,
        .sh_addralign = 8,
        .sh_entsize = sizeof(Elf64_Word),
        /* these will be computed later */
        .sh_size = 0,
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_DYNSYM] = {
        .sh_type = SHT_DYNSYM,
        .sh_flags = SHF_ALLOC,
        .sh_link = SECTION_DYNSTR,
        .sh_info = 1,
        .sh_addralign = 8,
        .sh_entsize = sizeof(Elf64_Sym),
        /* these will be computed later */
        .sh_size = 0,
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_DYNSTR] = {
        .sh_type = SHT_STRTAB,
        .sh_flags = SHF_ALLOC,
        .sh_link = SHN_UNDEF,
        .sh_info = 0,
        .sh_addralign = 1,
        .sh_entsize = 0,
        /* these will be computed later */
        .sh_size = 0,
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_GNU_VERSYM] = {
        .sh_type = SHT_GNU_VERSYM,
        .sh_flags = SHF_ALLOC,
        .sh_link = SECTION_DYNSYM,
        .sh_info = 0,
        .sh_addralign = 2,
        .sh_entsize = sizeof(Elf64_Half),
        /* these will be computed later */
        .sh_size = 0,
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_GNU_VERNEED] = {
        .sh_type = SHT_GNU_VERNEED,
        .sh_flags = SHF_ALLOC,
        .sh_link = SECTION_DYNSTR,
        .sh_info = 1,
        .sh_addralign = 8,
        .sh_entsize = 0,
        .sh_size = sizeof(Elf64_Verneed) + sizeof(Elf64_Vernaux),
        /* these will be computed later */
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_RELA_DYN] = {
        .sh_type = SHT_RELA,
        .sh_flags = SHF_ALLOC,
        .sh_link = SECTION_DYNSYM,
        .sh_info = 0,
        .sh_addralign = 8,
        .sh_entsize = sizeof(Elf64_Rela),
        /* these will be computed later */
        .sh_size = 0,
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_RELA_PLT] = {
        .sh_type = SHT_RELA,
        .sh_flags = SHF_ALLOC | SHF_INFO_LINK,
        .sh_link = SECTION_DYNSYM,
        .sh_info = SECTION_PLTGOT,
        .sh_addralign = 8,
        .sh_entsize = sizeof(Elf64_Rela),
        /* these will be computed later */
        .sh_size = 0,
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_PLT] = {
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_ALLOC | SHF_EXECINSTR,
        .sh_link = SHN_UNDEF,
        .sh_info = 0,
        .sh_addralign = 16,
        .sh_entsize = 16,
        /* these will be computed later */
        .sh_size = 0,
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_TEXT] = {
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_ALLOC | SHF_EXECINSTR,
        .sh_link = SHN_UNDEF,
        .sh_info = 0,
        .sh_addralign = 16,
        .sh_entsize = 0,
        /* these will be computed later */
        .sh_size = 0,
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_RODATA] = {
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_ALLOC,
        .sh_link = SHN_UNDEF,
        .sh_info = 0,
        .sh_addralign = 4,
        .sh_entsize = 0,
        /* these will be computed later */
        .sh_size = 0,
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_DYNAMIC] = {
        .sh_type = SHT_DYNAMIC,
        .sh_flags = SHF_WRITE | SHF_ALLOC,
        .sh_link = SECTION_DYNSTR,
        .sh_info = 0,
        .sh_addralign = 8,
        .sh_entsize = sizeof(Elf64_Dyn),
        .sh_size = sizeof(dynamic),
        /* these will be computed later */
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_PLTGOT] = {
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_WRITE | SHF_ALLOC,
        .sh_link = SHN_UNDEF,
        .sh_info = 0,
        .sh_addralign = 4096,
        .sh_entsize = sizeof(Elf64_Addr),
        /* these will be computed later */
        .sh_size = 0,
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_DATA] = {
        .sh_type = SHT_PROGBITS,
        .sh_flags = SHF_WRITE | SHF_ALLOC,
        .sh_link = SHN_UNDEF,
        .sh_info = 0,
        .sh_addralign = 4,
        .sh_entsize = 0,
        .sh_size = 8,
        /* these will be computed later */
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_BSS] = {
        .sh_type = SHT_NOBITS,
        .sh_flags = SHF_WRITE | SHF_ALLOC,
        .sh_link = SHN_UNDEF,
        .sh_info = 0,
        .sh_addralign = 16,
        .sh_entsize = 0,
        /* these will be computed later */
        .sh_size = 0,
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    },
    [SECTION_SHSTRTAB] = {
        .sh_type = SHT_STRTAB,
        .sh_flags = 0,
        .sh_link = SHN_UNDEF,
        .sh_info = 0,
        .sh_addralign = 1,
        .sh_entsize = 0,
        /* these will be computed later */
        .sh_size = 0,
        .sh_name = 0,
        .sh_addr = 0,
        .sh_offset = 0
    }
};

struct write_state {
    FILE *f;
    Elf64_Off offset;
};

static void initialize_write_state(struct write_state *state, FILE *f) {
    state->f = f;
    state->offset = 0;
}

static void write_bytes(struct write_state *state, const void *bytes, size_t nbytes) {
    fwrite(bytes, 1, nbytes, state->f);
    
    if(ferror(state->f)) {
        perror("Error: file write error");
        exit(EXIT_FAILURE);
    }
    
    state->offset += nbytes;
}

static void align_state(struct write_state *state, Elf64_Xword alignment) {
    while((state->offset & (alignment - 1)) != 0) {
        char zero = 0;
        write_bytes(state, &zero, 1);
    }
}

static void start_section(struct write_state *state, int index) {
    align_state(state, sections[index].sh_addralign);
    
    if(state->offset != sections[index].sh_offset) {
        fprintf(
            stderr,
            "Error: incorrect offset at start of section %s (expected: %" PRIu64 " actual: %" PRIu64 ")\n",
            section_names[index],
            sections[index].sh_offset,
            state->offset
        );
        exit(EXIT_FAILURE);
    }
}

#define MAX_LOCAL_FUNCTIONS 5

struct local_function {
    local_symbol symbol;
    size_t size;
    struct x86_instr *instrs;
    struct x86_encoder_function *encoder_func;
};

struct local_functions {
    size_t n;
    struct local_function funcs[MAX_LOCAL_FUNCTIONS];
};

static void generate_local_functions(struct local_functions *local_functions, const struct node *root) {
    struct x86_instr *main_instrs = generate_code_for_x86(root);
    
    bool has_fail_right = false;
    bool has_fail_left = false;
    bool has_check_input = false;
    
    for(const struct x86_instr *instr = main_instrs; instr != NULL; instr = instr->next) {
        if(instr->op == X86_INSTR_CALL && instr->dst->type == X86_OPERAND_LOCAL) {
            switch(instr->dst->n) {
            case LOCAL_FAIL_TOO_FAR_RIGHT:
                has_fail_right = true;
                break;
            case LOCAL_FAIL_TOO_FAR_LEFT:
                has_fail_left = true;
                break;
            case LOCAL_CHECK_INPUT:
                has_check_input = true;
                break;
            default:
                fprintf(
                    stderr,
                    "Error: (in elf64 backend) missing support for local function %s()\n",
                    local_symbol_names[instr->dst->n]
                );
                exit(EXIT_FAILURE);
            }
        }
    }
    
    int index = 0;
    
    local_functions->funcs[index].symbol = LOCAL_START;
    local_functions->funcs[index].instrs = generate_start_for_x86();
    ++index;
    
    local_functions->funcs[index].symbol = LOCAL_MAIN;
    local_functions->funcs[index].instrs = main_instrs;
    ++index;
    
    if(has_fail_right) {
        local_functions->funcs[index].symbol = LOCAL_FAIL_TOO_FAR_RIGHT;
        local_functions->funcs[index].instrs = generate_fail_too_far_right_for_x86();
        ++index;
    }
    
    if(has_fail_left) {
        local_functions->funcs[index].symbol = LOCAL_FAIL_TOO_FAR_LEFT;
        local_functions->funcs[index].instrs = generate_fail_too_far_left_for_x86();
        ++index;
    }
    
    if(has_check_input) {
        local_functions->funcs[index].symbol = LOCAL_CHECK_INPUT;
        local_functions->funcs[index].instrs = generate_check_input_for_x86();
        ++index;
    }
    
    local_functions->n = index;
}

typedef enum {
    EXTERN_TYPE_UNUSED = 0,
    EXTERN_TYPE_FUNCTION,
    EXTERN_TYPE_DATA
} extern_type;

typedef enum {
    LOCAL_TYPE_UNUSED = 0,
    LOCAL_TYPE_REFERENCED
} local_type;

static void enumerate_references(
    local_type *local_types,
    extern_type *extern_types,
    const struct local_functions *local_functions
) {
    memset(extern_types, 0, NUM_LOCAL_SYMBOLS * sizeof(local_type));
    memset(extern_types, 0, NUM_EXTERN_SYMBOLS * sizeof(extern_type));
    
    for(int idx = 0; idx < local_functions->n; ++idx) {
        const struct x86_instr *instr = local_functions->funcs[idx].instrs;
        
        while(instr != NULL) {
            if(instr->dst != NULL) {
                switch(instr->dst->type) {
                case X86_OPERAND_EXTERN:
                    extern_types[instr->dst->n] = EXTERN_TYPE_FUNCTION;
                    break;
                case X86_OPERAND_MEM64_EXTERN:
                    extern_types[instr->dst->n] = EXTERN_TYPE_DATA;
                    break;
                case X86_OPERAND_LOCAL:
                case X86_OPERAND_MEM64_LOCAL:
                    local_types[instr->dst->n] = LOCAL_TYPE_REFERENCED;
                    break;
                default:
                    break;
                }
            }
            
            if(instr->src != NULL) {
                switch(instr->src->type) {
                case X86_OPERAND_MEM64_EXTERN:
                    extern_types[instr->src->n] = EXTERN_TYPE_DATA;
                    break;
                case X86_OPERAND_LOCAL:
                case X86_OPERAND_MEM64_LOCAL:
                    local_types[instr->src->n] = LOCAL_TYPE_REFERENCED;
                    break;
                default:
                    break;
                }
            }
            
            instr = instr->next;
        }
    }
}

static int count_externs_with_type(const extern_type *extern_types, extern_type type) {
    int n = 0;
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_types[idx] == type) {
            ++n;
        }
    }
    
    return n;
}

static int count_externs(const extern_type *extern_types) {
    int n = 0;
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_types[idx] != EXTERN_TYPE_UNUSED) {
            ++n;
        }
    }
    
    return n;
}

static int compute_dynstr_size(const extern_type *extern_types) {
    /* one because there is a leading empty string */
    int size = 1 + sizeof(libcso6) + sizeof(glibc_225);
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_types[idx] != EXTERN_TYPE_UNUSED) {
            size += strlen(extern_symbol_names[idx]) + 1;
        }
    }
    
    return size;
}

static Elf64_Off align_offset(Elf64_Off offset, Elf64_Xword alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
}

static void compute_addresses_up_to_text_section(const extern_type *extern_types) {
    const int num_extern_functions = count_externs_with_type(extern_types, EXTERN_TYPE_FUNCTION);
    const int num_extern_data = count_externs_with_type(extern_types, EXTERN_TYPE_DATA);
    
    /* plus one for reserved STN_UNDEF entry */
    const int num_dynsyms = num_extern_functions + num_extern_data + 1;
    
    sections[SECTION_HASH].sh_size = (2 + NUM_HASH_BUCKETS + num_dynsyms) * sizeof(Elf64_Word);
    sections[SECTION_DYNSYM].sh_size = num_dynsyms * sections[SECTION_DYNSYM].sh_entsize;
    sections[SECTION_DYNSTR].sh_size = compute_dynstr_size(extern_types);
    sections[SECTION_GNU_VERSYM].sh_size = num_dynsyms * sections[SECTION_GNU_VERSYM].sh_entsize;
    sections[SECTION_RELA_DYN].sh_size = num_extern_data * sections[SECTION_RELA_DYN].sh_entsize;
    sections[SECTION_RELA_PLT].sh_size = num_extern_functions * sections[SECTION_RELA_PLT].sh_entsize;
    sections[SECTION_PLT].sh_size = (num_extern_functions + 1) * sections[SECTION_PLT].sh_entsize;
    
    sections[SECTION_INTERP].sh_offset = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr[NUM_PHDRS]);
    sections[SECTION_INTERP].sh_addr = sections[SECTION_INTERP].sh_offset + TEXT_PHDR_BASE_ADDR;
    
    for(int idx = SECTION_HASH; idx <= SECTION_TEXT; ++idx) {
        Elf64_Off previous_end = sections[idx - 1].sh_offset + sections[idx - 1].sh_size;
        Elf64_Xword alignment = sections[idx].sh_addralign;
        
        sections[idx].sh_offset = align_offset(previous_end, alignment);
        sections[idx].sh_addr = sections[idx].sh_offset + TEXT_PHDR_BASE_ADDR;
    }
}

static void compute_local_functions_sizes(struct local_functions *local_functions) {
    int start_address = sections[SECTION_TEXT].sh_addr;
    int address = start_address;
    
    for(int idx = 0; idx < local_functions->n; ++idx) {
        struct local_function *func = &local_functions->funcs[idx];
        
        func->encoder_func = x86_encoder_function_create(func->instrs, address);
        func->size = x86_encoder_compute_function_size(func->encoder_func);
        
        address += func->size;
    }
    
    sections[SECTION_TEXT].sh_size = address - start_address;
}

static size_t compute_rodata_size(const local_type *local_types) {
    size_t size = 0;
    
    if(local_types[LOCAL_MSG_EOI]) {
        size += sizeof(msg_eoi);
    }
    
    if(local_types[LOCAL_MSG_FERR]) {
        size += sizeof(msg_ferr);
    }
    
    if(local_types[LOCAL_MSG_LEFT]) {
        size += sizeof(msg_left);
    }
    
    if(local_types[LOCAL_MSG_RIGHT]) {
        size += sizeof(msg_right);
    }
    
    return size;
}

static int compute_shstrtab_size(void) {
    /* one because there is a leading empty string */
    int size = 1;
    
    /* start index 1: skip the NULL section at the beginning */
    for(int idx = 1; idx < NUM_SECTIONS; ++idx) {
        size += strlen(section_names[idx]) + 1;
    }
    
    return size;
}

static void compute_remaining_section_addresses(
    const local_type *local_types,
    const extern_type *extern_types
) {
    /* There are three reserved entries defined by the ELF spec for X86_64. */
    int plt_got_entries = count_externs_with_type(extern_types, EXTERN_TYPE_FUNCTION) + 3;
    int data_got_entries = count_externs_with_type(extern_types, EXTERN_TYPE_DATA);
    
    sections[SECTION_RODATA].sh_size = compute_rodata_size(local_types);
    sections[SECTION_PLTGOT].sh_size = plt_got_entries * sections[SECTION_PLTGOT].sh_entsize;
    sections[SECTION_BSS].sh_size = data_got_entries * sizeof(Elf64_Addr) + MSIZE;
    sections[SECTION_SHSTRTAB].sh_size = compute_shstrtab_size();

    for(int idx = SECTION_RODATA; idx < NUM_SECTIONS; ++idx) {
        Elf64_Off previous_end_offset;
        
        if(sections[idx - 1].sh_type == SHT_NOBITS) {
            previous_end_offset = sections[idx - 1].sh_offset;
        } else {
            previous_end_offset = sections[idx - 1].sh_offset + sections[idx - 1].sh_size;
        }
        
        Elf64_Xword alignment = sections[idx].sh_addralign;
        
        if(sections[idx].sh_type == SHT_NOBITS) {
            sections[idx].sh_offset = previous_end_offset;
        } else {
            sections[idx].sh_offset = align_offset(previous_end_offset, alignment);
        }
        
        if(idx >= SECTION_SHSTRTAB) {
            sections[idx].sh_addr = 0;
        } else {
            Elf64_Addr previous_end_addr = sections[idx - 1].sh_addr + sections[idx - 1].sh_size;
            sections[idx].sh_addr = align_offset(previous_end_addr, alignment);
            
            if(idx == SECTION_DYNAMIC) {
                sections[idx].sh_addr += DATA_PHDR_BASE_ADDR - TEXT_PHDR_BASE_ADDR;
            }
        }
    }
}

static Elf64_Off compute_section_headers_offset(void) {
    Elf64_Off shstrtab_end = sections[SECTION_SHSTRTAB].sh_offset + sections[SECTION_SHSTRTAB].sh_size;
    return align_offset(shstrtab_end, SHTAB_ALIGNMENT);
}

void write_elf_header(struct write_state *state) {
    Elf64_Ehdr ehdr;
    
    memset(&ehdr, 0, sizeof(ehdr));
    ehdr.e_ident[EI_MAG0] = 0x7f;
    ehdr.e_ident[EI_MAG1] = 'E';
    ehdr.e_ident[EI_MAG2] = 'L';
    ehdr.e_ident[EI_MAG3] = 'F';
    ehdr.e_ident[EI_CLASS] = ELFCLASS64;
    ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = 1;
    ehdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
    ehdr.e_ident[EI_ABIVERSION] = 0;
    
    ehdr.e_type = ET_EXEC;
    ehdr.e_machine = EM_X86_64;
    ehdr.e_version = 1;
    ehdr.e_entry = sections[SECTION_TEXT].sh_addr;
    ehdr.e_phoff = sizeof(ehdr);
    ehdr.e_shoff = compute_section_headers_offset();
    ehdr.e_flags = 0;
    ehdr.e_ehsize = sizeof(ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum = NUM_PHDRS;
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum = NUM_SECTIONS;
    ehdr.e_shstrndx = SECTION_SHSTRTAB;
    
    write_bytes(state, &ehdr, sizeof(ehdr));
}

void write_program_headers(struct write_state *state) {
    Elf64_Phdr phdrs[NUM_PHDRS];
    
    int index = 0;
    /* program header */
    phdrs[index].p_type = PT_PHDR,
    phdrs[index].p_flags = PF_R | PF_X,
    phdrs[index].p_align = 8,
    phdrs[index].p_offset = sizeof(Elf64_Ehdr),
    phdrs[index].p_vaddr = TEXT_PHDR_BASE_ADDR + sizeof(Elf64_Ehdr),
    phdrs[index].p_paddr = TEXT_PHDR_BASE_ADDR + sizeof(Elf64_Ehdr),
    phdrs[index].p_filesz = sizeof(phdrs);
    phdrs[index].p_memsz = sizeof(phdrs);
    ++index;
    
    /* interpreter */
    phdrs[index].p_type = PT_INTERP,
    phdrs[index].p_flags = PF_R,
    phdrs[index].p_align = 1,
    phdrs[index].p_filesz = sections[SECTION_INTERP].sh_size;
    phdrs[index].p_memsz = sections[SECTION_INTERP].sh_size;
    phdrs[index].p_offset = sections[SECTION_INTERP].sh_offset;
    phdrs[index].p_vaddr = sections[SECTION_INTERP].sh_addr;
    phdrs[index].p_paddr = sections[SECTION_INTERP].sh_addr;
    ++index;
    
    /* text/read-only */
    phdrs[index].p_type = PT_LOAD,
    phdrs[index].p_flags = PF_R | PF_X,
    phdrs[index].p_align = 0x200000,
    phdrs[index].p_filesz = sections[SECTION_RODATA].sh_offset + sections[SECTION_RODATA].sh_size;
    phdrs[index].p_memsz = sections[SECTION_RODATA].sh_offset + sections[SECTION_RODATA].sh_size;
    phdrs[index].p_offset = 0;
    phdrs[index].p_vaddr = TEXT_PHDR_BASE_ADDR;
    phdrs[index].p_paddr = TEXT_PHDR_BASE_ADDR;
    ++index;
    
    Elf64_Addr file_start = sections[SECTION_DYNAMIC].sh_offset;
    Elf64_Addr file_end = sections[SECTION_DATA].sh_offset + sections[SECTION_DATA].sh_size;
    Elf64_Addr mem_start = sections[SECTION_DYNAMIC].sh_addr;
    Elf64_Addr mem_end = sections[SECTION_BSS].sh_addr + sections[SECTION_BSS].sh_size;
    /* read/write data */
    phdrs[index].p_type = PT_LOAD,
    phdrs[index].p_flags = PF_R | PF_W,
    phdrs[index].p_align = 0x200000,
    phdrs[index].p_filesz = file_end - file_start;
    phdrs[index].p_memsz = mem_end - mem_start;
    phdrs[index].p_offset = sections[SECTION_DYNAMIC].sh_offset;
    phdrs[index].p_vaddr = sections[SECTION_DYNAMIC].sh_addr;
    phdrs[index].p_paddr = sections[SECTION_DYNAMIC].sh_addr;
    ++index;
    
    /* dynamic section */
    phdrs[index].p_type = PT_DYNAMIC,
    phdrs[index].p_flags = PF_R | PF_W,
    phdrs[index].p_align = 8,
    phdrs[index].p_filesz = sections[SECTION_DYNAMIC].sh_size;
    phdrs[index].p_memsz = sections[SECTION_DYNAMIC].sh_size;
    phdrs[index].p_offset = sections[SECTION_DYNAMIC].sh_offset;
    phdrs[index].p_vaddr = sections[SECTION_DYNAMIC].sh_addr;
    phdrs[index].p_paddr = sections[SECTION_DYNAMIC].sh_addr;
    ++index;
    
    /* GNU relro */
    phdrs[index].p_type = PT_GNU_RELRO,
    phdrs[index].p_flags = PF_R,
    phdrs[index].p_align = 1,
    phdrs[index].p_filesz = sections[SECTION_DYNAMIC].sh_size;
    phdrs[index].p_memsz = sections[SECTION_DYNAMIC].sh_size;
    phdrs[index].p_offset = sections[SECTION_DYNAMIC].sh_offset;
    phdrs[index].p_vaddr = sections[SECTION_DYNAMIC].sh_addr;
    phdrs[index].p_paddr = sections[SECTION_DYNAMIC].sh_addr;
    ++index;
    
    write_bytes(state, phdrs, sizeof(phdrs));
}

static void write_interpreter_section(struct write_state *state) {
    start_section(state, SECTION_INTERP);
    write_bytes(state, interp, sizeof(interp));
}

/* "ELF-64 Object File Format" Version 1.5 Draft 2 (May 27, 1998) section 11
 * "Hash table" Figure 10 "Hash Function" */
static unsigned long elf64_hash(const unsigned char *name) {
    unsigned long h = 0, g;
    
    while (*name) {
        h = (h << 4) + *name++;
        g = h & 0xf0000000;
        if(g) {
            h ^= g >> 24;
        }
        h &= 0x0fffffff;
    }
    return h;
}

static void write_hash_section(struct write_state *state, const extern_type *extern_types) {
    struct {
        Elf64_Word nbucket;
        Elf64_Word nchain;
        Elf64_Word bucket[NUM_HASH_BUCKETS];
        /* worst case length - actual length may be less */
        Elf64_Word chain[NUM_EXTERN_SYMBOLS + 1];
    } hash;
   
    /* plus one for reserved STN_UNDEF entry */
    const int num_dynsyms = count_externs(extern_types) + 1;
    const int hash_word_count = 2 + NUM_HASH_BUCKETS + num_dynsyms;
    
    memset(&hash, 0, sizeof(hash));
    hash.nbucket = NUM_HASH_BUCKETS;
    hash.nchain = num_dynsyms;
    
    int chain_index = 1;
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_types[idx] == EXTERN_TYPE_UNUSED) {
            continue;
        }
        
        unsigned long bucket_index = elf64_hash((const unsigned char *)extern_symbol_names[idx]) % NUM_HASH_BUCKETS;
        
        hash.chain[chain_index] = hash.bucket[bucket_index];        
        hash.bucket[bucket_index] = chain_index;
        ++chain_index;
    }
    
    start_section(state, SECTION_HASH);
    write_bytes(state, &hash, hash_word_count * sizeof(Elf64_Word));
}

#define checked_malloc_type(t, d) checked_malloc(sizeof(t), d)

#define checked_malloc_arraytype(t, n, d) checked_malloc(n * sizeof(t), d)

static void *checked_malloc(size_t size, const char *description) {
    void *object = malloc(size);
    
    if(object == NULL) {
        fprintf(stderr, "Error: memory allocation (%s)\n", description);
        exit(EXIT_FAILURE);
    }
    
    return object;
}

struct strtab {
    size_t size;
    Elf64_Word *indexes;
    char *payload;
};

static struct strtab *create_strtab(size_t size, size_t num_strings) {
    struct strtab *strtab = checked_malloc_type(struct strtab, "string table");
    
    strtab->size = size;
    strtab->indexes = checked_malloc_arraytype(Elf64_Word, num_strings, "string table index array");
    strtab->payload = checked_malloc(size, "string table payload");
    
    return strtab;
}

void free_strtab(struct strtab *strtab) {
    if(strtab != NULL) {
        free(strtab->indexes);
        free(strtab->payload);
    }
    free(strtab);
}

struct strtab_state {
    struct strtab *strtab;
    int position;
};

static void initialize_strtab_state(struct strtab_state *state, struct strtab *strtab) {
    state->strtab = strtab;
    strtab->payload[0] = 0;
    state->position = 1;
}

static void add_to_strtab(struct strtab_state *state, int index, const char *str) {
    struct strtab *strtab = state->strtab;
    
    strtab->indexes[index] = state->position;
    strcpy(&strtab->payload[state->position], str);
    state->position += strlen(str) + 1;
}

#define DYNSTR_LIBCSO6 0
#define DYNSTR_GLIBC225 1
#define DYNSTR_EXTERN(n) (n + 2)

static struct strtab *create_dynstr(const extern_type *extern_types) {
    struct strtab *strtab = create_strtab(sections[SECTION_DYNSTR].sh_size, NUM_EXTERN_SYMBOLS + 2);
    
    struct strtab_state state;
    initialize_strtab_state(&state, strtab);
    
    add_to_strtab(&state, DYNSTR_LIBCSO6, libcso6);
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_types[idx] == EXTERN_TYPE_UNUSED) {
            continue;
        }
        
        add_to_strtab(&state, DYNSTR_EXTERN(idx), extern_symbol_names[idx]);
    }
    
    add_to_strtab(&state, DYNSTR_GLIBC225, glibc_225);
    
    return strtab;
}

void write_dynamic_symbols_section(
    struct write_state *state,
    const extern_type *extern_types,
    const struct strtab *dynstr
) {
    start_section(state, SECTION_DYNSYM);
    
    Elf64_Sym symbol;
    /* STN_UNDEF */
    memset(&symbol, 0, sizeof(symbol));
    write_bytes(state, &symbol, sizeof(symbol));
    
    int got_section = SECTION_BSS;
    Elf64_Addr got_addr = sections[got_section].sh_addr;
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_types[idx] == EXTERN_TYPE_UNUSED) {
            continue;
        }
        
        Elf64_Sym symbol;
        symbol.st_name = dynstr->indexes[DYNSTR_EXTERN(idx)];
        symbol.st_other = 0;
        
        if(extern_types[idx] == EXTERN_TYPE_FUNCTION) {
            symbol.st_info = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);
            symbol.st_shndx = SHN_UNDEF;
            symbol.st_value = 0;
            symbol.st_size = 0;
        } else {
            symbol.st_info = ELF64_ST_INFO(STB_GLOBAL, STT_OBJECT);
            symbol.st_shndx = SECTION_BSS;
            symbol.st_value = got_addr;
            symbol.st_size = sizeof(Elf64_Addr);
            
            got_addr += sizeof(Elf64_Addr);
        }
        
        write_bytes(state, &symbol, sizeof(symbol));
    }
}

void write_string_table_section(struct write_state *state, const struct strtab *strtab, int section_index) {
    start_section(state, section_index);
    write_bytes(state, strtab->payload, strtab->size);
}

void write_symbol_versioning_sections(
    struct write_state *state,
    const extern_type *extern_types,
    const struct strtab *dynstr
) {
    Elf64_Half zero_id = 0;
    Elf64_Half version_id = SYMBOL_VERSION_ID;
    
    start_section(state, SECTION_GNU_VERSYM);
    
    write_bytes(state, &zero_id, sizeof(zero_id));
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_types[idx] == EXTERN_TYPE_UNUSED) {
            continue;
        }
        write_bytes(state, &version_id, sizeof(version_id));
    }
    
    start_section(state, SECTION_GNU_VERNEED);
    
    Elf64_Verneed verneed;
    verneed.vn_version = 1;
	verneed.vn_cnt = 1;
	verneed.vn_file = dynstr->indexes[DYNSTR_LIBCSO6];
	verneed.vn_aux = sizeof(Elf64_Verneed);
	verneed.vn_next = 0;
    
    write_bytes(state, &verneed, sizeof(verneed));
    
    Elf64_Vernaux vernaux;
    vernaux.vna_hash = elf64_hash((const unsigned char *)glibc_225);
	vernaux.vna_flags = 0;
	vernaux.vna_other = SYMBOL_VERSION_ID;
	vernaux.vna_name = dynstr->indexes[DYNSTR_GLIBC225];
	vernaux.vna_next =  0;
    
    write_bytes(state, &vernaux, sizeof(vernaux));
}

void write_relocation_sections(struct write_state *state, const extern_type *extern_types) {
    start_section(state, SECTION_RELA_DYN);
    
    const Elf64_Addr *bss_got = (const Elf64_Addr *)sections[SECTION_BSS].sh_addr;
    int got_index = 0;
    /* first entry is reserved (STN_UNDEF) */
    Elf64_Xword symbol_index = 1;
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_types[idx] != EXTERN_TYPE_DATA) {
            if(extern_types[idx] != EXTERN_TYPE_UNUSED) {
                ++symbol_index;
            }
            continue;
        }
        
        Elf64_Rela relocation;
        relocation.r_offset = (Elf64_Addr)&bss_got[got_index];
        relocation.r_info = ELF64_R_INFO(symbol_index, R_X86_64_COPY);
        relocation.r_addend = 0;
        
        write_bytes(state, &relocation, sizeof(relocation));
        
        ++got_index;
        ++symbol_index;
    }
    
    start_section(state, SECTION_RELA_PLT);
    
    const Elf64_Addr *plt_got = (const Elf64_Addr *)sections[SECTION_PLTGOT].sh_addr;
    /* the first three entries are reserved for use by the dynamic linker */
    got_index = 3;
    /* first entry is reserved (STN_UNDEF) */
    symbol_index = 1;
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_types[idx] != EXTERN_TYPE_FUNCTION) {
            if(extern_types[idx] != EXTERN_TYPE_UNUSED) {
                ++symbol_index;
            }
            continue;
        }
        
        Elf64_Rela relocation;
        relocation.r_offset = (Elf64_Addr)&plt_got[got_index];
        relocation.r_info = ELF64_R_INFO(symbol_index, R_X86_64_JUMP_SLOT);
        relocation.r_addend = 0;
        
        write_bytes(state, &relocation, sizeof(relocation));
        
        ++got_index;
        ++symbol_index;
    }
}

static struct x86_instr *generate_instructions_for_plt(const extern_type *extern_types) {
    struct x86_builder builder;
    x86_builder_initialize_empty(&builder);
    
    const Elf64_Addr *plt_got = (const Elf64_Addr *)sections[SECTION_PLTGOT].sh_addr;
    
    const int plt0_label = 0;
    x86_builder_append_instr(&builder, x86_instr_new_label(plt0_label));
    
    x86_builder_append_instr(&builder, x86_instr_new_push(
        x86_operand_new_mem64_rel((uintptr_t)&plt_got[1])
    ));
    
    x86_builder_append_instr(&builder, x86_instr_new_jmp(
        x86_operand_new_mem64_rel((uintptr_t)&plt_got[2])
    ));
    
    x86_builder_append_instr(&builder, x86_instr_new_align(16));
    
    /* first three entries are reserved for use by the dynamic linker */
    int function_index = 0;
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_types[idx] != EXTERN_TYPE_FUNCTION) {
            continue;
        }
        
        x86_builder_append_instr(&builder, x86_instr_new_jmp(
            /* +3: the first three entries are reserved for use by the dynamic linker */
            x86_operand_new_mem64_rel((uintptr_t)&plt_got[function_index + 3])
        ));
        
        x86_builder_append_instr(&builder, x86_instr_new_push(
            x86_operand_new_imm32(function_index)
        ));
        
        x86_builder_append_instr(&builder, x86_instr_new_jmp(
            x86_operand_new_label(plt0_label)
        ));
        
        ++function_index;
    }
    
    return x86_builder_get_first(&builder);
}

static void write_process_linkage_table(struct write_state *state, const extern_type *extern_types) {
    x86_encoder_context dummy_context;
    
    int address = sections[SECTION_PLT].sh_addr;
    size_t size = sections[SECTION_PLT].sh_size;
    
    struct x86_instr *instrs = generate_instructions_for_plt(extern_types);
    
    x86_encoder_function *func = x86_encoder_function_create(instrs, address);
    
    if(x86_encoder_compute_function_size(func) != size) {
        fprintf(stderr, "Error: PLT generation (wrong size)\n");
        exit(EXIT_FAILURE);
    }
    
    unsigned char *buffer = checked_malloc(size, "encoding buffer for PLT");
    
    encode_for_x86(buffer, size, func, &dummy_context);
    
    start_section(state, SECTION_PLT);
    write_bytes(state, buffer, size);
    
    free(buffer);
    x86_encoder_function_free(func);
    x86_instr_free_tree(instrs);
}

static void initialize_encoder_context(
    x86_encoder_context *context,
    const struct local_functions *local_functions,
    const local_type *local_types,
    const extern_type *extern_types
) {
    
    /* external symbols */
    
    const char *plt = (const char *)sections[SECTION_PLT].sh_addr;
    const Elf64_Addr *bss_got = (const Elf64_Addr *)sections[SECTION_BSS].sh_addr;
    
    /* first entry (16 bytes) is reserved for use by dynamic linker */
    int plt_offset = 16;
    int bss_got_index = 0;
    
    for(int idx = 0; idx < NUM_EXTERN_SYMBOLS; ++idx) {
        if(extern_types[idx] == EXTERN_TYPE_FUNCTION) {
            context->externs[idx] = (intptr_t)&plt[plt_offset];
            plt_offset += 16;
        }
        
        if(extern_types[idx] == EXTERN_TYPE_DATA) {
            context->externs[idx] = (intptr_t)&bss_got[bss_got_index];
            ++bss_got_index;
        }
    }
    
    /* local symbols - functions */
    
    for(int idx = 0; idx < local_functions->n; ++idx) {
        const struct local_function *func = &local_functions->funcs[idx];
        context->locals[func->symbol] = x86_encoder_function_get_address(func->encoder_func);
    }
    
    /* local symbols - data */
    
    const char *rodata = (const char *)sections[SECTION_RODATA].sh_addr;
    int rodata_index = 0;
    
    if(local_types[LOCAL_MSG_EOI]) {
        context->locals[LOCAL_MSG_EOI] = (intptr_t)&rodata[rodata_index];
        rodata_index += sizeof(msg_eoi);
    }
    
    if(local_types[LOCAL_MSG_FERR]) {
        context->locals[LOCAL_MSG_FERR] = (intptr_t)&rodata[rodata_index];
        rodata_index += sizeof(msg_ferr);
    }
    
    if(local_types[LOCAL_MSG_LEFT]) {
        context->locals[LOCAL_MSG_LEFT] = (intptr_t)&rodata[rodata_index];
        rodata_index += sizeof(msg_left);
    }
    
    if(local_types[LOCAL_MSG_RIGHT]) {
        context->locals[LOCAL_MSG_RIGHT] = (intptr_t)&rodata[rodata_index];
        rodata_index += sizeof(msg_right);
    }
    
    context->locals[LOCAL_M] = sections[SECTION_DATA].sh_addr;
}

static void write_text_section(
    struct write_state *state,
    const struct local_functions *local_functions,
    const local_type *local_types,
    const extern_type *extern_types
) {
    x86_encoder_context context;
    initialize_encoder_context(&context, local_functions, local_types, extern_types);
    
    start_section(state, SECTION_TEXT);
    
    for(int idx = 0; idx < local_functions->n; ++idx) {
        size_t size = local_functions->funcs[idx].size;
        
        unsigned char *buffer = checked_malloc(size, "encoding buffer for local function");
        
        encode_for_x86(buffer, size, local_functions->funcs[idx].encoder_func, &context);
        write_bytes(state, buffer, size);
        
        free(buffer);
    }
}

static void write_rodata_section(struct write_state *state, const local_type *local_types) {
    start_section(state, SECTION_RODATA);
    
    if(local_types[LOCAL_MSG_EOI]) {
        write_bytes(state, msg_eoi, sizeof(msg_eoi));
    }
    
    if(local_types[LOCAL_MSG_FERR]) {
        write_bytes(state, msg_ferr, sizeof(msg_ferr));
    }
    
    if(local_types[LOCAL_MSG_LEFT]) {
        write_bytes(state, msg_left, sizeof(msg_left));
    }
    
    if(local_types[LOCAL_MSG_RIGHT]) {
        write_bytes(state, msg_right, sizeof(msg_right));
    }
}

static void write_dynamic_section(struct write_state *state, const struct strtab *dynstr) {
    dynamic[DYNAMIC_NEEDED].d_un.d_val = dynstr->indexes[DYNSTR_LIBCSO6];
    dynamic[DYNAMIC_HASH].d_un.d_ptr = sections[SECTION_HASH].sh_addr;
    dynamic[DYNAMIC_STRTAB].d_un.d_ptr = sections[SECTION_DYNSTR].sh_addr;
    dynamic[DYNAMIC_SYMTAB].d_un.d_ptr = sections[SECTION_DYNSYM].sh_addr;
    dynamic[DYNAMIC_STRSZ].d_un.d_val = sections[SECTION_DYNSTR].sh_size;
    dynamic[DYNAMIC_SYMENT].d_un.d_val = sizeof(Elf64_Sym);
    dynamic[DYNAMIC_DEBUG].d_un.d_ptr = 0;
    dynamic[DYNAMIC_PLTGOT].d_un.d_ptr = sections[SECTION_PLTGOT].sh_addr;
    dynamic[DYNAMIC_PLTRELSZ].d_un.d_val = sections[SECTION_RELA_PLT].sh_size;
    dynamic[DYNAMIC_PLTREL].d_un.d_val = DT_RELA;
    dynamic[DYNAMIC_JMPREL].d_un.d_ptr = sections[SECTION_RELA_PLT].sh_addr;
    dynamic[DYNAMIC_RELA].d_un.d_ptr = sections[SECTION_RELA_DYN].sh_addr;
    dynamic[DYNAMIC_RELASZ].d_un.d_val = sections[SECTION_RELA_DYN].sh_size;
    dynamic[DYNAMIC_RELAENT].d_un.d_val = sizeof(Elf64_Rela);
    dynamic[DYNAMIC_VERNEED].d_un.d_ptr = sections[SECTION_GNU_VERNEED].sh_addr;
    dynamic[DYNAMIC_VERNEEDNUM].d_un.d_val = 1;
    dynamic[DYNAMIC_VERSYM].d_un.d_ptr = sections[SECTION_GNU_VERSYM].sh_addr;
    
    start_section(state, SECTION_DYNAMIC);
    write_bytes(state, dynamic, sizeof(dynamic));
}

static void write_got_section(struct write_state *state, const extern_type *extern_types) {
    Elf64_Addr got[NUM_EXTERN_SYMBOLS + 3];
    
    /* reserved entries */
    got[0] = sections[SECTION_DYNAMIC].sh_addr;
    got[1] = 0;
    got[2] = 0;
    
    /* The remaining entries are jumps back into the PLT to support lazy loading. */
    const size_t plt_entry_size = 16;
    const size_t plt_jump_size = 6; 
    
    Elf64_Addr addr_in_plt = sections[SECTION_PLT].sh_addr + plt_entry_size + plt_jump_size;
    
    const int num_got_entries = count_externs_with_type(extern_types, EXTERN_TYPE_FUNCTION);
    
    int got_index;
    
    for(got_index = 3; got_index < 3 + num_got_entries; ++got_index) {
        got[got_index] = addr_in_plt;
        addr_in_plt += plt_entry_size;
    }
    
    start_section(state, SECTION_PLTGOT);
    write_bytes(state, got, got_index * sizeof(Elf64_Addr));
}

static void write_data_section(struct write_state *state, const extern_type *extern_types) {
    const Elf64_Addr *bss = (const Elf64_Addr *)sections[SECTION_BSS].sh_addr;
    
    /* The .bss section starts with the GOT for data externs, followed by the
     * memory array used by the program. The .data section contains a single
     * pointer which points to memory array. */
    int num_data_externs = count_externs_with_type(extern_types, EXTERN_TYPE_DATA);
    const Elf64_Addr m = (Elf64_Addr)&bss[num_data_externs];
    
    start_section(state, SECTION_DATA);
    write_bytes(state, &m, sizeof(m));
}

static struct strtab *create_section_header_strings_table(void) {
    struct strtab *strtab = create_strtab(sections[SECTION_SHSTRTAB].sh_size, NUM_SECTIONS);
    
    struct strtab_state state;
    initialize_strtab_state(&state, strtab);
    
    for(int idx = 0; idx < NUM_SECTIONS; ++idx) {
        if(idx == SHN_UNDEF || idx == SECTION_PLT) {
            /* special cases - handled below */
            continue;
        }
        
        add_to_strtab(&state, idx, section_names[idx]);
    }
    
    strtab->indexes[SHN_UNDEF] = 0;
    
    /* for the .plt section, re-use the end of .got.plt */
    strtab->indexes[SECTION_PLT] = strtab->indexes[SECTION_PLTGOT] + 4;
    
    return strtab;
}

static void write_section_headers(struct write_state *state, const struct strtab *shstrtab) {
    for(int idx = 0; idx < NUM_SECTIONS; ++idx) {
        sections[idx].sh_name = shstrtab->indexes[idx];
    }
    
    align_state(state, SHTAB_ALIGNMENT);
    
    Elf64_Off expected_offset = compute_section_headers_offset();
    
    if(state->offset != expected_offset) {
        fprintf(
            stderr,
            "Error: incorrect offset at start of section headers table (expected: %" PRIu64 " actual: %" PRIu64 ")\n",
            expected_offset,
            state->offset
        );
        exit(EXIT_FAILURE);
    }
    
    write_bytes(state, sections, sizeof(sections));
}

static void cleanup_local_functions(struct local_functions *local_functions) {
    for(int idx = 0; idx < local_functions->n; ++idx) {
        x86_encoder_function_free(local_functions->funcs[idx].encoder_func);
        x86_instr_free_tree(local_functions->funcs[idx].instrs);
    }
}

void elf64_generate(FILE *f, const struct node *root) {
    struct local_functions local_functions;
    generate_local_functions(&local_functions, root);
    
    local_type local_types[NUM_LOCAL_SYMBOLS];
    extern_type extern_types[NUM_EXTERN_SYMBOLS];
    enumerate_references(local_types, extern_types, &local_functions);
    
    compute_addresses_up_to_text_section(extern_types);
    
    compute_local_functions_sizes(&local_functions);
    
    compute_remaining_section_addresses(local_types, extern_types);
    
    struct write_state write_state;
    initialize_write_state(&write_state, f);
    
    write_elf_header(&write_state);
    
    write_program_headers(&write_state);
    
    write_interpreter_section(&write_state);
    
    write_hash_section(&write_state, extern_types);
    
    struct strtab *dynstr = create_dynstr(extern_types);
    
    write_dynamic_symbols_section(&write_state, extern_types, dynstr);
    
    write_string_table_section(&write_state, dynstr, SECTION_DYNSTR);
    
    write_symbol_versioning_sections(&write_state, extern_types, dynstr);
    
    write_relocation_sections(&write_state, extern_types);
    
    write_process_linkage_table(&write_state, extern_types);
    
    write_text_section(&write_state, &local_functions, local_types, extern_types);
    
    write_rodata_section(&write_state, local_types);
    
    write_dynamic_section(&write_state, dynstr);
    
    write_got_section(&write_state, extern_types);
    
    write_data_section(&write_state, extern_types);
    
    struct strtab *shstrtab = create_section_header_strings_table();
    
    write_string_table_section(&write_state, shstrtab, SECTION_SHSTRTAB);
    
    write_section_headers(&write_state, shstrtab);
    
    free_strtab(dynstr);
    
    free_strtab(shstrtab);
    
    cleanup_local_functions(&local_functions);
}
