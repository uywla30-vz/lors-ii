# Lors Language Specification

Lors is a scientific, logical programming language designed to be transpile to C++.

## Philosophy
- **Logical**: Syntax follows a structured, scientific method approach.
- **Explicit**: No magic; types and scopes are clearly defined.
- **Unique**: Avoids common keywords like `if`, `while`, `int`, `void` in favor of more descriptive, scientific terms.

## Data Types
- `whole`: Represents an integer (maps to `long long` in C++).
- `precise`: Represents a floating-point number (maps to `double` in C++).
- `series`: Represents a string of text (maps to `std::string` in C++).
- `state`: Represents a boolean value (maps to `bool` in C++).

## Keywords

### Variable Declaration
Use `datum` to declare variables.
Syntax: `datum <name> : <type> = <value>;`

```lors
datum count : whole = 10;
datum pi : precise = 3.14159;
datum message : series = "Hello Universe";
datum is_valid : state = true;
```

### Control Flow

#### Conditional (The Hypothesis)
Instead of `if/else`, Lors uses `verify`.

```lors
verify (count > 5) then
    reveal("Count is greater than 5");
otherwise
    reveal("Count is small");
conclude
```

#### Loop (The Cycle)
Instead of `while`, Lors uses `cycle`.

```lors
datum i : whole = 0;
cycle (i < 10) do
    reveal(i);
    i = i + 1;
conclude
```

### Functions (Algorithms)
Functions are defined as `algorithm`.
Return values are sent back using `result`.

```lors
algorithm add_numbers(a : whole, b : whole) -> whole
begin
    datum sum : whole = a + b;
    result sum;
end
```

### Input/Output
- Output: `reveal(<expression>);` (Prints to stdout with newline).

### Comments
- Line comments start with `//`. (Standard, logical).

## Program Structure
A Lors program consists of a series of `datum` (globals) and `algorithm` definitions.
The entry point is an algorithm named `genesis` (instead of main).

```lors
algorithm genesis() -> whole
begin
    reveal("Starting Experiment");
    result 0;
end
```
