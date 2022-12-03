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

#include "../app/options.h" 
#include "bound_checks.h"
#include "compute_offsets.h"
#include "dead_loops.h"
#include "optimizations.h"
#include "run_length.h"

struct node *run_optimizations(struct node *program, const struct options *options) {
    /* memory allocation contract: caller is responsible for freeing the
     * original (if it so chooses). This function is only responsible for
     * freeing any intermediate trees it creates. */
     
    if(options->optimization_level == 0) {
        if(options->no_check) {
            return node_clone_tree(program);
        }
        return insert_bound_checks(program);
    }
    
    struct node *run_length = run_length_optimize(program);
    
    struct node *no_dead_loops = remove_dead_loops(run_length);
    
    node_free(run_length);
    
    struct node *with_offsets = compute_offsets(no_dead_loops);
    
    node_free(no_dead_loops);
    
    if(options->no_check) {
        return with_offsets;
    }
    
    struct node *with_checks = insert_bound_checks(with_offsets);
    
    node_free(with_offsets);
     
    return with_checks;
}
