# Ghostty Status

A small macOS terminal status display written in C23. Shows total, system, and user CPU usage in two rows, updating roughly once per second.

Meant to be used in Ghostty in its own window. 

## Build and run

Requires macOS and a C23-capable compiler. Run these commands from the project root in a terminal.


```zsh
clang -std=c23 src/main.c -o gs
./gs
```

Press **Ctrl+C** to exit.

## Source layout

The project uses a header-free unity build: `main.c` includes the other C files, so compile only `src/main.c`. 
Conditional `UNITY_BUILD` includes provide dependencies for static IDE analysis, e.g clion.

| File                                       | Purpose                                                      |
|--------------------------------------------|--------------------------------------------------------------|
| [main.c](src/main.c)                       | Main loop, state updates, and terminal setup                 |
| [cpu.c](src/cpu.c)                         | CPU sampling, usage calculations, and timeline formatting    |
| [state.c](src/state.c)                     | Shared state, including previous/current samples and history |
| [ds.c](src/ds.c)                           | Data structures and utility functions                        |
| [bar.c](src/bar.c)                         | Status bar layout and terminal drawing                       |
| [global_typedefs.c](src/global_typedefs.c) | Shared types and numeric aliases                             |
