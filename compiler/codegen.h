/*
 * codegen.h - C Code Generator
 * 
 * Walks the AST and produces C code that uses the runtime.
 */

#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include <stdio.h>

/* Generate C code for the program and write to file */
void codegen_generate(Program *prog, FILE *out);

#endif /* CODEGEN_H */
