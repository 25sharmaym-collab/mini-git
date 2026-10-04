# Mini Git

A lightweight educational command-line version control system written in C++.

## What it implements
- `minigit init` — initialize a repository
- `minigit add <file>...` — stage files
- `minigit status` — inspect staged files
- `minigit commit "<message>"` — create a snapshot commit
- `minigit log` — view commit history

The implementation uses the C++17 filesystem API and an internal FNV-1a content hash. It is **not** compatible with Git repositories; it is designed to demonstrate the core ideas behind version control.

## Build
Requires a C++17 compiler and CMake 3.16+.

```bash
cmake -S . -B build
cmake --build build
```

## Run
From a project directory:

```bash
./build/minigit init
./build/minigit add notes.txt
./build/minigit status
./build/minigit commit "first commit"
./build/minigit log
```

On Windows, run `build\Debug\minigit.exe` with the same arguments.

## Concepts demonstrated
- CLI argument parsing
- File I/O
- Directory traversal
- Content hashing
- Staging/index management
- Commit metadata
- Snapshot-based version history
- C++17 filesystem
