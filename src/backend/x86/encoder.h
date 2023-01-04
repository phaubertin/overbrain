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

#ifndef BFC_X86_ENCODER_H
#define BFC_X86_ENCODER_H

#include <stdint.h>
#include <stddef.h>
#include "isa.h"

typedef struct x86_encoder_function x86_encoder_function;

typedef struct {
    int locals[NUM_LOCAL_SYMBOLS];
    int externs[NUM_EXTERN_SYMBOLS];
} x86_encoder_context;

x86_encoder_function *x86_encoder_function_create(struct x86_instr *instrs, int address);

void x86_encoder_function_free(x86_encoder_function *func);

int x86_encoder_function_get_address(const x86_encoder_function *func);

void x86_encoder_context_set_extern(x86_encoder_context *ctx, int symbol, int value);

void x86_encoder_context_set_local(x86_encoder_context *ctx, int symbol, int value);

size_t x86_encoder_compute_function_size(const x86_encoder_function *func);

size_t encode_for_x86(
    unsigned char *buf,
    size_t bufsize,
    const x86_encoder_function *func,
    x86_encoder_context *ctx
);

#endif
