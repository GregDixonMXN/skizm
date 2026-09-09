# Skizm Roadmap

Skizm is for building small indie games first. Treat every language and runtime choice as serving that goal.

## Locked decisions

- One rule for state: field access is always a message send, with
  auto-generated accessors per `var`. No typed fields, no second rule.
- Construction is `new` + zero-arg `init`. No keyword allocation.
- No blocks or closures: `if`/`while` are statements; higher-order
  collection messages wait until blocks exist, if they ever do.
- Standard library as library objects (`lib/`), never new syntax.
- macOS and Linux build clean from the same script; CI runs both.

## Current Direction

- Keep the language small, readable, and game-oriented.
- Prefer fast iteration and simple deployment over broad language completeness.
- Build toward a lightweight Unity-like workflow without inheriting Unity's editor/runtime bloat.
- Correctness comes before new engine features. Broken object semantics will poison every gameplay system built on top.

## Foundation Fixes

- Field access must be correct across objects. `self.health` can compile to a direct field index, but `hero.health` needs runtime lookup or compiler type knowledge.
- Keyword messages must support multi-argument selectors like `System random: 1 max: 100`.
- Message sends now bind looser than arithmetic, so `"Gold: " + self.gold print` means `("Gold: " + self.gold) print`.
- String literals now support escaped quotes, backslashes, newlines, carriage returns, tabs, and null bytes through lexer, parser, and C codegen.
- Parser errors now include line/column, source excerpts, caret markers, and specific messages for missing initializers, keyword arguments, field names, and method/class `end`s.
- Semantic validation now catches unknown variables, unknown classes, and unknown `self` fields before C code generation.
- Runtime diagnostics now list available fields for dynamic field mistakes and suggest similarly named selectors with expected argument counts.

## Engine Path

1. Stabilize language semantics with tests for fields, methods, arrays, strings, conditionals, loops, and system calls.
2. Add a small standard library for game needs: vectors, timers, input, scenes, entities, resources, and save data.
3. Introduce a simple runtime game loop: `init`, `update`, `draw`, and input events.
4. Add a renderer behind a tiny API. Start with terminal or 2D software rendering, then move to SDL or another lean native backend.
5. Build a minimal editor only after the runtime model is stable. The first editor should inspect scenes/resources and run the game, not try to clone Unity.

## Near-Term Priorities

- Expand automated example tests so regressions are obvious.
- Add source locations to runtime diagnostics so dynamic object mistakes point back to the `.skizm` line.
- Object field semantics are settled (messages-only); the remaining question
  is what the game loop and input API look like.
- Add useful game primitives before advanced language features.
