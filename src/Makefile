# Copyright (C) 2023 Philippe Aubertin.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 
# 3. Neither the name of the author nor the names of other contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

include ../header.mk

targets = bf bfc
sources = \
	app/app.c \
	app/options.c \
	backend/backend.c \
	backend/c.c \
	backend/elf64.c \
	backend/nasm.c \
	backend/common/symbols.c \
	backend/x86/builder.c \
	backend/x86/codegen.c \
	backend/x86/encoder.c \
	backend/x86/isa.c \
	frontend/parser.c \
	interpreter/slow.c \
	interpreter/tree.c \
	ir/builder.c \
	ir/node.c \
	ir/query.c \
	optimizations/bound_checks.c \
	optimizations/compute_offsets.c \
	optimizations/dead_loops.c \
	optimizations/optimizations.c \
	optimizations/run_length.c

.PHONY: all
all: $(targets)

.PHONY: clean
clean:
	-rm -f $(targets)

bfc: bfc.c $(sources)
bf: bf.c $(sources)
