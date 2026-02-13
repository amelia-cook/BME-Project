#!/usr/bin/env python3
import re
import sys
from pathlib import Path

SNAKE_CASE = re.compile(r'^[a-z][a-z0-9_]*$')
UPPER_CASE = re.compile(r'^[A-Z][A-Z0-9_]*$')

function_pattern = re.compile(
    r'^\s*(?:static\s+)?(?:inline\s+)?[a-zA-Z_][a-zA-Z0-9_\s\*]*\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\('
)

variable_pattern = re.compile(
    r'^\s*(?:static\s+)?(?:const\s+)?[a-zA-Z_][a-zA-Z0-9_\s\*]+\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*(=|;)'
)

macro_pattern = re.compile(
    r'^\s*#define\s+([a-zA-Z_][a-zA-Z0-9_]*)'
)

typedef_pattern = re.compile(
    r'^\s*typedef\s+.*\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*;'
)


def print_error(path, lineno, line, message):
    print(f"\n{path}:{lineno}")
    print(f"  {line.rstrip()}")
    print(f"  ^-- {message}")


def check_file(path: Path):
    errors = 0

    try:
        with path.open("r", encoding="utf-8", errors="ignore") as f:
            for lineno, line in enumerate(f, 1):
                stripped = line.strip()

                # Ignore includes
                if stripped.startswith("#include"):
                    continue

                # Ignore struct/enum/union declarations
                if stripped.startswith(("struct ", "enum ", "union ")):
                    continue

                # ---- Macro check ----
                m = macro_pattern.match(line)
                if m:
                    name = m.group(1)
                    if not UPPER_CASE.match(name):
                        print_error(path, lineno, line,
                                    f"Macro '{name}' should be UPPER_CASE")
                        errors += 1
                    continue

                # ---- Function check ----
                m = function_pattern.match(line)
                if m:
                    name = m.group(1)
                    if name in ("if", "for", "while", "switch", "return"):
                        continue

                    if not SNAKE_CASE.match(name):
                        print_error(path, lineno, line,
                                    f"Function '{name}' should be snake_case")
                        errors += 1
                    continue

                # ---- Variable check ----
                m = variable_pattern.match(line)
                if m:
                    name = m.group(1)
                    if not SNAKE_CASE.match(name):
                        print_error(path, lineno, line,
                                    f"Variable '{name}' should be snake_case")
                        errors += 1
                    continue

                # ---- Typedef check ----
                m = typedef_pattern.match(line)
                if m:
                    name = m.group(1)
                    if not name.endswith("_t"):
                        print_error(path, lineno, line,
                                    f"Typedef '{name}' should end with _t")
                        errors += 1
                    continue

    except Exception as e:
        print(f"Error reading {path}: {e}")
        return 1

    return errors


def gather_files(target: Path):
    if target.is_file():
        if target.suffix in (".c", ".h"):
            return [target]
        else:
            return []
    elif target.is_dir():
        return list(target.rglob("*.[ch]"))
    else:
        return []


def main():
    if len(sys.argv) != 2:
        print("Usage: lint.py <file.c|file.h|directory>")
        sys.exit(2)

    target = Path(sys.argv[1])

    if not target.exists():
        print(f"Error: '{target}' does not exist.")
        sys.exit(2)

    files = gather_files(target)

    if not files:
        print("No .c or .h files found.")
        sys.exit(0)

    total_errors = 0

    for file in files:
        total_errors += check_file(file)

    if total_errors:
        print(f"\nFound {total_errors} naming violation(s).")
        sys.exit(1)
    else:
        print("No naming violations found.")
        sys.exit(0)


if __name__ == "__main__":
    main()
