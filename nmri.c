/**
 * NMRI - Command Line Calculator
 *
 * Author: Davide Santangelo
 *
 * A simple yet powerful command-line calculator supporting variables,
 * common math functions, command history, and basic line editing.
 *
 * Copyright (c) 2025, Davide Santangelo
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define _GNU_SOURCE // Define before including headers to potentially get SOCK_CLOEXEC etc.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <termios.h> // For terminal raw mode (Unix-like systems)
#include <unistd.h>  // For read() and STDIN_FILENO
#include <fcntl.h>   // Needed for fcntl

/* --- Configuration Constants --- */

#define MAX_TOKENS 100                                 // Maximum tokens allowed in an expression
#define MAX_IDENTIFIER_LEN 32                          // Maximum length for variable/function names (increased from 20)
#define MAX_VARIABLES 100                              // Maximum number of user-defined variables
#define HISTORY_SIZE 20                                // Number of commands to keep in history
#define NMRI_MAX_INPUT 256                             // Maximum length of user input line
#define MAX_LOG_LINE 1024                              // Maximum length of a single log line
#define DEFAULT_LOG_FILENAME "nmri.log"                // Default name for the log file
#define CMD_LINE_EXPR_BUFFER_SIZE (NMRI_MAX_INPUT * 2) // Buffer for concatenated cmd line args

/* --- ANSI Color Codes --- */
#define COLOR_RESET "\033[0m"
#define COLOR_BOLD "\033[1m"
#define COLOR_DIM "\033[2m"
#define COLOR_ITALIC "\033[3m"
#define COLOR_UNDERL "\033[4m"
#define COLOR_BLINK "\033[5m"
#define COLOR_REVERSE "\033[7m"
#define COLOR_BLACK "\033[30m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_WHITE "\033[37m"
#define COLOR_BG_BLACK "\033[40m"
#define COLOR_BG_RED "\033[41m"
#define COLOR_BG_GREEN "\033[42m"
#define COLOR_BG_YELLOW "\033[43m"
#define COLOR_BG_BLUE "\033[44m"
#define COLOR_BG_MAGENTA "\033[45m"
#define COLOR_BG_CYAN "\033[46m"
#define COLOR_BG_WHITE "\033[47m"

/* --- Type Definitions --- */

// Type of token identified during parsing
typedef enum
{
    TOKEN_NUMBER,    // A numeric literal (double)
    TOKEN_OPERATOR,  // An arithmetic operator (+, -, *, /, ^, %)
    TOKEN_FUNCTION,  // A mathematical function (sin, cos, log, etc.)
    TOKEN_LPAREN,    // Left parenthesis '('
    TOKEN_RPAREN,    // Right parenthesis ')'
    TOKEN_VARIABLE,  // An identifier representing a variable (legacy, maybe remove if not needed)
    TOKEN_ASSIGNMENT // An identifier followed by '=' (e.g., "x =")
} TokenType;

// Specific type of arithmetic operator
typedef enum
{
    OP_ADD, // +
    OP_SUB, // -
    OP_MUL, // *
    OP_DIV, // /
    OP_POW, // ^ (power)
    OP_MOD  // % (modulo)
} OperatorType;

// Specific type of mathematical function
typedef enum
{
    FUNC_SIN,
    FUNC_COS,
    FUNC_TAN, // Trigonometric
    FUNC_ASIN,
    FUNC_ACOS,
    FUNC_ATAN,        // Inverse trigonometric
    FUNC_LOG,         // Natural logarithm (ln)
    FUNC_SQRT,        // Square root
    FUNC_EXP,         // Exponential (e^x)
    FUNC_ABS,         // Absolute value
    FUNC_FLOOR,       // Round down
    FUNC_CEIL,        // Round up
    FUNC_ROUND,       // Round to nearest integer
    FUNC_INVALID = -1 // Represents an invalid/unknown function
} FunctionType;

// Represents a single token in the input expression
typedef struct
{
    TokenType type;
    int is_percentage; // Flag: 1 if the token represents a percentage value (e.g., "20%")
    union
    {
        double number;                     // Value if TOKEN_NUMBER
        OperatorType op;                   // Value if TOKEN_OPERATOR
        FunctionType func;                 // Value if TOKEN_FUNCTION
        char var_name[MAX_IDENTIFIER_LEN]; // Name if TOKEN_ASSIGNMENT or TOKEN_VARIABLE
    } value;
} Token;

// Represents a value on the evaluation stack (used in RPN evaluation)
// Needed to differentiate between raw numbers and percentages during calculations.
typedef struct
{
    double num;        // The numeric value
    int is_percentage; // Flag: 1 if this value originated from a percentage token
} Value;

// Structure to map function names (strings) to their corresponding enum type
typedef struct
{
    const char *name;
    FunctionType func;
} FunctionMap;

// Structure to store user-defined variables
typedef struct
{
    char name[MAX_IDENTIFIER_LEN];
    double value;
} Variable;

/* --- Global Variables --- */

// Map of recognized function names to their FunctionType
FunctionMap function_map[] = {
    {"sin", FUNC_SIN}, {"cos", FUNC_COS}, {"tan", FUNC_TAN}, {"asin", FUNC_ASIN}, {"acos", FUNC_ACOS}, {"atan", FUNC_ATAN}, {"log", FUNC_LOG}, {"ln", FUNC_LOG}, // Alias 'ln' for natural log
    {"sqrt", FUNC_SQRT},
    {"exp", FUNC_EXP},
    {"abs", FUNC_ABS},
    {"floor", FUNC_FLOOR},
    {"ceil", FUNC_CEIL},
    {"round", FUNC_ROUND},
    {NULL, FUNC_INVALID} // Sentinel value to mark the end of the map
};

// Storage for user-defined variables
Variable variables[MAX_VARIABLES];
int variable_count = 0; // Number of currently defined variables

// Calculator state
double memory = 0.0;      // Value stored in the 'M' memory register
double last_result = 0.0; // Result of the last successful calculation (used for 'ans')

// Command history
char command_history[HISTORY_SIZE][NMRI_MAX_INPUT];
int history_count = 0; // Number of commands currently in history

// Logging state
FILE *log_file = NULL;                                // File pointer for the log file
int logging_enabled = 0;                              // Flag: 1 if logging is active, 0 otherwise
char log_path[NMRI_MAX_INPUT] = DEFAULT_LOG_FILENAME; // Path to the log file

// Terminal state (for raw mode)
struct termios orig_termios; // Stores original terminal settings

/* --- Function Prototypes --- */
int init_logging(void);
void log_session_start(void);
void log_session_stop(void);
void close_logging(void);
void log_message(const char *format, ...);
void show_log(int lines);
OperatorType char_to_op(char c);
int find_variable(const char *name);
int set_variable(const char *name, double value);
void show_help(void);
void add_to_history(const char *cmd);
void show_history(void);
void show_variables(void);
int tokenize(const char *input, Token *tokens, int max_tokens);
int precedence(OperatorType op);
int is_left_associative(OperatorType op);
int shunting_yard(Token *tokens, int token_count, Token *output, int max_output);
double evaluate_postfix(Token *postfix, int count);
double handle_assignment(const char *var_name, const char *expression);
int process_command(const char *input);
double evaluate_expression(const char *input);
double clean_near_zero(double value, double epsilon);
void disableRawMode(void);
void enableRawMode(void);
void readCommand(char *buffer, int max_size);

/* --- Logging Functions --- */

/**
 * @brief Initializes the log file if not already open.
 * @return 1 if logging is ready (file opened successfully), 0 on error.
 */
int init_logging(void)
{
    if (!log_file)
    {
        log_file = fopen(log_path, "a"); // Open in append mode
        if (!log_file)
        {
            fprintf(stderr, "%sError:%s Could not open log file '%s'\n", COLOR_RED, COLOR_RESET, log_path);
            return 0; // Failure
        }
    }
    return 1; // Success
}

/**
 * @brief Logs the start of a calculator session with a timestamp.
 */
void log_session_start(void)
{
    if (!logging_enabled || !init_logging())
        return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t)
    { // Check if localtime returned a valid pointer
        fprintf(log_file, "\n--- SESSION START on %04d-%02d-%02d %02d:%02d:%02d ---\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec);
        fflush(log_file); // Ensure the message is written immediately
    }
}

/**
 * @brief Logs the end of a calculator session with a timestamp.
 */
void log_session_stop(void)
{
    if (!logging_enabled || !log_file)
        return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t)
    {
        fprintf(log_file, "--- SESSION STOP on %04d-%02d-%02d %02d:%02d:%02d ---\n\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec);
        fflush(log_file);
    }
}

/**
 * @brief Closes the log file if it's open. Logs session stop if logging was enabled.
 */
void close_logging(void)
{
    if (log_file)
    {
        if (logging_enabled)
        {
            log_session_stop();
        }
        fclose(log_file);
        log_file = NULL;
    }
}

/**
 * @brief Logs a formatted message to the log file with a timestamp.
 * Similar to printf. Appends a newline if not present.
 * @param format The format string.
 * @param ...    Variable arguments for the format string.
 */
void log_message(const char *format, ...)
{
    if (!logging_enabled || !init_logging())
        return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (!t)
        return; // Safety check for localtime result

    // Print timestamp
    fprintf(log_file, "[%04d-%02d-%02d %02d:%02d:%02d] ",
            t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
            t->tm_hour, t->tm_min, t->tm_sec);

    // Print the formatted message
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    // Ensure the log entry ends with a newline
    size_t format_len = strlen(format);
    if (format_len == 0 || format[format_len - 1] != '\n')
    {
        fprintf(log_file, "\n");
    }
    fflush(log_file); // Ensure immediate write
}

/**
 * @brief Displays the last 'n' lines from the log file to the console.
 * Note: This implementation reads the file twice for simplicity, which might
 * be inefficient for very large log files.
 * @param lines The maximum number of recent lines to show.
 */
void show_log(int lines)
{
    // Ensure log file is available for reading
    if (log_file)
    {
        fclose(log_file);
        log_file = NULL; // Will be reopened in 'a' mode later
    }
    FILE *read_log = fopen(log_path, "r");
    if (!read_log)
    {
        fprintf(stderr, "%sError:%s Could not open log file '%s' for reading.\n", COLOR_RED, COLOR_RESET, log_path);
        init_logging(); // Try to reopen in append mode for future logs
        return;
    }

    int total_lines = 0;
    char buffer[MAX_LOG_LINE];
    // First pass: Count total lines
    while (fgets(buffer, sizeof(buffer), read_log))
    {
        total_lines++;
    }

    // Calculate the starting line number
    int start_line = (total_lines > lines) ? (total_lines - lines) : 0;
    // Second pass: Rewind and skip to the starting line
    rewind(read_log);
    int current_line = 0;
    while (current_line < start_line && fgets(buffer, sizeof(buffer), read_log))
    {
        current_line++;
    }

    // Print the relevant lines with coloring
    printf("%s%s=== Recent Log Entries (Last %d lines) ===%s\n", COLOR_BOLD, COLOR_CYAN, (total_lines - start_line), COLOR_RESET);
    while (fgets(buffer, sizeof(buffer), read_log))
    {
        char *line_ptr = buffer;
        // Basic coloring based on content
        if (strstr(line_ptr, "Error:"))
            printf("%s%s", COLOR_RED, line_ptr);
        else if (strstr(line_ptr, "SESSION START") || strstr(line_ptr, "SESSION STOP"))
            printf("%s%s", COLOR_GREEN, line_ptr);
        else if (strstr(line_ptr, "User input:"))
            printf("%s%s", COLOR_YELLOW, line_ptr);
        else if (strstr(line_ptr, "Result:") || strstr(line_ptr, "assignment:"))
            printf("%s%s", COLOR_GREEN, line_ptr);
        else
            printf("%s%s", COLOR_CYAN, line_ptr); // Default color for other entries

        // Ensure reset at the end of each line if not already present
        if (line_ptr[strlen(line_ptr) - 1] == '\n')
        {
            line_ptr[strlen(line_ptr) - 1] = '\0'; // Temporarily remove newline for check
            if (!strstr(line_ptr, COLOR_RESET))
                printf("%s", COLOR_RESET);
            printf("\n"); // Add newline back
        }
        else
        {
            if (!strstr(line_ptr, COLOR_RESET))
                printf("%s", COLOR_RESET);
        }
    }
    printf("%s%s=== End of Log ===%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    fclose(read_log);
    init_logging(); // Reopen in append mode for subsequent logs
}

/* --- Core Calculation Logic --- */

/**
 * @brief Converts a character representation of an operator to its OperatorType enum.
 * @param c The character operator (+, -, *, /, ^, %).
 * @return The corresponding OperatorType, or -1 if invalid.
 */
OperatorType char_to_op(char c)
{
    switch (c)
    {
    case '+':
        return OP_ADD;
    case '-':
        return OP_SUB;
    case '*':
        return OP_MUL;
    case '/':
        return OP_DIV;
    case '^':
        return OP_POW;
    case '%':
        return OP_MOD;
    default:
        return (OperatorType)-1; // Invalid operator character
    }
}

/**
 * @brief Finds the index of a variable by its name.
 * @param name The name of the variable to find.
 * @return The index in the `variables` array if found, otherwise -1.
 */
int find_variable(const char *name)
{
    for (int i = 0; i < variable_count; i++)
    {
        if (strcmp(variables[i].name, name) == 0)
        {
            return i; // Found
        }
    }
    return -1; // Not found
}

/**
 * @brief Sets or updates the value of a variable.
 * If the variable exists, its value is updated. If not, and there's space,
 * a new variable is created.
 * @param name The name of the variable (max MAX_IDENTIFIER_LEN-1 chars).
 * @param value The double value to assign.
 * @return The index of the variable in the `variables` array, or -1 if failed (e.g., array full).
 */
int set_variable(const char *name, double value)
{
    if (name == NULL || name[0] == '\0')
        return -1; // Invalid name
    int index = find_variable(name);
    if (index >= 0)
    {
        // Variable exists, update its value
        variables[index].value = value;
        return index;
    }
    else
    {
        // Variable doesn't exist, add if space is available
        if (variable_count >= MAX_VARIABLES)
        {
            fprintf(stderr, "%sError:%s Maximum number of variables (%d) reached.\n", COLOR_RED, COLOR_RESET, MAX_VARIABLES);
            return -1; // Variable storage full
        }
        // Copy name safely
        strncpy(variables[variable_count].name, name, MAX_IDENTIFIER_LEN - 1);
        variables[variable_count].name[MAX_IDENTIFIER_LEN - 1] = '\0'; // Ensure null termination
        variables[variable_count].value = value;
        return variable_count++; // Return the index of the newly added variable
    }
}

/* --- User Interface & Command Handling --- */

/**
 * @brief Displays the help message with available commands, constants, operators, and functions.
 */
void show_help(void)
{
    printf("\n%s%sNMRI Calculator Help%s\n\n", COLOR_BOLD, COLOR_YELLOW, COLOR_RESET);
    printf("%s%sCommands:%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    printf("  %sexit%s      Exit the calculator.\n", COLOR_GREEN, COLOR_RESET);
    printf("  %shelp%s      Show this help message.\n", COLOR_GREEN, COLOR_RESET);
    printf("  %sclear%s     Clear the terminal screen.\n", COLOR_GREEN, COLOR_RESET);
    printf("  %shistory%s   Show the command history.\n", COLOR_GREEN, COLOR_RESET);
    printf("  %svars%s      List all defined variables (alias: variables).\n", COLOR_GREEN, COLOR_RESET);
    printf("  %smem%s       Show the current value stored in memory (alias: memory).\n", COLOR_GREEN, COLOR_RESET);
    printf("  %sm+%s        Add the last result ('ans') to memory.\n", COLOR_GREEN, COLOR_RESET);
    printf("  %sm-%s        Subtract the last result ('ans') from memory.\n", COLOR_GREEN, COLOR_RESET);
    printf("  %smr%s        Recall the value from memory (sets 'ans').\n", COLOR_GREEN, COLOR_RESET);
    printf("  %smc%s        Clear the memory (set to 0).\n", COLOR_GREEN, COLOR_RESET);
    printf("  %sstore <n>%s Store the last result ('ans') in variable <n> (e.g., store my_var).\n", COLOR_GREEN, COLOR_RESET);
    printf("  %slog on%s    Enable logging to '%s'.\n", COLOR_GREEN, COLOR_RESET, log_path);
    printf("  %slog off%s   Disable logging.\n", COLOR_GREEN, COLOR_RESET);
    printf("  %slog show%s  Show the last %d lines from the log file.\n", COLOR_GREEN, COLOR_RESET, HISTORY_SIZE);
    printf("  %slog file%s  Show the current log file path.\n\n", COLOR_GREEN, COLOR_RESET);
    printf("%s%sConstants:%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    printf("  %spi%s        Pi (π ≈ 3.14159...)\n", COLOR_MAGENTA, COLOR_RESET);
    printf("  %se%s         Euler's number (e ≈ 2.71828...)\n", COLOR_MAGENTA, COLOR_RESET);
    printf("  %sphi%s       Golden ratio (φ ≈ 1.61803...)\n", COLOR_MAGENTA, COLOR_RESET);
    printf("  %sgamma%s     Euler-Mascheroni constant (γ ≈ 0.57721...)\n", COLOR_MAGENTA, COLOR_RESET);
    printf("  %sc%s         Speed of light (m/s ≈ 2.99792e8)\n", COLOR_MAGENTA, COLOR_RESET);
    printf("  %sh%s         Planck's constant (J⋅s ≈ 6.626e-34)\n", COLOR_MAGENTA, COLOR_RESET);
    printf("  %sG%s         Gravitational constant (m³/kg/s² ≈ 6.674e-11)\n", COLOR_MAGENTA, COLOR_RESET);
    printf("  %sNa%s        Avogadro's number (mol⁻¹ ≈ 6.022e+23)\n", COLOR_MAGENTA, COLOR_RESET);
    printf("  %sk%s         Boltzmann constant (J/K ≈ 1.380e-23)\n", COLOR_MAGENTA, COLOR_RESET);
    printf("  %sinf%s       Infinity.\n", COLOR_MAGENTA, COLOR_RESET);
    printf("  %sans%s       Result of the last successful calculation.\n\n", COLOR_MAGENTA, COLOR_RESET);
    printf("%s%sOperators:%s\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    printf("  %s+, -%s      Addition, Subtraction.\n", COLOR_BLUE, COLOR_RESET);
    printf("             %sSupports percentages: 'A + B%%' means A + (B/100)*A%s\n", COLOR_DIM, COLOR_RESET);
    printf("  %s*, /%s      Multiplication, Division.\n", COLOR_BLUE, COLOR_RESET);
    printf("             %sSupports percentages: 'A * B%%' means A * (B/100)%s\n", COLOR_DIM, COLOR_RESET);
    printf("  %s^%s         Power (right-associative).\n", COLOR_BLUE, COLOR_RESET);
    printf("  %s%%%s         Modulo (remainder).\n", COLOR_BLUE, COLOR_RESET);
    printf("  %s=%s         Assignment (e.g., 'x = 5 * 2'). Must be the first operator.\n\n", COLOR_BLUE, COLOR_RESET);
    printf("%s%sFunctions:%s (Arguments in radians unless specified)\n", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    printf("  %ssin(x), cos(x), tan(x)%s   Trigonometric functions.\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sasin(x), acos(x), atan(x)%s Inverse trigonometric functions.\n", COLOR_CYAN, COLOR_RESET);
    printf("  %slog(x), ln(x)%s            Natural logarithm.\n", COLOR_CYAN, COLOR_RESET);
    printf("  %ssqrt(x)%s                  Square root.\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sexp(x)%s                   Exponential (e^x).\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sabs(x)%s                   Absolute value.\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sfloor(x)%s                 Floor (round down).\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sceil(x)%s                  Ceiling (round up).\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sround(x)%s                 Round to nearest integer.\n", COLOR_CYAN, COLOR_RESET);
    printf("  %sArguments can be percentages: e.g., sin(30%%) == sin(0.3)%s\n\n", COLOR_DIM, COLOR_RESET);
    printf("%s%sExamples:%s\n", COLOR_BOLD, COLOR_YELLOW, COLOR_RESET);
    printf("  %s> 2 + 2%s\n", COLOR_DIM, COLOR_RESET);
    printf("  %s  4%s\n", COLOR_GREEN, COLOR_RESET);
    printf("  %s> 100 + 20%%%s        (100 + 0.20 * 100)\n", COLOR_DIM, COLOR_RESET);
    printf("  %s  120%s\n", COLOR_GREEN, COLOR_RESET);
    printf("  %s> 100 * 50%%%s        (100 * 0.50)\n", COLOR_DIM, COLOR_RESET);
    printf("  %s  50%s\n", COLOR_GREEN, COLOR_RESET);
    printf("  %s> sin(pi / 2)%s\n", COLOR_DIM, COLOR_RESET);
    printf("  %s  1%s\n", COLOR_GREEN, COLOR_RESET);
    printf("  %s> x = 5%s\n", COLOR_DIM, COLOR_RESET);
    printf("  %sx = 5%s\n", COLOR_GREEN, COLOR_RESET);
    printf("  %s> y = x^2 + 2*x + 1%s\n", COLOR_DIM, COLOR_RESET);
    printf("  %sy = 36%s\n", COLOR_GREEN, COLOR_RESET);
    printf("  %s> store result_var%s\n", COLOR_DIM, COLOR_RESET);
    printf("  %sStored 36 in result_var%s\n", COLOR_GREEN, COLOR_RESET);
    printf("  %s> ans * 2%s\n", COLOR_DIM, COLOR_RESET);
    printf("  %s  72%s\n\n", COLOR_GREEN, COLOR_RESET);
}

/**
 * @brief Adds a command to the history buffer.
 * If the history buffer is full, the oldest command is removed.
 * Avoids adding empty commands or the "history" command itself.
 * @param cmd The command string to add.
 */
void add_to_history(const char *cmd)
{
    // Basic validation
    if (cmd == NULL || cmd[0] == '\0')
        return;
    // Avoid adding the history command itself or duplicates of the last command
    if (strcmp(cmd, "history") == 0 || (history_count > 0 && strcmp(cmd, command_history[history_count - 1]) == 0))
    {
        return;
    }
    if (history_count == HISTORY_SIZE)
    {
        // History is full, shift existing commands up
        memmove(command_history[0], command_history[1], (HISTORY_SIZE - 1) * NMRI_MAX_INPUT);
        history_count--; // Make space for the new command
    }
    // Add the new command at the end
    strncpy(command_history[history_count], cmd, NMRI_MAX_INPUT - 1);
    command_history[history_count][NMRI_MAX_INPUT - 1] = '\0'; // Ensure null termination
    history_count++;
}

/**
 * @brief Displays the command history to the console.
 */
void show_history(void)
{
    printf("%s%s=== Command History ===%s\n", COLOR_BOLD, COLOR_YELLOW, COLOR_RESET);
    if (history_count == 0)
    {
        printf("  %s(History is empty)%s\n", COLOR_DIM, COLOR_RESET);
    }
    else
    {
        for (int i = 0; i < history_count; i++)
        {
            printf("  %s%2d:%s %s\n", COLOR_CYAN, i + 1, COLOR_RESET, command_history[i]);
        }
    }
    printf("%s%s=== End of History ===%s\n", COLOR_BOLD, COLOR_YELLOW, COLOR_RESET);
}

/**
 * @brief Displays the currently defined variables and their values.
 */
void show_variables(void)
{
    printf("%s%s=== Variables ===%s\n", COLOR_BOLD, COLOR_MAGENTA, COLOR_RESET);
    if (variable_count == 0)
    {
        printf("  %s(No variables defined)%s\n", COLOR_DIM, COLOR_RESET);
    }
    else
    {
        // Always ensure 'ans' is shown if defined
        int ans_idx = find_variable("ans");
        if (ans_idx != -1)
        {
            printf("  %s%s%s = %s%g%s\n", COLOR_YELLOW, variables[ans_idx].name, COLOR_RESET,
                   COLOR_GREEN, variables[ans_idx].value, COLOR_RESET);
        }
        // Print other variables
        for (int i = 0; i < variable_count; i++)
        {
            if (i == ans_idx)
                continue; // Skip 'ans' if already printed
            printf("  %s%s%s = %s%g%s\n", COLOR_YELLOW, variables[i].name, COLOR_RESET,
                   COLOR_GREEN, variables[i].value, COLOR_RESET);
        }
    }
    printf("%s%s=== End of Variables ===%s\n", COLOR_BOLD, COLOR_MAGENTA, COLOR_RESET);
}

/* --- Parsing and Evaluation --- */

/**
 * @brief Tokenizes the input mathematical expression string.
 * Converts the input string into a sequence of tokens (numbers, operators, functions, etc.).
 * Handles unary minus, constants (pi, e, etc.), and variables.
 * @param input The input expression string.
 * @param tokens Output array where tokens will be stored.
 * @param max_tokens The maximum capacity of the `tokens` array.
 * @return The number of tokens generated, or -1 on error.
 */
int tokenize(const char *input, Token *tokens, int max_tokens)
{
    const char *p = input;
    int token_count = 0;
    int expecting_operand = 1; // Start expecting an operand (number, variable, function, parenthesis)

    while (*p)
    {
        // Skip whitespace
        while (isspace((unsigned char)*p))
            p++;
        if (*p == '\0')
            break; // End of input

        // Check for token limit
        if (token_count >= max_tokens)
        {
            fprintf(stderr, "%sError:%s Expression too complex (too many tokens).\n", COLOR_RED, COLOR_RESET);
            return -1;
        }

        Token *current_token = &tokens[token_count];
        current_token->is_percentage = 0; // Default

        // Handle identifiers (functions, variables, constants, assignment start)
        if (isalpha((unsigned char)*p) || *p == '_')
        { // Allow identifiers to start with _
            const char *start = p;
            while (isalnum((unsigned char)*p) || *p == '_')
                p++; // Read the whole identifier
            size_t len = p - start;
            if (len >= MAX_IDENTIFIER_LEN)
            {
                fprintf(stderr, "%sError:%s Identifier '%.*s...' too long (max %d chars).\n",
                        COLOR_RED, COLOR_RESET, MAX_IDENTIFIER_LEN / 2, start, MAX_IDENTIFIER_LEN - 1);
                return -1;
            }
            char identifier[MAX_IDENTIFIER_LEN];
            strncpy(identifier, start, len);
            identifier[len] = '\0';

            // Check for assignment (identifier followed by '=')
            const char *next_char = p;
            while (isspace((unsigned char)*next_char))
                next_char++;
            if (*next_char == '=')
            {
                // Ensure assignment is at the start or after an operator/paren? (No, can be like `y=x=5`)
                // Current simple implementation allows `y=x=5`, handled in main loop logic.
                // Here we just mark it as an assignment start token.
                current_token->type = TOKEN_ASSIGNMENT;
                strncpy(current_token->value.var_name, identifier, MAX_IDENTIFIER_LEN - 1);
                current_token->value.var_name[MAX_IDENTIFIER_LEN - 1] = '\0';
                token_count++;
                p = next_char + 1; // Consume the '='
                expecting_operand = 1;
                continue;
            }

            // Check for predefined constants
            if (strcmp(identifier, "pi") == 0)
            {
                current_token->type = TOKEN_NUMBER;
                current_token->value.number = M_PI;
            }
            else if (strcmp(identifier, "e") == 0)
            {
                current_token->type = TOKEN_NUMBER;
                current_token->value.number = M_E;
            }
            else if (strcmp(identifier, "phi") == 0)
            {
                current_token->type = TOKEN_NUMBER;
                current_token->value.number = (1.0 + sqrt(5.0)) / 2.0;
            }
            else if (strcmp(identifier, "gamma") == 0)
            {
                current_token->type = TOKEN_NUMBER;
                current_token->value.number = 0.5772156649015329;
            }
            else if (strcmp(identifier, "c") == 0)
            {
                current_token->type = TOKEN_NUMBER;
                current_token->value.number = 299792458.0;
            }
            else if (strcmp(identifier, "h") == 0)
            {
                current_token->type = TOKEN_NUMBER;
                current_token->value.number = 6.62607015e-34;
            }
            else if (strcmp(identifier, "G") == 0)
            {
                current_token->type = TOKEN_NUMBER;
                current_token->value.number = 6.67430e-11;
            }
            else if (strcmp(identifier, "Na") == 0)
            {
                current_token->type = TOKEN_NUMBER;
                current_token->value.number = 6.02214076e23;
            }
            else if (strcmp(identifier, "k") == 0)
            {
                current_token->type = TOKEN_NUMBER;
                current_token->value.number = 1.380649e-23;
            }
            else if (strcmp(identifier, "inf") == 0)
            {
                current_token->type = TOKEN_NUMBER;
                current_token->value.number = INFINITY;
            }
            else if (strcmp(identifier, "ans") == 0)
            {
                current_token->type = TOKEN_NUMBER;
                current_token->value.number = last_result;
            }
            else
            {
                // Check if it's a known function
                FunctionType func = FUNC_INVALID;
                for (int i = 0; function_map[i].name; i++)
                {
                    if (strcmp(identifier, function_map[i].name) == 0)
                    {
                        func = function_map[i].func;
                        break;
                    }
                }
                if (func != FUNC_INVALID)
                {
                    // It's a function
                    current_token->type = TOKEN_FUNCTION;
                    current_token->value.func = func;
                    expecting_operand = 1; // Function must be followed by '('
                }
                else
                {
                    // Assume it's a variable
                    int var_index = find_variable(identifier);
                    if (var_index >= 0)
                    {
                        // Known variable
                        current_token->type = TOKEN_NUMBER; // Treat variable use as injecting its number value
                        current_token->value.number = variables[var_index].value;
                        expecting_operand = 0; // Variable acts as an operand
                    }
                    else
                    {
                        // Unknown identifier
                        fprintf(stderr, "%sError:%s Unknown identifier '%s'.\n", COLOR_RED, COLOR_RESET, identifier);
                        return -1;
                    }
                }
            }
            if (current_token->type == TOKEN_NUMBER)
            {
                expecting_operand = 0;
            } // Constants and variables are operands
            token_count++;
        }
        // Handle numbers (including decimals and percentages)
        else if (isdigit((unsigned char)*p) || (*p == '.' && isdigit((unsigned char)*(p + 1))))
        {
            char *end;
            current_token->value.number = strtod(p, &end);
            if (end == p)
            { // Should not happen with the check above, but safety first
                fprintf(stderr, "%sError:%s Invalid numeric format near '%.*s'.\n", COLOR_RED, COLOR_RESET, 10, p);
                return -1;
            }
            current_token->type = TOKEN_NUMBER;
            // Check for percentage sign
            if (*end == '%')
            {
                current_token->is_percentage = 1;
                end++; // Consume the '%'
            }
            p = end; // Move parser position
            token_count++;
            expecting_operand = 0; // A number is an operand
        }
        // Handle operators (+, -, *, /, ^, %)
        else if (strchr("+-*/^%", *p))
        {
            // Special case: Unary minus (or plus, though less common)
            if ((*p == '-' || *p == '+') && expecting_operand)
            {
                // Treat as part of the number or implicitly multiplying by -1/1.
                // Simplest way for Shunting-Yard: Insert a zero operand before it.
                // ** MODIFIED: Insert 0 and then the operator (+ or -) **
                if (token_count >= max_tokens)
                {
                    fprintf(stderr, "%sError:%s Expression too complex (unary op).\n", COLOR_RED, COLOR_RESET);
                    return -1;
                }
                tokens[token_count].type = TOKEN_NUMBER;
                tokens[token_count].is_percentage = 0;
                tokens[token_count].value.number = 0.0; // Insert 0
                token_count++;

                if (token_count >= max_tokens)
                { // Check again before adding operator
                    fprintf(stderr, "%sError:%s Expression too complex (unary op).\n", COLOR_RED, COLOR_RESET);
                    return -1;
                }
                // Now add the actual operator (+ or -)
                tokens[token_count].type = TOKEN_OPERATOR;
                tokens[token_count].is_percentage = 0;
                tokens[token_count].value.op = char_to_op(*p);
                token_count++;
                p++;
                expecting_operand = 1; // Operator expects an operand next
            }
            else
            {
                // Binary operator
                OperatorType op = char_to_op(*p);
                if (op == (OperatorType)-1)
                { // Should not happen due to strchr check
                    fprintf(stderr, "%sError:%s Invalid operator '%c'.\n", COLOR_RED, COLOR_RESET, *p);
                    return -1;
                }
                current_token->type = TOKEN_OPERATOR;
                current_token->value.op = op;
                token_count++;
                p++;
                expecting_operand = 1; // Operator expects an operand next
            }
        }
        // Handle parentheses
        else if (*p == '(')
        {
            current_token->type = TOKEN_LPAREN;
            token_count++;
            p++;
            expecting_operand = 1; // Expect an operand inside parentheses
        }
        else if (*p == ')')
        {
            current_token->type = TOKEN_RPAREN;
            token_count++;
            p++;
            expecting_operand = 0; // After ')', expect an operator or end of expression
        }
        // Handle unrecognized characters
        else
        {
            fprintf(stderr, "%sError:%s Invalid character '%c' in expression.\n", COLOR_RED, COLOR_RESET, *p);
            return -1;
        }
    }
    return token_count; // Success
}

/**
 * @brief Gets the precedence level of an operator. Higher value means higher precedence.
 * @param op The operator type.
 * @return The precedence level (integer).
 */
int precedence(OperatorType op)
{
    switch (op)
    {
    case OP_ADD:
    case OP_SUB:
        return 1;
    case OP_MUL:
    case OP_DIV:
    case OP_MOD:
        return 2;
    case OP_POW:
        return 3; // Power has the highest precedence
    default:
        return 0; // Should not happen for valid operators
    }
}

/**
 * @brief Checks if an operator is left-associative.
 * Most operators are left-associative (e.g., a - b - c = (a - b) - c).
 * Power (^) is typically right-associative (e.g., a ^ b ^ c = a ^ (b ^ c)).
 * @param op The operator type.
 * @return 1 if left-associative, 0 otherwise.
 */
int is_left_associative(OperatorType op) { return op != OP_POW; }

/**
 * @brief Converts an infix expression (token array) to postfix (RPN) using the Shunting-yard algorithm.
 * @param tokens Input array of infix tokens.
 * @param token_count Number of tokens in the input array.
 * @param output Output array where postfix tokens will be stored.
 * @param max_output The maximum capacity of the `output` array.
 * @return The number of tokens in the postfix output, or -1 on error.
 */
int shunting_yard(Token *tokens, int token_count, Token *output, int max_output)
{
    Token op_stack[MAX_TOKENS]; // Stack for operators and parentheses
    int op_top = -1;            // Operator stack pointer (-1 means empty)
    int output_count = 0;       // Number of tokens added to the output queue

    for (int i = 0; i < token_count; i++)
    {
        Token token = tokens[i];
        switch (token.type)
        {
        case TOKEN_NUMBER:
            // Numbers go directly to the output queue
            if (output_count >= max_output)
            {
                fprintf(stderr, "%sError:%s Output queue overflow during parsing.\n", COLOR_RED, COLOR_RESET);
                return -1;
            }
            output[output_count++] = token;
            break;
        case TOKEN_FUNCTION:
            // Functions go onto the operator stack
            if (op_top >= MAX_TOKENS - 1)
            {
                fprintf(stderr, "%sError:%s Operator stack overflow (function).\n", COLOR_RED, COLOR_RESET);
                return -1;
            }
            op_stack[++op_top] = token;
            break;
        case TOKEN_OPERATOR:
            // Pop operators with higher or equal precedence (respecting associativity)
            while (op_top >= 0 && op_stack[op_top].type == TOKEN_OPERATOR &&
                   ((is_left_associative(token.value.op) && precedence(token.value.op) <= precedence(op_stack[op_top].value.op)) ||
                    (!is_left_associative(token.value.op) && precedence(token.value.op) < precedence(op_stack[op_top].value.op))))
            {
                if (output_count >= max_output)
                {
                    fprintf(stderr, "%sError:%s Output queue overflow (operator pop).\n", COLOR_RED, COLOR_RESET);
                    return -1;
                }
                output[output_count++] = op_stack[op_top--]; // Pop from stack to output
            }
            // Push the current operator onto the stack
            if (op_top >= MAX_TOKENS - 1)
            {
                fprintf(stderr, "%sError:%s Operator stack overflow (operator push).\n", COLOR_RED, COLOR_RESET);
                return -1;
            }
            op_stack[++op_top] = token;
            break;
        case TOKEN_LPAREN:
            // Push left parenthesis onto the operator stack
            if (op_top >= MAX_TOKENS - 1)
            {
                fprintf(stderr, "%sError:%s Operator stack overflow (left paren).\n", COLOR_RED, COLOR_RESET);
                return -1;
            }
            op_stack[++op_top] = token;
            break;
        case TOKEN_RPAREN:
            // Pop operators until a matching left parenthesis is found
            while (op_top >= 0 && op_stack[op_top].type != TOKEN_LPAREN)
            {
                if (output_count >= max_output)
                {
                    fprintf(stderr, "%sError:%s Output queue overflow (right paren pop).\n", COLOR_RED, COLOR_RESET);
                    return -1;
                }
                output[output_count++] = op_stack[op_top--]; // Pop from stack to output
            }
            // Check for mismatched parentheses
            if (op_top < 0)
            {
                fprintf(stderr, "%sError:%s Mismatched parentheses (extra right parenthesis?).\n", COLOR_RED, COLOR_RESET);
                return -1;
            }
            // Pop the left parenthesis itself (discard it)
            op_top--;
            // If the token before '(' was a function, pop it to output
            if (op_top >= 0 && op_stack[op_top].type == TOKEN_FUNCTION)
            {
                if (output_count >= max_output)
                {
                    fprintf(stderr, "%sError:%s Output queue overflow (function pop).\n", COLOR_RED, COLOR_RESET);
                    return -1;
                }
                output[output_count++] = op_stack[op_top--];
            }
            break;
        case TOKEN_ASSIGNMENT: // Should ideally be handled before shunting yard
            fprintf(stderr, "%sInternal Error:%s Assignment token found in shunting yard.\n", COLOR_RED, COLOR_RESET);
            return -1;
        case TOKEN_VARIABLE: // Should have been resolved to TOKEN_NUMBER by tokenizer
            fprintf(stderr, "%sInternal Error:%s Variable token found in shunting yard.\n", COLOR_RED, COLOR_RESET);
            return -1;
        }
    }
    // After processing all input tokens, pop any remaining operators from the stack to the output
    while (op_top >= 0)
    {
        // Check for leftover parentheses (mismatched)
        if (op_stack[op_top].type == TOKEN_LPAREN)
        {
            fprintf(stderr, "%sError:%s Mismatched parentheses (extra left parenthesis?).\n", COLOR_RED, COLOR_RESET);
            return -1;
        }
        if (output_count >= max_output)
        {
            fprintf(stderr, "%sError:%s Output queue overflow (final pop).\n", COLOR_RED, COLOR_RESET);
            return -1;
        }
        output[output_count++] = op_stack[op_top--];
    }
    return output_count; // Success, return number of tokens in postfix expression
}

/**
 * @brief Evaluates a postfix (RPN) expression.
 * @param postfix Array of tokens in postfix order.
 * @param count Number of tokens in the `postfix` array.
 * @return The calculated result (double), or NAN (Not a Number) on error.
 */
double evaluate_postfix(Token *postfix, int count)
{
    Value stack[MAX_TOKENS]; // Evaluation stack holding intermediate 'Value' structs
    int top = -1;            // Stack pointer (-1 means empty)

    for (int i = 0; i < count; i++)
    {
        Token token = postfix[i];
        if (token.type == TOKEN_NUMBER)
        {
            // Push numbers onto the stack
            if (top >= MAX_TOKENS - 1)
            {
                fprintf(stderr, "%sError:%s Evaluation stack overflow.\n", COLOR_RED, COLOR_RESET);
                return NAN;
            }
            stack[++top] = (Value){.num = token.value.number, .is_percentage = token.is_percentage};
        }
        else if (token.type == TOKEN_OPERATOR)
        {
            // Apply operator to the top two stack elements
            if (top < 1)
            { // Need at least two operands for binary operators
                fprintf(stderr, "%sError:%s Insufficient operands for operator '%c'.\n",
                        COLOR_RED, COLOR_RESET, "+-*/^%"[token.value.op]); // Simple way to get char
                return NAN;
            }
            Value op_b = stack[top--]; // Pop second operand
            Value op_a = stack[top--]; // Pop first operand
            double result;
            // Perform the operation
            switch (token.value.op)
            {
            case OP_ADD:
                result = op_b.is_percentage ? op_a.num + (op_b.num / 100.0 * op_a.num) : op_a.num + op_b.num;
                break;
            case OP_SUB:
                result = op_b.is_percentage ? op_a.num - (op_b.num / 100.0 * op_a.num) : op_a.num - op_b.num;
                break;
            case OP_MUL:
            {
                double a = op_a.is_percentage ? op_a.num / 100.0 : op_a.num;
                double b = op_b.is_percentage ? op_b.num / 100.0 : op_b.num;
                result = a * b;
            }
            break;
            case OP_DIV:
            {
                double a = op_a.is_percentage ? op_a.num / 100.0 : op_a.num;
                double b = op_b.is_percentage ? op_b.num / 100.0 : op_b.num;
                if (b == 0.0)
                {
                    fprintf(stderr, "%sError:%s Division by zero.\n", COLOR_RED, COLOR_RESET);
                    return NAN;
                }
                result = a / b;
            }
            break;
            case OP_POW:
                if (op_a.is_percentage || op_b.is_percentage)
                    fprintf(stderr, "%sWarning:%s Percentage ignored in power operation.\n", COLOR_YELLOW, COLOR_RESET);
                result = pow(op_a.num, op_b.num);
                break;
            case OP_MOD:
                if (op_a.is_percentage || op_b.is_percentage)
                    fprintf(stderr, "%sWarning:%s Percentage ignored in modulo operation.\n", COLOR_YELLOW, COLOR_RESET);
                if (op_b.num == 0.0)
                {
                    fprintf(stderr, "%sError:%s Modulo by zero.\n", COLOR_RED, COLOR_RESET);
                    return NAN;
                }
                result = fmod(op_a.num, op_b.num);
                break;
            default:
                fprintf(stderr, "%sInternal Error:%s Unknown operator type.\n", COLOR_RED, COLOR_RESET);
                return NAN;
            }
            // Push the result back onto the stack (result is never a percentage itself)
            if (top >= MAX_TOKENS - 1)
            { // Check before pushing
                fprintf(stderr, "%sError:%s Evaluation stack overflow after operation.\n", COLOR_RED, COLOR_RESET);
                return NAN;
            }
            stack[++top] = (Value){.num = result, .is_percentage = 0};
        }
        else if (token.type == TOKEN_FUNCTION)
        {
            // Apply function to the top stack element
            if (top < 0)
            { // Need at least one argument
                fprintf(stderr, "%sError:%s Insufficient arguments for function.\n", COLOR_RED, COLOR_RESET);
                // Could lookup function name here for better error message
                return NAN;
            }
            Value arg = stack[top--];                                       // Pop argument
            double arg_val = arg.is_percentage ? arg.num / 100.0 : arg.num; // Convert percentage if needed
            double result;
            // Evaluate the function
            switch (token.value.func)
            {
            // Trig functions (assume radians)
            case FUNC_SIN:
                result = sin(arg_val);
                break;
            case FUNC_COS:
                result = cos(arg_val);
                break;
            case FUNC_TAN:
                result = tan(arg_val);
                break;
            // Inverse trig functions
            case FUNC_ASIN:
                if (arg_val < -1.0 || arg_val > 1.0)
                {
                    fprintf(stderr, "%sError:%s Arcsin argument out of range [-1, 1].\n", COLOR_RED, COLOR_RESET);
                    return NAN;
                }
                result = asin(arg_val);
                break;
            case FUNC_ACOS:
                if (arg_val < -1.0 || arg_val > 1.0)
                {
                    fprintf(stderr, "%sError:%s Arccos argument out of range [-1, 1].\n", COLOR_RED, COLOR_RESET);
                    return NAN;
                }
                result = acos(arg_val);
                break;
            case FUNC_ATAN:
                result = atan(arg_val);
                break;
            // Log, Sqrt, Exp
            case FUNC_LOG:
                if (arg_val <= 0.0)
                {
                    fprintf(stderr, "%sError:%s Logarithm requires positive argument.\n", COLOR_RED, COLOR_RESET);
                    return NAN;
                }
                result = log(arg_val);
                break;
            case FUNC_SQRT:
                if (arg_val < 0.0)
                {
                    fprintf(stderr, "%sError:%s Square root requires non-negative argument.\n", COLOR_RED, COLOR_RESET);
                    return NAN;
                }
                result = sqrt(arg_val);
                break;
            case FUNC_EXP:
                result = exp(arg_val);
                break;
            // Utility functions
            case FUNC_ABS:
                result = fabs(arg_val);
                break;
            case FUNC_FLOOR:
                result = floor(arg_val);
                break;
            case FUNC_CEIL:
                result = ceil(arg_val);
                break;
            case FUNC_ROUND:
                result = round(arg_val);
                break;
            default: // Includes FUNC_INVALID
                fprintf(stderr, "%sInternal Error:%s Unknown function type.\n", COLOR_RED, COLOR_RESET);
                return NAN;
            }
            // Push the result back onto the stack (result is never a percentage)
            if (top >= MAX_TOKENS - 1)
            { // Check before pushing
                fprintf(stderr, "%sError:%s Evaluation stack overflow after function.\n", COLOR_RED, COLOR_RESET);
                return NAN;
            }
            stack[++top] = (Value){.num = result, .is_percentage = 0};
        }
    }
    // After processing all tokens, the final result should be the only item on the stack
    if (top != 0)
    {
        // This often indicates an invalid expression structure (e.g., "2 3 + 4")
        fprintf(stderr, "%sError:%s Invalid expression structure (stack top %d, expected 0).\n", COLOR_RED, COLOR_RESET, top);
        return NAN;
    }
    // Return the final result (handle potential remaining percentage)
    // This shouldn't happen if operators/functions clear the flag, but safety check:
    Value final_val = stack[0];
    return final_val.is_percentage ? final_val.num / 100.0 : final_val.num;
}

/**
 * @brief Handles variable assignment (e.g., "x = 1 + 2").
 * Parses and evaluates the expression on the right-hand side and assigns
 * the result to the specified variable.
 * @param var_name The name of the variable to assign to.
 * @param expression The expression string to evaluate.
 * @return The calculated result, or NAN on error.
 */
double handle_assignment(const char *var_name, const char *expression)
{
    Token tokens[MAX_TOKENS], postfix[MAX_TOKENS];
    // 1. Tokenize the right-hand side expression
    int token_count = tokenize(expression, tokens, MAX_TOKENS);
    if (token_count < 0)
    {
        log_message("Assignment Error: Tokenization failed for '%s = %s'", var_name, expression);
        return NAN;
    }
    if (token_count == 0)
    {
        fprintf(stderr, "%sError:%s Missing expression after '=' for assignment to '%s'.\n", COLOR_RED, COLOR_RESET, var_name);
        log_message("Assignment Error: Missing expression for '%s'", var_name);
        return NAN;
    }
    // 2. Convert the expression to postfix (RPN)
    int postfix_count = shunting_yard(tokens, token_count, postfix, MAX_TOKENS);
    if (postfix_count < 0)
    {
        log_message("Assignment Error: Shunting-yard failed for '%s = %s'", var_name, expression);
        return NAN;
    }
    // 3. Evaluate the postfix expression
    double result = evaluate_postfix(postfix, postfix_count);
    // 4. Assign the result to the variable if evaluation was successful
    if (!isnan(result))
    {
        if (set_variable(var_name, result) < 0)
        {
            log_message("Assignment Error: Failed to store result %g in variable '%s'", result, var_name);
            return NAN;
        }
        // Update last_result and 'ans' only if assignment is successful
        last_result = result;
        set_variable("ans", result); // Keep 'ans' variable updated
        log_message("Assignment: %s = %g (Expression: '%s')", var_name, result, expression);
    }
    else
    {
        log_message("Assignment Error: Evaluation failed for '%s = %s'", var_name, expression);
    }
    return result; // Return the calculated value (or NAN if evaluation failed)
}

/**
 * @brief Processes built-in text commands (non-mathematical).
 * @param input The user input string.
 * @return
 * 1 if a command was processed (and no further evaluation needed).
 * -1 if the 'exit' command was given.
 * 0 if the input was not a recognized command (should be treated as expression).
 */
int process_command(const char *input)
{
    // Trim leading/trailing whitespace for simpler comparison
    char trimmed_input[NMRI_MAX_INPUT];
    const char *start = input;
    while (isspace((unsigned char)*start))
        start++;
    strncpy(trimmed_input, start, NMRI_MAX_INPUT - 1);
    trimmed_input[NMRI_MAX_INPUT - 1] = '\0';
    char *end = trimmed_input + strlen(trimmed_input) - 1;
    while (end >= trimmed_input && isspace((unsigned char)*end))
        *end-- = '\0';

    // Handle simple commands
    if (strcmp(trimmed_input, "help") == 0)
    {
        show_help();
        return 1;
    }
    if (strcmp(trimmed_input, "exit") == 0 || strcmp(trimmed_input, "quit") == 0)
    {
        return -1;
    }
    if (strcmp(trimmed_input, "clear") == 0 || strcmp(trimmed_input, "cls") == 0)
    {
        printf("\033[2J\033[H");
        return 1;
    }
    if (strcmp(trimmed_input, "history") == 0)
    {
        show_history();
        return 1;
    }
    if (strcmp(trimmed_input, "variables") == 0 || strcmp(trimmed_input, "vars") == 0)
    {
        show_variables();
        return 1;
    }
    if (strcmp(trimmed_input, "memory") == 0 || strcmp(trimmed_input, "mem") == 0)
    {
        printf("Memory: %g\n", memory);
        return 1;
    }
    if (strcmp(trimmed_input, "m+") == 0)
    {
        memory += last_result;
        printf("Memory = %g (added %g)\n", memory, last_result);
        log_message("Memory += %g --> %g", last_result, memory);
        return 1;
    }
    if (strcmp(trimmed_input, "m-") == 0)
    {
        memory -= last_result;
        printf("Memory = %g (subtracted %g)\n", memory, last_result);
        log_message("Memory -= %g --> %g", last_result, memory);
        return 1;
    }
    if (strcmp(trimmed_input, "mr") == 0)
    {
        printf("Recalled from memory: %g\n", memory);
        last_result = memory;
        set_variable("ans", last_result);
        log_message("Memory Recall (mr): %g", memory);
        return 1;
    }
    if (strcmp(trimmed_input, "mc") == 0)
    {
        memory = 0.0;
        printf("Memory cleared.\n");
        log_message("Memory Cleared (mc)");
        return 1;
    }
    // Handle 'store <varname>' command
    if (strncmp(trimmed_input, "store ", 6) == 0)
    {
        const char *var_name = trimmed_input + 6;
        while (isspace((unsigned char)*var_name))
            var_name++;
        if (*var_name == '\0')
        {
            fprintf(stderr, "%sError:%s Missing variable name for 'store' command.\n", COLOR_RED, COLOR_RESET);
            log_message("Command Error: Missing variable name for store");
            return 1;
        }
        char name_buf[MAX_IDENTIFIER_LEN] = {0};
        int i = 0;
        const char *name_start = var_name;
        if (!isalpha((unsigned char)*name_start) && *name_start != '_')
        {
            fprintf(stderr, "%sError:%s Invalid variable name '%s' for 'store'. Must start with a letter or underscore.\n", COLOR_RED, COLOR_RESET, name_start);
            log_message("Command Error: Invalid store variable name '%s'", name_start);
            return 1;
        }
        while (*var_name && !isspace((unsigned char)*var_name) && i < MAX_IDENTIFIER_LEN - 1)
        {
            if (!isalnum((unsigned char)*var_name) && *var_name != '_')
            {
                fprintf(stderr, "%sError:%s Invalid character '%c' in variable name for 'store'.\n", COLOR_RED, COLOR_RESET, *var_name);
                log_message("Command Error: Invalid char in store variable name '%s'", name_start);
                return 1;
            }
            name_buf[i++] = *var_name++;
        }
        name_buf[i] = '\0';
        if (*var_name != '\0' && !isspace((unsigned char)*var_name))
        {
            fprintf(stderr, "%sError:%s Variable name '%s...' too long or invalid for 'store'.\n", COLOR_RED, COLOR_RESET, name_buf);
            log_message("Command Error: Store variable name too long or invalid '%s'", name_buf);
            return 1;
        }
        if (set_variable(name_buf, last_result) >= 0)
        {
            printf("Stored %g in variable '%s'\n", last_result, name_buf);
            log_message("Command: Stored %g in variable '%s'", last_result, name_buf);
        }
        else
        {
            log_message("Command Error: Failed to store %g in variable '%s'", last_result, name_buf);
        }
        return 1;
    }
    // Handle 'log ...' commands
    if (strncmp(trimmed_input, "log ", 4) == 0)
    {
        const char *subcommand = trimmed_input + 4;
        while (isspace((unsigned char)*subcommand))
            subcommand++;
        if (strcmp(subcommand, "on") == 0)
        {
            if (!logging_enabled)
            {
                if (init_logging())
                {
                    logging_enabled = 1;
                    printf("%sLogging enabled.%s To file: %s\n", COLOR_GREEN, COLOR_RESET, log_path);
                    log_session_start();
                    log_message("Command: Logging enabled");
                }
            }
            else
            {
                printf("Logging is already enabled. Log file: %s\n", log_path);
            }
            return 1;
        }
        if (strcmp(subcommand, "off") == 0)
        {
            if (logging_enabled)
            {
                log_message("Command: Logging disabled");
                log_session_stop();
                logging_enabled = 0;
                printf("%sLogging disabled.%s\n", COLOR_YELLOW, COLOR_RESET);
            }
            else
            {
                printf("Logging is already disabled.\n");
            }
            return 1;
        }
        if (strcmp(subcommand, "show") == 0)
        {
            log_message("Command: Show log requested");
            show_log(HISTORY_SIZE);
            return 1;
        }
        if (strcmp(subcommand, "file") == 0)
        {
            printf("Current log file path: %s%s%s\n", COLOR_YELLOW, log_path, COLOR_RESET);
            log_message("Command: Log file path requested (%s)", log_path);
            return 1;
        }
        if (strncmp(subcommand, "file ", 5) == 0)
        {
            const char *new_path = subcommand + 5;
            while (isspace((unsigned char)*new_path))
                new_path++;
            if (*new_path)
            {
                close_logging();
                strncpy(log_path, new_path, sizeof(log_path) - 1);
                log_path[sizeof(log_path) - 1] = '\0';
                printf("Log file path set to: %s%s%s\n", COLOR_YELLOW, log_path, COLOR_RESET);
                if (logging_enabled)
                {
                    init_logging();
                    log_session_start();
                    log_message("Command: Log file path changed to %s", log_path);
                }
                else
                {
                    log_message("Command: Log file path set to %s (logging is off)", log_path);
                }
            }
            else
            {
                printf("Usage: log file <new_path>\n");
            }
            return 1;
        }
        fprintf(stderr, "%sError:%s Unknown 'log' subcommand '%s'. Use 'on', 'off', 'show', 'file', or 'file <path>'.\n", COLOR_RED, COLOR_RESET, subcommand);
        log_message("Command Error: Unknown log subcommand '%s'", subcommand);
        return 1;
    }
    // If none of the above, it's not a recognized command
    return 0;
}

/**
 * @brief Evaluates a complete mathematical expression string.
 * This function orchestrates the tokenization, Shunting-yard conversion,
 * and postfix evaluation steps. Updates `last_result` and 'ans' variable on success.
 * @param input The mathematical expression string.
 * @return The calculated result, or NAN on error.
 */
double evaluate_expression(const char *input)
{
    Token tokens[MAX_TOKENS], postfix[MAX_TOKENS];
    // 1. Tokenize
    int token_count = tokenize(input, tokens, MAX_TOKENS);
    if (token_count < 0)
    {
        log_message("Evaluation Error: Tokenization failed for '%s'", input);
        return NAN;
    }
    if (token_count == 0)
    {
        return NAN;
    } // Empty expression is invalid for evaluation
    // 2. Convert to Postfix (RPN)
    int postfix_count = shunting_yard(tokens, token_count, postfix, MAX_TOKENS);
    if (postfix_count < 0)
    {
        log_message("Evaluation Error: Shunting-yard failed for '%s'", input);
        return NAN;
    }
    // 3. Evaluate Postfix
    double result = evaluate_postfix(postfix, postfix_count);
    // Update global state if successful
    if (!isnan(result))
    {
        last_result = result;
        set_variable("ans", result);
        log_message("Result: %s = %g", input, result);
    }
    else
    {
        log_message("Evaluation Error: Postfix evaluation failed for '%s'", input);
    }
    return result;
}

/**
 * @brief Cleans up floating point results that are very close to zero.
 * @param value The double value to clean.
 * @param epsilon The threshold below which values are considered zero.
 * @return The cleaned value (0.0 if very close to zero, original value otherwise).
 */
double clean_near_zero(double value, double epsilon)
{
    return (fabs(value) < epsilon) ? 0.0 : value;
}

/* --- Terminal Raw Mode & Input Handling --- */

/**
 * @brief Restores the original terminal settings upon program exit.
 */
void disableRawMode(void)
{
    // Restore original terminal attributes
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf("%s", COLOR_RESET); // Ensure colors are reset on exit
}

/**
 * @brief Enables raw mode for the terminal.
 * Disables canonical mode (line buffering) and echoing of input characters.
 * Registers `disableRawMode` to be called automatically on exit.
 */
void enableRawMode(void)
{
    // Get current terminal attributes
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    {
        perror("Error getting terminal attributes");
        exit(EXIT_FAILURE);
    }
    // Register the cleanup function to restore settings on exit
    atexit(disableRawMode);
    // Create a copy and modify it for raw mode
    struct termios raw = orig_termios;
    // ICANON: Disable canonical mode (don't wait for Enter)
    // ECHO: Disable echoing input characters
    raw.c_lflag &= ~(ECHO | ICANON);
    // VMIN = 1: read() waits for at least 1 byte
    // VTIME = 0: read() waits indefinitely
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    // Apply the modified attributes
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    {
        perror("Error setting terminal attributes");
        exit(EXIT_FAILURE);
    }
}

// Define key codes for clarity
#define KEY_ENTER '\n'
#define KEY_RETURN '\r'
#define KEY_BACKSPACE 127 // ASCII DEL character often used for backspace
#define KEY_CTRL_A 1
#define KEY_CTRL_D 4
#define KEY_CTRL_E 5
#define KEY_ESC 27

/**
 * @brief Reads a line of input from the user with basic line editing features.
 * Supports backspace, delete (Ctrl+D), moving cursor (left/right), history (up/down),
 * jumping to start/end (Ctrl+A/Ctrl+E). Uses raw terminal mode.
 * @param buffer Buffer to store the read command.
 * @param max_size Maximum size of the `buffer`.
 */
void readCommand(char *buffer, int max_size)
{
    enableRawMode(); // Switch to raw mode for character-by-character input
    int pos = 0, len = 0, history_pos = history_count, saved_current = 0;
    char current_typed[NMRI_MAX_INPUT] = {0}; // Buffer to save current input when navigating history
    memset(buffer, 0, max_size);              // Clear the input buffer initially
    // Print the prompt
    printf("%s%s■%s ", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    fflush(stdout);

    while (1)
    {
        char c;
        // Read one character from standard input
        if (read(STDIN_FILENO, &c, 1) != 1)
        {
            perror("readCommand read error");
            break;
        }

        // Handle character input
        if (c == KEY_ENTER || c == KEY_RETURN)
        {
            printf("\n");
            break;
        } // Enter pressed
        else if (c == KEY_BACKSPACE)
        { // Backspace pressed
            if (pos > 0)
            {
                memmove(buffer + pos - 1, buffer + pos, len - pos);
                len--;
                pos--;
                buffer[len] = '\0';
                // Redraw line: move cursor left, print shifted part, print space, move cursor back
                printf("\033[1D%s \033[%dD", buffer + pos, len - pos + 1);
            }
        }
        else if (c == KEY_CTRL_D)
        { // Ctrl+D (Delete character under cursor)
            if (pos < len)
            {
                memmove(buffer + pos, buffer + pos + 1, len - pos - 1);
                len--;
                buffer[len] = '\0';
                // Redraw line: print shifted part, print space, move cursor back
                printf("%s \033[%dD", buffer + pos, len - pos + 1);
            }
        }
        else if (c == KEY_CTRL_A)
        { // Ctrl+A (Move cursor to beginning of line)
            if (pos > 0)
            {
                printf("\033[%dD", pos);
                pos = 0;
            }
        }
        else if (c == KEY_CTRL_E)
        { // Ctrl+E (Move cursor to end of line)
            if (pos < len)
            {
                printf("\033[%dC", len - pos);
                pos = len;
            }
        }
        else if (c == KEY_ESC)
        { // Escape sequence (likely arrow keys or other special keys)
            char seq[3];
            // Read the next two characters (e.g., '[A' for up arrow)
            if (read(STDIN_FILENO, &seq[0], 1) != 1)
                continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1)
                continue;
            if (seq[0] == '[')
            {
                if (seq[1] >= '0' && seq[1] <= '9')
                {
                    // Extended escape sequence (like Home/End/Delete on some terminals)
                    if (read(STDIN_FILENO, &seq[2], 1) != 1)
                        continue; // Read '~'
                    if (seq[1] == '3' && seq[2] == '~')
                    { // Delete key
                        // Same logic as Ctrl+D
                        if (pos < len)
                        {
                            memmove(buffer + pos, buffer + pos + 1, len - pos - 1);
                            len--;
                            buffer[len] = '\0';
                            printf("%s \033[%dD", buffer + pos, len - pos + 1);
                        }
                    }
                    // Add more cases for Home (^[1~), End (^[4~) if needed
                }
                else
                {
                    switch (seq[1])
                    {
                    case 'A': // Up Arrow (History Previous)
                        if (!saved_current && len > 0)
                        {
                            strncpy(current_typed, buffer, NMRI_MAX_INPUT - 1);
                            current_typed[NMRI_MAX_INPUT - 1] = '\0';
                            saved_current = 1;
                        }
                        if (history_pos > 0)
                        {
                            history_pos--;
                            // Clear current line display
                            printf("\r%s%s■%s ", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
                            for (int i = 0; i < len; i++)
                                printf(" ");
                            printf("\r%s%s■%s ", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
                            // Load history item
                            strncpy(buffer, command_history[history_pos], max_size - 1);
                            buffer[max_size - 1] = '\0';
                            len = strlen(buffer);
                            pos = len;
                            printf("%s", buffer);
                        }
                        break;
                    case 'B': // Down Arrow (History Next)
                        if (history_pos < history_count)
                        {
                            history_pos++;
                            // Clear current line display
                            printf("\r%s%s■%s ", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
                            for (int i = 0; i < len; i++)
                                printf(" ");
                            printf("\r%s%s■%s ", COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
                            // Load next history item or the saved current input
                            if (history_pos == history_count && saved_current)
                            {
                                strncpy(buffer, current_typed, max_size - 1);
                                buffer[max_size - 1] = '\0';
                            }
                            else if (history_pos < history_count)
                            {
                                strncpy(buffer, command_history[history_pos], max_size - 1);
                                buffer[max_size - 1] = '\0';
                            }
                            else
                            {
                                buffer[0] = '\0';
                                saved_current = 0;
                            } // End of history, clear buffer
                            len = strlen(buffer);
                            pos = len;
                            printf("%s", buffer);
                        }
                        break;
                    case 'C':
                        if (pos < len)
                        {
                            printf("\033[1C");
                            pos++;
                        }
                        break; // Right Arrow
                    case 'D':
                        if (pos > 0)
                        {
                            printf("\033[1D");
                            pos--;
                        }
                        break; // Left Arrow
                    } // End switch(seq[1])
                } // End else (seq[1] is not 0-9)
            } // End if (seq[0] == '[')
        }
        else if (!iscntrl((unsigned char)c) && len < max_size - 1)
        { // Printable character
            // Insert character at cursor position
            if (pos == len)
            { // Inserting at the end
                buffer[pos++] = c;
                len++;
                buffer[len] = '\0';
                printf("%c", c);
            }
            else
            {                                                       // Inserting in the middle
                memmove(buffer + pos + 1, buffer + pos, len - pos); // Shift right
                buffer[pos++] = c;
                len++;
                buffer[len] = '\0';
                // Redraw the part of the line from the cursor onwards
                printf("%s", buffer + pos - 1);
                // Move cursor back to correct position
                printf("\033[%dD", len - pos);
            }
        }
        fflush(stdout); // Ensure prompt and input are displayed immediately
    }
    disableRawMode();   // Restore terminal settings before returning
    buffer[len] = '\0'; // Ensure final null termination
}

/* --- Main Function --- */

#ifndef FOR_TESTING // Allow compiling without main for testing purposes
int main(int argc, char *argv[])
{
    // --- Option Parsing ---
    // int opt; // Removed unused variable
    // Add getopt loop here if options like -h, -v, -l logfile are needed
    // For now, assume optind starts at 1 if no options are parsed
    // TODO: Implement getopt for options if needed later
    optind = 1; // Reset optind if getopt is not used or finished

    // Initialize 'ans' and logging
    set_variable("ans", 0.0);
    init_logging(); // Initialize logging system

    // --- Check for non-option arguments (the expression) ---
    if (optind < argc)
    {
        // --- Command-Line Expression Mode ---
        char expression_buffer[CMD_LINE_EXPR_BUFFER_SIZE];
        expression_buffer[0] = '\0';
        size_t current_len = 0;

        // Concatenate all non-option arguments
        for (int i = optind; i < argc; i++)
        {
            size_t arg_len = strlen(argv[i]);
            // Check for potential buffer overflow before strcat
            if (current_len + arg_len + (current_len > 0 ? 1 : 0) >= sizeof(expression_buffer))
            {
                fprintf(stderr, "%sError:%s Command line expression too long.\n", COLOR_RED, COLOR_RESET);
                close_logging(); // Close log before exiting
                return 1;
            }
            // Add space separator if not the first argument
            if (current_len > 0)
            {
                strcat(expression_buffer, " ");
                current_len++;
            }
            strcat(expression_buffer, argv[i]);
            current_len += arg_len;
        }

        log_message("Command line execution: %s", expression_buffer);

        // Evaluate the expression
        double result = evaluate_expression(expression_buffer);

        if (isnan(result))
        {
            // Error message already printed by evaluate_expression
            log_message("Command line result: Error");
            close_logging();
            return 1; // Indicate error
        }
        else
        {
            // Print result (consistent format)
            // Use clean_near_zero to avoid printing "-0"
            printf("%s%g%s\n", COLOR_GREEN, clean_near_zero(result, 1e-10), COLOR_RESET);
            log_message("Command line result: %g", result);
            close_logging();
            return 0; // Indicate success
        }
    }
    else
    {
        // --- Interactive Mode ---
        printf("\n%sNMRI Command Line Calculator%s\n", COLOR_BOLD, COLOR_RESET);
        printf("Type '%shelp%s' for instructions, '%sexit%s' to quit.\n\n",
               COLOR_GREEN, COLOR_RESET, COLOR_GREEN, COLOR_RESET);

        log_session_start(); // Log start of interactive session

        while (1)
        {
            char input[NMRI_MAX_INPUT];
            readCommand(input, sizeof(input)); // Use line editing function

            // Trim leading whitespace (readCommand might leave some if only Enter is pressed)
            char *start = input;
            while (isspace((unsigned char)*start))
                start++;
            if (*start == '\0')
                continue; // Ignore empty lines

            // Add non-empty command to history
            add_to_history(start);
            // Log the raw input
            log_message("User input: %s", start);

            // Process built-in commands first
            int cmd_result = process_command(start);
            if (cmd_result == 1)
                continue; // Command was handled, loop for next input
            if (cmd_result == -1)
            { // Exit command received
                log_message("User requested exit.");
                printf("\n%s%sGoodbye!%s\n", COLOR_BOLD, COLOR_GREEN, COLOR_RESET);
                break;
            }

            // If not a built-in command, check for assignment or treat as expression

            // Check for assignment: identifier = expression
            char *equals_pos = strchr(start, '=');
            char *first_op = strpbrk(start, "+-*/^%"); // Find first non-assignment operator
            // Check if '=' exists and appears *before* any other operator (or if no other ops exist)
            // Also ensure it's not the first character (e.g. "= 5")
            if (equals_pos != NULL && equals_pos != start && (first_op == NULL || equals_pos < first_op))
            {
                // Potential assignment found
                char var_name[MAX_IDENTIFIER_LEN];
                char *name_end = equals_pos;
                // Trim whitespace before '='
                while (name_end > start && isspace((unsigned char)*(name_end - 1)))
                    name_end--;

                size_t name_len = name_end - start;
                if (name_len > 0 && name_len < MAX_IDENTIFIER_LEN)
                {
                    strncpy(var_name, start, name_len);
                    var_name[name_len] = '\0';

                    // Validate variable name (starts with letter/_, contains letter/digit/_)
                    int valid_name = (isalpha((unsigned char)var_name[0]) || var_name[0] == '_');
                    for (size_t k = 1; k < name_len && valid_name; k++)
                    {
                        if (!isalnum((unsigned char)var_name[k]) && var_name[k] != '_')
                            valid_name = 0;
                    }
                    // Simplified reserved word check - expand this list as needed
                    if (valid_name && (strcmp(var_name, "help") == 0 || strcmp(var_name, "exit") == 0 ||
                                       strcmp(var_name, "pi") == 0 || strcmp(var_name, "e") == 0 ||
                                       strcmp(var_name, "sin") == 0 /* add more reserved words */))
                    {
                        fprintf(stderr, "%sError:%s Cannot assign to reserved name '%s'.\n", COLOR_RED, COLOR_RESET, var_name);
                        log_message("Assignment Error: Attempt to assign to reserved name '%s'", var_name);
                        valid_name = 0;
                    }

                    if (valid_name)
                    {
                        const char *expr_start = equals_pos + 1; // Start of the expression part
                        double result = handle_assignment(var_name, expr_start);
                        if (!isnan(result))
                        {
                            // Print assignment result
                            printf("%s%s = %s%g%s\n", COLOR_YELLOW, var_name, COLOR_GREEN, clean_near_zero(result, 1e-10), COLOR_RESET);
                        }
                        else
                        {
                            // Error message already printed by handle_assignment or its sub-functions
                            log_message("Assignment failed for: %s", start);
                        }
                        continue; // Assignment handled, proceed to next input
                    }
                    else
                    {
                        // Error message for invalid name already printed if needed
                        // Or print a generic one here if validation fails silently
                        fprintf(stderr, "%sError:%s Invalid variable name '%s' for assignment.\n", COLOR_RED, COLOR_RESET, var_name);
                        log_message("Assignment Error: Invalid variable name '%s'", var_name);
                        continue; // Invalid assignment syntax
                    }
                }
                else
                {
                    fprintf(stderr, "%sError:%s Invalid variable name length for assignment.\n", COLOR_RED, COLOR_RESET);
                    log_message("Assignment Error: Invalid variable name length near '%s'", start);
                    continue;
                }
            }

            // If not an assignment or command, evaluate as a mathematical expression
            double result = evaluate_expression(start);
            if (!isnan(result))
            {
                // Print the final result, cleaning near-zero values
                printf("%s%g%s\n", COLOR_GREEN, clean_near_zero(result, 1e-10), COLOR_RESET);
            }
            // Error messages and logging handled within evaluate_expression
        } // End while(1)

        close_logging(); // Close the log file properly
        // disableRawMode() is called automatically via atexit()
        return 0; // Success
    }
}
#endif // FOR_TESTING
