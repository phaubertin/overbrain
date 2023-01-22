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

#ifndef BFC_BACKEND_SYMBOLS_H
#define BFC_BACKEND_SYMBOLS_H

typedef enum {
    EXTERN_EXIT,
    EXTERN_FERROR,
    EXTERN_FGETC,
    EXTERN_FPRINTF,
    EXTERN_LIBC_START_MAIN,
    EXTERN_PERROR,
    EXTERN_PUTC,
    EXTERN_STDERR,
    EXTERN_STDIN,
    EXTERN_STDOUT
} extern_symbol;

#define NUM_EXTERN_SYMBOLS 10

extern const char *extern_symbol_names[NUM_EXTERN_SYMBOLS];

typedef enum {
    LOCAL_CHECK_INPUT,
    LOCAL_FAIL_TOO_FAR_LEFT,
    LOCAL_FAIL_TOO_FAR_RIGHT,
    LOCAL_M,
    LOCAL_MAIN,
    LOCAL_MSG_EOI,
    LOCAL_MSG_FERR,
    LOCAL_MSG_LEFT,
    LOCAL_MSG_RIGHT,
    LOCAL_START,
} local_symbol;

#define NUM_LOCAL_SYMBOLS 10

extern const char *local_symbol_names[NUM_LOCAL_SYMBOLS];

#endif
