# Contributing

## Development Principles

- Keep the core document, layer, undo, and rendering contracts testable outside the UI.
- Prefer narrow, working increments over broad placeholders.
- Do not copy UI artwork, icons, text, terminology, or layout details from competing tools.
- Add dependencies through the dependency register before using them in code.
- Treat destructive image edits as explicit commands with undo coverage.

## Local Checks

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

When Qt 6 is installed:

```sh
cmake --preset desktop -DCMAKE_PREFIX_PATH=/path/to/Qt
cmake --build --preset desktop
```

On macOS, `brew install qtbase` is enough for the current Widgets app target.
