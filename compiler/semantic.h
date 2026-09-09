/*
 * semantic.h - Semantic validation before C code generation
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ast.h"

/* Returns 1 if the program is semantically valid, 0 otherwise. */
int semantic_validate(Program *prog);

#endif /* SEMANTIC_H */
