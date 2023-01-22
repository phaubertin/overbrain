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


#include <stddef.h>
#include "symbols.h"

const char *extern_symbol_names[NUM_EXTERN_SYMBOLS] = {
    [EXTERN_EXIT] = "exit",
    [EXTERN_FERROR] = "ferror",
    [EXTERN_FGETC] = "fgetc",
    [EXTERN_FPRINTF] = "fprintf",
    [EXTERN_LIBC_START_MAIN] = "__libc_start_main",
    [EXTERN_PERROR] = "perror",
    [EXTERN_PUTC] = "putc",
    [EXTERN_STDERR] = "stderr",
    [EXTERN_STDIN] = "stdin",
    [EXTERN_STDOUT] = "stdout"
};

const char *local_symbol_names[NUM_LOCAL_SYMBOLS] = {
    [LOCAL_CHECK_INPUT] = "check_input",
    [LOCAL_FAIL_TOO_FAR_LEFT] = "fail_too_far_left",
    [LOCAL_FAIL_TOO_FAR_RIGHT] = "fail_too_far_right",
    [LOCAL_M] = "m",
    [LOCAL_MAIN] = "main",
    [LOCAL_MSG_EOI] = "msg_eoi",
    [LOCAL_MSG_FERR] = "msg_ferr",
    [LOCAL_MSG_LEFT] = "msg_left",
    [LOCAL_MSG_RIGHT] = "msg_right",
    [LOCAL_START] = "_start"
};
