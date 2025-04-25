# CMake Package Dump

Dump specifications in CMake package.

## Requirements

- CMake 3.16
- Compiler with C++17 support

## Usage

```bash
cmakedump <script>      \
    [--cmake <path>]    \
    [--ninja <path>]    \
    [--dir <path>]      \
    [-o <path>]         \
    [-- <args>]         \
    [--verbose]
```

- `<script>`: path to the CMake script that calls `find_package()`
- `--cmake <path>`: path to the CMake executable (default: `cmake`)
- `--ninja <path>`: path to the Ninja executable (default: `ninja`)
- `--dir <path>`: path to the temporary directory (default: `build`)
- `-o <path>`: path to the output file (default: stdout)
- `-- <args>`: additional arguments to pass to CMake Configuration

CMake and Ninja is required.