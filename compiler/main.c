/*
 * main.c - Skizm Compiler
 * 
 * Usage: ./skizmc source.skizm -o output.c
 * 
 * Reads your Lua-inspired code, parses it, and generates C.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "codegen.h"

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Could not open file '%s'\n", path);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *buffer = malloc(size + 1);
    if (!buffer) {
        fprintf(stderr, "Not enough memory to read '%s'\n", path);
        fclose(file);
        return NULL;
    }
    
    size_t read = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    
    fclose(file);
    return buffer;
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <source.skizm> [-o output.c]\n", prog);
    fprintf(stderr, "       %s --tokens <source.skizm>   (show tokens)\n", prog);
    fprintf(stderr, "       %s --ast <source.skizm>      (show AST)\n", prog);
}

static void show_tokens(const char *source) {
    Lexer lexer;
    lexer_init(&lexer, source);
    
    Token token;
    do {
        token = lexer_next(&lexer);
        token_print(&token);
    } while (token.type != TOKEN_EOF);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    const char *source_path = NULL;
    const char *output_path = NULL;
    int show_tokens_flag = 0;
    int show_ast_flag = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) {
            show_tokens_flag = 1;
        } else if (strcmp(argv[i], "--ast") == 0) {
            show_ast_flag = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_path = argv[++i];
            } else {
                fprintf(stderr, "Expected output path after -o\n");
                return 1;
            }
        } else if (argv[i][0] != '-') {
            source_path = argv[i];
        }
    }
    
    if (!source_path) {
        fprintf(stderr, "No source file specified\n");
        print_usage(argv[0]);
        return 1;
    }
    
    char *source = read_file(source_path);
    if (!source) return 1;
    
    /* Show tokens mode */
    if (show_tokens_flag) {
        printf("=== Tokens ===\n");
        show_tokens(source);
        free(source);
        return 0;
    }
    
    /* Parse */
    Program *prog = parse(source);
    if (!prog) {
        fprintf(stderr, "Compilation failed.\n");
        free(source);
        return 1;
    }

    if (!semantic_validate(prog)) {
        fprintf(stderr, "Compilation failed.\n");
        ast_free_program(prog);
        free(source);
        return 1;
    }
    
    /* Show AST mode */
    if (show_ast_flag) {
        printf("=== AST ===\n");
        ast_print_program(prog);
        ast_free_program(prog);
        free(source);
        return 0;
    }
    
    /* Generate code */
    FILE *out;
    if (output_path) {
        out = fopen(output_path, "w");
        if (!out) {
            fprintf(stderr, "Could not open output file '%s'\n", output_path);
            ast_free_program(prog);
            free(source);
            return 1;
        }
    } else {
        out = stdout;
    }
    
    codegen_generate(prog, out);
    
    if (output_path) {
        fclose(out);
        printf("Generated: %s\n", output_path);
    }
    
    ast_free_program(prog);
    free(source);
    
    return 0;
}
