# Contributing

## Commit Style

- Use short, imperative subject lines.
- Keep doc changes separate from code changes.

## Documentation Style

- Headings use Title Case.
- Use fenced code blocks with a language hint, e.g., ```bash```.
- Prefer lists with `-` for bullets and `1.` for numbered steps.

## Code Formatting

- Use `clang-format` with the repository's `.clang-format`.
- Format C/C++ sources in `main/` and project components (excluding vendored libraries) before committing.
- A convenient full-tree format command:

```bash
find main components/cjson \( -name '*.c' -o -name '*.h' \) -print0 | xargs -0 clang-format -i
```

## Testing

- Run `with-idf idf.py fullclean build` before submitting.
- Lint docs with `markdownlint`.
