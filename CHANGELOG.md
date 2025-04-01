# Changelog

All notable changes to the NMRI calculator will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.2] - 2025-04-01

### Added
- **Command-line expression evaluation:** You can now pass an expression directly as command-line arguments (e.g., `nmri 1 + 2 * 3` or `nmri "5 * (2 + 3)"`) for quick calculations without entering the interactive mode. The program evaluates the expression and exits. If no expression arguments are provided, it enters the standard interactive mode.

### Fixed
- Restored full implementations for core parsing and evaluation functions (`tokenize`, `shunting_yard`, `evaluate_postfix`, `evaluate_expression`, `handle_assignment`, `process_command`, `readCommand`, etc.), fixing numerous `-Wunused-parameter` warnings caused by previous dummy implementations used for brevity.
- Removed unused `opt` variable declaration in `main`, fixing a `-Wunused-variable` warning.

## [0.1.1] - 2025-03-31

### Fixed
- Corrected division (`/`) and modulo (`%`) operations to properly handle division by exactly zero. This prevents potential errors and inaccurate results that could occur due to floating-point representation issues near zero.
- Ensured that the special `ans` variable and the internal memory of the last result are updated only *after* a variable assignment (e.g., `x = 1 + 2`) is fully and successfully completed.

### Changed
- Improved the internal parsing logic for unary plus (`+`) and minus (`-`) operators for better consistency and potentially simpler evaluation flow.

### Code Quality
- Minor internal code formatting adjustments.
- Restored previously omitted function bodies (`clean_near_zero`).


## [0.1.0] - 2025-03-30

### Added
- Initial release of the NMRI calculator with a powerful, intuitive command-line interface
- Comprehensive arithmetic operations including addition, subtraction, multiplication, division, exponentiation, and modulo
- Intuitive percentage calculations with natural syntax (e.g., 100 + 20% = 120, 50 - 10% = 45)
- Full suite of mathematical functions: trigonometric (sin, cos, tan), inverse trigonometric (asin, acos, atan), logarithmic (log), exponential (exp), and numerical utilities (sqrt, abs, floor, ceil, round)
- Dynamic variable system with persistent storage throughout sessions
- Mathematical constants like (π, e, φ) and automatic result memory (ans)
- Versatile memory operations for advanced calculation workflows
- Command history with arrow key navigation for seamless interaction
- Intelligent parsing engine that handles complex nested expressions
- Detailed logging system with session tracking and query history
- Robust error handling with clear, helpful error messages
- Colorful interface with ANSI color codes
- Complete test suite ensuring calculation accuracy and reliability
- Open-source availability under the BSD-2-Clause license

### Changed
- N/A (initial release)

### Fixed
- N/A (initial release)
