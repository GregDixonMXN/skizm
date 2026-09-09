/*
 * ast.h - Abstract Syntax Tree
 * 
 * The parser reads tokens and builds a tree representing your program.
 * The code generator walks this tree to produce C code.
 */

#ifndef AST_H
#define AST_H

#include <stdint.h>

/* Forward declarations */
typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct ClassDef ClassDef;
typedef struct MethodDef MethodDef;
typedef struct Program Program;

/* =========================================================================
 * Expression Nodes
 * ========================================================================= */

typedef enum ExprType {
    EXPR_INT,           /* 42 */
    EXPR_FLOAT,         /* 3.14 */
    EXPR_STRING,        /* "hello" */
    EXPR_BOOL,          /* true, false */
    EXPR_NIL,           /* nil */
    EXPR_IDENTIFIER,    /* foo */
    EXPR_SELF,          /* self */
    EXPR_BINARY,        /* a + b */
    EXPR_UNARY,         /* -x, not x */
    EXPR_CALL,          /* obj method: arg */
    EXPR_GET_FIELD,     /* obj.field */
    EXPR_SET_FIELD,     /* obj.field = value */
    EXPR_NEW,           /* ClassName new */
    EXPR_ARRAY,         /* [a, b, c] */
    EXPR_INDEX,         /* arr[i] */
} ExprType;

typedef enum BinaryOp {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV,
    OP_EQ, OP_NE, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_AND, OP_OR
} BinaryOp;

typedef enum UnaryOp {
    OP_NEG, OP_NOT
} UnaryOp;

struct Expr {
    ExprType type;
    int line;  /* For error messages */
    
    union {
        int64_t int_value;
        double float_value;
        int bool_value;
        
        struct {
            char *chars;
            int length;
        } string_value;
        
        struct {
            char *name;
        } identifier;
        
        struct {
            BinaryOp op;
            Expr *left;
            Expr *right;
        } binary;
        
        struct {
            UnaryOp op;
            Expr *operand;
        } unary;
        
        struct {
            Expr *receiver;      /* The object receiving the message */
            char *selector;      /* Method name (e.g., "take_damage:") */
            Expr **args;         /* Array of argument expressions */
            int arg_count;
        } call;
        
        struct {
            Expr *object;
            char *field_name;
        } get_field;
        
        struct {
            Expr *object;
            char *field_name;
            Expr *value;
        } set_field;
        
        struct {
            char *class_name;
        } new_expr;
        
        struct {
            Expr **elements;
            int count;
        } array;
        
        struct {
            Expr *object;
            Expr *index;
        } index;
        
    } as;
};

/* =========================================================================
 * Statement Nodes
 * ========================================================================= */

typedef enum StmtType {
    STMT_EXPR,          /* expression; */
    STMT_VAR,           /* var x = value */
    STMT_ASSIGN,        /* x = value */
    STMT_IF,            /* if cond do ... end */
    STMT_WHILE,         /* while cond do ... end */
    STMT_RETURN,        /* return value */
    STMT_BLOCK,         /* do ... end */
} StmtType;

struct Stmt {
    StmtType type;
    int line;
    
    union {
        struct {
            Expr *expression;
        } expr;
        
        struct {
            char *name;
            Expr *initializer;  /* NULL if no initializer */
        } var;
        
        struct {
            char *name;
            Expr *value;
        } assign;
        
        struct {
            Expr *condition;
            Stmt **then_branch;
            int then_count;
            Stmt **else_branch;
            int else_count;
        } if_stmt;
        
        struct {
            Expr *condition;
            Stmt **body;
            int body_count;
        } while_stmt;
        
        struct {
            Expr *value;  /* NULL for bare return */
        } return_stmt;
        
        struct {
            Stmt **statements;
            int count;
        } block;
        
    } as;
};

/* =========================================================================
 * Top-Level Definitions
 * ========================================================================= */

struct MethodDef {
    char *name;           /* Method name */
    char **params;        /* Parameter names */
    int param_count;
    Stmt **body;          /* Method body statements */
    int body_count;
    int line;
};

struct ClassDef {
    char *name;           /* Class name */
    char **fields;        /* Field names */
    int field_count;
    MethodDef **methods;  /* Method definitions */
    int method_count;
    int line;
};

struct Program {
    ClassDef **classes;
    int class_count;
    MethodDef *main_method;  /* The 'method main do ... end' */
};

/* =========================================================================
 * AST Construction Helpers
 * ========================================================================= */

/* Expressions */
Expr *expr_int(int64_t value, int line);
Expr *expr_float(double value, int line);
Expr *expr_string(const char *chars, int length, int line);
Expr *expr_bool(int value, int line);
Expr *expr_nil(int line);
Expr *expr_identifier(const char *name, int line);
Expr *expr_self(int line);
Expr *expr_binary(BinaryOp op, Expr *left, Expr *right, int line);
Expr *expr_unary(UnaryOp op, Expr *operand, int line);
Expr *expr_call(Expr *receiver, const char *selector, Expr **args, int count, int line);
Expr *expr_get_field(Expr *object, const char *field, int line);
Expr *expr_set_field(Expr *object, const char *field, Expr *value, int line);
Expr *expr_new(const char *class_name, int line);
Expr *expr_array(Expr **elements, int count, int line);
Expr *expr_index(Expr *object, Expr *idx, int line);

/* Statements */
Stmt *stmt_expr(Expr *expression, int line);
Stmt *stmt_var(const char *name, Expr *initializer, int line);
Stmt *stmt_assign(const char *name, Expr *value, int line);
Stmt *stmt_if(Expr *cond, Stmt **then_b, int then_c, Stmt **else_b, int else_c, int line);
Stmt *stmt_while(Expr *cond, Stmt **body, int count, int line);
Stmt *stmt_return(Expr *value, int line);
Stmt *stmt_block(Stmt **stmts, int count, int line);

/* Definitions */
MethodDef *method_def(const char *name, char **params, int param_count,
                      Stmt **body, int body_count, int line);
ClassDef *class_def(const char *name, char **fields, int field_count,
                    MethodDef **methods, int method_count, int line);
Program *program_new(void);
void program_add_class(Program *prog, ClassDef *cls);
void program_set_main(Program *prog, MethodDef *main);

/* Memory */
void ast_free_program(Program *prog);

/* Debug */
void ast_print_program(Program *prog);

#endif /* AST_H */
