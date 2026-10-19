# Contributing

## Prerequisites

- A POSIX C99 compiler and `make`
- `pkg-config`
- `clang-format`
- An installed `oopdbms-engine` 0.2.0 or newer development package

## Local checks

Run these from the repository root before submitting changes:

```sh
make format
make format-check
make test
make
make quick_start
```

`make quick_start` recreates `build/quick-start.oopdb`; do not point it at a
database whose contents should be kept.

The engine is maintained in the
[OOPDBMS repository](https://github.com/KrzysztofMarciniak/oopdbms). Changes
to its execution interfaces should be coordinated with this adapter and tested
against both repositories.

Update the public header and README when changing observable API behavior.
Add API tests for new behavior and failure cases. Generated files under
`build/` are not source files and should not be submitted.
