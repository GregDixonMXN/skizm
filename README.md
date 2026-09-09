# Skizm

A small, readable programming language for building indie games — a lightweight,
Unity-like workflow without the editor/runtime bloat. Write game logic in `.skizm`
files with clean Smalltalk-style syntax; the Skizm compiler translates them to
C, which builds against a tiny object runtime.

```yl
class Player
    var health
    var gold

    method take_damage: amount do
        self.health = self.health - amount
        if self.health <= 0 do
            self die
        end
    end

    method die do
        "You have fallen..." print
    end
end
```

## Quickstart (macOS, clang)

```
./build.sh           # build the compiler and runtime
./build.sh test      # build + run the full regression suite
./build.sh compile examples/game.skizm   # compile a .skizm file to C
```

## Layout

- `compiler/` — lexer, parser, semantic checker, and C code generator (C11)
- `runtime/` — object model, message dispatch, arena memory management
- `examples/` — sample games: roguelike (`game.skizm`), autobattler, kingdom sim
- `tests/` — regression tests with expected outputs (parser, errors, diagnostics)
- `ROADMAP.md` — where the language is headed

## Status

Compiler pipeline, runtime, and diagnostics are in place with a green test
suite (`./build.sh test` runs on macOS and Linux via CI). Next up per the
roadmap: a small game standard library, an init/update/draw game loop,
terminal-first rendering, and only then a minimal editor. See `ROADMAP.md`
for details.

## Language decisions (locked)

- One rule for state: every `obj.field` read is the message `obj field` and
  every `obj.field = v` write is the message `obj field: v`. Each `var`
  auto-generates those accessors (override them with your own methods);
  there is no direct field access and no second rule.
- Construction is `Class new`, which allocates then calls zero-arg `init`
  when defined. Put starting values in `init`, not in main.
- `if`/`while` with `do`/`end` are statements. There are no blocks or
  closures; game logic that needs callbacks polls instead (see `lib/timer.skizm`).
- The standard library is library objects, not syntax: `lib/` holds `Vector`
  and `Timer`, compiled alongside your program
  (`skizmc lib/vector.skizm lib/timer.skizm game.skizm -o game.c`).

## License

MIT — see `LICENSE`.
