# NMRI - A Powerful Command-Line Calculator

> **Powerful mathematics in just a few KB: all the calculation power you need without the bloat.**

**NMRI isn't just another calculator** - it's your mathematical ally for the command line. Combining the elegance of simple syntax with the power of advanced mathematics, NMRI puts professional-grade computational power at your fingertips, right in your terminal.

## ✨ Features

- **Complete Mathematical Arsenal**: From basic arithmetic to complex functions—add, subtract, multiply, divide, power, modulo, and more
- **Percentage Calculations**: Intuitive support for percentage operations (e.g., 100 + 20% = 120)
- **Scientific Function Suite**: Unlock the full potential of mathematics with trigonometric, logarithmic, and exponential functions
- **Smart Variable System**: Create and manipulate variables as easily as in your favorite programming language
- **Rich Built-in Constants**: Access mathematical constants (π, e, φ) and physical constants (c, h, G, Na, k) with simple identifiers
- **Memory Operations**: Store, recall, and manipulate calculation results with intuitive commands
- **Command History**: Never lose track of your calculations with full history navigation using arrow keys
- **Colorful Interface**: Enjoy a visually appealing experience with color-coded outputs for results, errors, and help text
- **Interactive Experience**: Enjoy a responsive, user-friendly interface designed for both quick calculations and complex mathematical explorations
- **Comprehensive Logging**: Keep track of your calculation sessions with detailed logging capabilities
- **Elegant Syntax**: Express complex mathematical ideas with clean, intuitive notation

## Getting Started

### Prerequisites

- GCC or any C compiler
- Make (optional, for using the Makefile)
- Terminal with ANSI color support

### Compilation

Compile with the included Makefile:

```bash
make
```

Or manually:

```bash
gcc -o nmri nmri.c -lm
```

### Installation

Install system-wide (requires administrative privileges):

```bash
sudo make install
```

This will install the `nmri` executable to `/usr/local/bin`, making it available system-wide.

You can also specify a different installation prefix:

```bash
sudo make install PREFIX=/opt
```

To uninstall the calculator:

```bash
sudo make uninstall
```

If you installed with a custom prefix, use the same prefix for uninstalling:

```bash
sudo make uninstall PREFIX=/opt
```

## Testing & Quality Assurance

NMRI comes with a robust test suite that ensures reliability and accuracy:

```bash
make test
```

Our tests verify everything from basic arithmetic to complex expressions and edge cases.

### Starting the Calculator

Run the executable:

```bash
./nmri
```

Or, if you've installed it system-wide:

```bash
nmri
```

### Basic Operations

```
■ 2 + 2
4

■ 5 * (3 + 2)
25

■ 2^8
256
```

### Using Functions

```
■ sin(pi/2)
1

■ sqrt(16)
4

■ log(e)
1
```

### Working with Variables

```
■ x = 5
x = 5

■ y = 10
y = 10

■ x + y
15

■ z = x^2 + y^2
z = 125

■ sqrt(x^2 + y^2)
11.18034

■ store pythag
Stored 11.18034 in pythag

■ pythag
11.18034
```

### Memory Operations

```
■ 42
42

■ m+
Memory: 42

■ 8
8

■ m+
Memory: 50

■ mr
50

■ mc
Memory cleared
```

### Logging

```
■ log on
Logging enabled

■ 2 + 2
4

■ log show
Recent log entries:
-------------------
[2023-05-25 14:32:07] Logging enabled
[2023-05-25 14:32:12] User input: 2 + 2
[2023-05-25 14:32:12] Expression result: 2 + 2 = 4

■ log off
Logging disabled
```

### Special Commands

- `help` - Display help information
- `exit` - Exit the calculator
- `clear` - Clear the screen
- `history` - Show command history
- `variables` - List all defined variables
- `memory` - Show the current memory value
- `m+` - Add last result to memory
- `m-` - Subtract last result from memory
- `mr` - Recall memory value
- `mc` - Clear memory
- `store x` - Store last result in variable x
- `log on` - Enable logging
- `log off` - Disable logging
- `log show` - Show recent log entries
- `log file` - Show current log file path

## Examples

### Percentage Calculations

```
■ 100 + 20%
120

■ 50 - 10%
45

■ 100 * 20%
20

■ 100 / 20%
500

■ x = 100
x = 100

■ x + 30%
130

■ (100 + 20%) * 50%
60
```

### Complex Calculations

```
■ (2 + 3) * (4 + 5)
45

■ sin(pi/4)^2 + cos(pi/4)^2
1

■ x = 2
x = 2

■ y = 3
y = 3

■ z = sqrt(x^2 + y^2)
z = 3.60555
```

### Using Previous Results and Storage

```
■ 5 * 5
25

■ ans + 15
40

■ store result
Stored 40 in result

■ result / 2
20
```

### Built-in Constants

```
■ pi
3.14159

■ e
2.71828

■ phi
1.61803

■ gamma
0.577216

■ c
299792458

■ h
6.62607e-34

■ Na
6.02214e+23

■ h * c
1.986e-25

■ G * 5.97e24 * 70 / (6371e3^2)
9.81

■ k * Na
8.314
```

## Changelog

See the [CHANGELOG.md](CHANGELOG.md) file for details on version history and updates.

## License

This project is licensed under the BSD-2-Clause License.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

---

**Repository**: https://github.com/davidesantangelo/nmri
