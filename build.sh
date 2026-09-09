#!/bin/bash
#
# build.sh - One-click build for Sprite (macOS compatible)
#
# Usage:
#   ./build.sh           Build everything
#   ./build.sh test      Build and run all regression tests
#   ./build.sh parser-test
#   ./build.sh string-test
#   ./build.sh error-test
#   ./build.sh runtime-diagnostic-test
#   ./build.sh clean     Remove build artifacts
#   ./build.sh compile   Compile a .yl file
#

set -e  # Exit on error

# Colors (may not work on all terminals)
YELLOW='\033[1;33m'
GREEN='\033[0;32m'
NC='\033[0m'

echo "=== Sprite Build System ==="
echo

# Compiler settings for macOS
CC="clang"
CFLAGS="-Wall -Wextra -std=c11 -O2"

# Directories
RUNTIME_DIR="runtime"
COMPILER_DIR="compiler"
BUILD_DIR="build"

mkdir -p "$BUILD_DIR"

case "${1:-build}" in
    clean)
        echo "Cleaning..."
        rm -rf "$BUILD_DIR"
        echo "Done."
        exit 0
        ;;
esac

# ============ Build Runtime ============
echo "Compiling runtime..."

$CC $CFLAGS -c "$RUNTIME_DIR/arena.c" -o "$BUILD_DIR/arena.o"
echo "  ✓ arena.c"

$CC $CFLAGS -c "$RUNTIME_DIR/object.c" -I"$RUNTIME_DIR" -o "$BUILD_DIR/object.o"
echo "  ✓ object.c"

$CC $CFLAGS -c "$RUNTIME_DIR/dispatch.c" -I"$RUNTIME_DIR" -o "$BUILD_DIR/dispatch.o"
echo "  ✓ dispatch.c"

$CC $CFLAGS -c "$RUNTIME_DIR/runtime.c" -I"$RUNTIME_DIR" -o "$BUILD_DIR/runtime.o"
echo "  ✓ runtime.c"

# Create static library (macOS uses libtool or ar)
ar rcs "$BUILD_DIR/libruntime.a" \
    "$BUILD_DIR/arena.o" \
    "$BUILD_DIR/object.o" \
    "$BUILD_DIR/dispatch.o" \
    "$BUILD_DIR/runtime.o"
echo "  ✓ libruntime.a"

# ============ Build Compiler ============
echo "Compiling compiler..."

$CC $CFLAGS -c "$COMPILER_DIR/token.c" -I"$COMPILER_DIR" -o "$BUILD_DIR/token.o"
echo "  ✓ token.c"

$CC $CFLAGS -c "$COMPILER_DIR/lexer.c" -I"$COMPILER_DIR" -o "$BUILD_DIR/lexer.o"
echo "  ✓ lexer.c"

$CC $CFLAGS -c "$COMPILER_DIR/ast.c" -I"$COMPILER_DIR" -o "$BUILD_DIR/ast.o"
echo "  ✓ ast.c"

$CC $CFLAGS -c "$COMPILER_DIR/parser.c" -I"$COMPILER_DIR" -o "$BUILD_DIR/parser.o"
echo "  ✓ parser.c"

$CC $CFLAGS -c "$COMPILER_DIR/codegen.c" -I"$COMPILER_DIR" -o "$BUILD_DIR/codegen.o"
echo "  ✓ codegen.c"

$CC $CFLAGS -c "$COMPILER_DIR/semantic.c" -I"$COMPILER_DIR" -o "$BUILD_DIR/semantic.o"
echo "  ✓ semantic.c"

$CC $CFLAGS -c "$COMPILER_DIR/main.c" -I"$COMPILER_DIR" -o "$BUILD_DIR/main.o"
echo "  ✓ main.c"

$CC $CFLAGS \
    "$BUILD_DIR/token.o" \
    "$BUILD_DIR/lexer.o" \
    "$BUILD_DIR/ast.o" \
    "$BUILD_DIR/parser.o" \
    "$BUILD_DIR/codegen.o" \
    "$BUILD_DIR/semantic.o" \
    "$BUILD_DIR/main.o" \
    -o "$BUILD_DIR/spritec"
echo "  ✓ spritec (Sprite Compiler)"

# ============ Build Test ============
# Link directly with object files instead of static library (more reliable on macOS)
echo "Compiling test..."
$CC $CFLAGS -I"$RUNTIME_DIR" \
    test_runtime.c \
    "$BUILD_DIR/arena.o" \
    "$BUILD_DIR/object.o" \
    "$BUILD_DIR/dispatch.o" \
    "$BUILD_DIR/runtime.o" \
    -o "$BUILD_DIR/test_runtime"
echo "  ✓ test_runtime"

echo
echo "Build complete!"
echo
echo "  Compiler: $BUILD_DIR/spritec"
echo "  Runtime:  $BUILD_DIR/libruntime.a"
echo

compile_yl() {
    local source="$1"
    local output="$2"
    "$BUILD_DIR/spritec" "$source" -o "$BUILD_DIR/$output.c"
    $CC $CFLAGS -I"$RUNTIME_DIR" "$BUILD_DIR/$output.c" \
        "$BUILD_DIR/arena.o" \
        "$BUILD_DIR/object.o" \
        "$BUILD_DIR/dispatch.o" \
        "$BUILD_DIR/runtime.o" \
        -o "$BUILD_DIR/$output"
}

run_parser_test() {
    echo "Running parser precedence test..."
    "$BUILD_DIR/spritec" --ast tests/parser_precedence.yl > "$BUILD_DIR/parser_precedence.ast"
    diff -u tests/parser_precedence.expected "$BUILD_DIR/parser_precedence.ast"
    echo "Parser precedence test passed."
}

run_string_test() {
    echo "Running string escaping test..."
    compile_yl tests/string_escaping.yl string_escaping
    "$BUILD_DIR/string_escaping" > "$BUILD_DIR/string_escaping.out"
    diff -u tests/string_escaping.expected "$BUILD_DIR/string_escaping.out"
    echo "String escaping test passed."
}

run_error_test() {
    local source="$1"
    local expected="$2"
    local name
    name=$(basename "$source" .yl)

    if "$BUILD_DIR/spritec" "$source" -o "$BUILD_DIR/$name.c" > "$BUILD_DIR/$name.stdout" 2> "$BUILD_DIR/$name.stderr"; then
        echo "Expected $source to fail, but it compiled."
        exit 1
    fi

    diff -u "$expected" "$BUILD_DIR/$name.stderr"
}

run_error_tests() {
    echo "Running compiler error diagnostic tests..."
    run_error_test tests/errors/bad_field.yl tests/errors/bad_field.expected
    run_error_test tests/errors/bad_initializer.yl tests/errors/bad_initializer.expected
    run_error_test tests/errors/bad_keyword_arg.yl tests/errors/bad_keyword_arg.expected
    run_error_test tests/errors/missing_method_end.yl tests/errors/missing_method_end.expected
    run_error_test tests/errors/unknown_variable.yl tests/errors/unknown_variable.expected
    run_error_test tests/errors/unknown_self_field.yl tests/errors/unknown_self_field.expected
    echo "Compiler error diagnostic tests passed."
}

run_runtime_diagnostic_test() {
    local source="$1"
    local expected="$2"
    local name
    name=$(basename "$source" .yl)

    compile_yl "$source" "$name"
    "$BUILD_DIR/$name" > "$BUILD_DIR/$name.out" 2>&1
    diff -u "$expected" "$BUILD_DIR/$name.out"
}

run_runtime_diagnostic_tests() {
    echo "Running runtime diagnostic tests..."
    run_runtime_diagnostic_test tests/runtime_errors/bad_dynamic_field.yl tests/runtime_errors/bad_dynamic_field.expected
    run_runtime_diagnostic_test tests/runtime_errors/bad_method_arity.yl tests/runtime_errors/bad_method_arity.expected
    run_runtime_diagnostic_test tests/runtime_errors/bad_system_arity.yl tests/runtime_errors/bad_system_arity.expected
    echo "Runtime diagnostic tests passed."
}

# ============ Handle Commands ============
case "${1:-build}" in
    test)
        echo "Running runtime test..."
        echo
        "$BUILD_DIR/test_runtime"
        echo
        run_parser_test
        echo
        run_string_test
        echo
        run_error_tests
        echo
        run_runtime_diagnostic_tests
        ;;

    parser-test)
        run_parser_test
        ;;

    string-test)
        run_string_test
        ;;

    error-test)
        run_error_tests
        ;;

    runtime-diagnostic-test)
        run_runtime_diagnostic_tests
        ;;
        
    compile)
        if [ -z "$2" ]; then
            echo "Usage: ./build.sh compile <source.yl>"
            exit 1
        fi
        SOURCE="$2"
        BASENAME=$(basename "$SOURCE" .yl)
        
        echo "Compiling $SOURCE..."
        
        compile_yl "$SOURCE" "$BASENAME"
        
        echo "Created: $BUILD_DIR/$BASENAME"
        echo
        echo "Run with: ./$BUILD_DIR/$BASENAME"
        ;;
esac
