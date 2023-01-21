# Copyright (C) 2022 Philippe Aubertin.
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

include header.mk

.PHONY: 
all:
	make -C src

.PHONY: clean
clean:
	make -C src clean
	-rm -f \
		examples/echo \
		examples/hello \
		examples/left \
		examples/right

.PHONY: slow-hello
slow-hello: all
	src/bf -slow examples/hello.bf

.PHONY: tree-hello
tree-hello: all
	src/bf -tree examples/hello.bf

.PHONY: echo
echo: examples/echo

.PHONY: hello
hello: examples/hello

.PHONY: left
left: examples/left

.PHONY: right
right: examples/right

.PHONY: run-echo-eof
run-echo-eof: examples/echo
	echo -ne '' | examples/echo

.PHONY: run-echo-hello
run-echo-hello: examples/echo
	echo -ne 'Hello World!\n\0' | examples/echo

.PHONY: run-hello
run-hello: examples/hello
	examples/hello

.PHONY: run-left
run-left: examples/left
	examples/left

.PHONY: run-right
run-right: examples/right
	examples/right

examples/echo: examples/echo.bf all
examples/hello: examples/hello.bf all
examples/left: examples/left.bf all
examples/right: examples/right.bf all

%: %.bf
	src/bfc -backend elf64 -o $@ $<

%.c: %.bf
	src/bfc -backend c -o $@ $<
