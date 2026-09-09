# Skizm

A small, readable programming language for building indie games — a lightweight,
Unity-like workflow without the editor/runtime bloat. Write game logic in `.yl`
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
./build.sh compile examples/game.yl   # compile a .yl file to C
```

## Layout

- `compiler/` — lexer, parser, semantic checker, and C code generator (C11)
- `runtime/` — object model, message dispatch, arena memory management
- `examples/` — sample games: roguelike (`game.yl`), autobattler, kingdom sim
- `tests/` — regression tests with expected outputs (parser, errors, diagnostics)
- `ROADMAP.md` — where the language is headed

## Status

Compiler pipeline, runtime, and diagnostics are in place with a green test
suite. Next up per the roadmap: settling object field semantics, a small game
standard library (vectors, timers, input, scenes, save data), an
init/update/draw game loop, terminal-first rendering, and only then a minimal
editor. See `ROADMAP.md` for details.

## License

MIT — see `LICENSE`.
