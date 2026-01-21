# TSH: A Tiny Shell with Job Control

A fully functional UNIX shell implementation written in C, demonstrating advanced systems programming concepts including process management, job control, signal handling, I/O redirection, and piping.

## 🎯 Project Overview

This project implements `tsh` (Tiny Shell), a complete command-line shell that supports:
- **Job control** with foreground/background process management
- **Signal handling** for interrupt (Ctrl-C) and suspend (Ctrl-Z) operations
- **Built-in commands**: `quit`, `jobs`, `bg`, `fg`
- **I/O redirection**: input (`<`) and output (`>`) redirection
- **Piping**: multi-process pipelines using the pipe operator (`|`)
- **Concurrent job management**: supports up to 16 simultaneous jobs

The main implementation file is located at: `Project-Student-Bash-Shell/tsh.c`

## 🛠️ Technologies Used

### Programming Language
- **C** - Core implementation language for low-level systems programming

### UNIX System Calls & APIs
- **Process Management**: `fork()`, `execve()`, `waitpid()`, `setpgid()`
- **Signal Handling**: `sigaction()`, `sigprocmask()`, `kill()`, `sigsuspend()`
- **I/O Operations**: `pipe()`, `dup2()`, `open()`, `close()`
- **Process Groups**: Process group manipulation for proper job control
- **Error Handling**: `errno`, `perror()`, error checking throughout

### Development Tools
- **Make** - Build system and test automation
- **Perl** - Trace-driven shell driver (`sdriver.pl`) for automated testing
- **GCC** - Compiler with `-Wall -Werror -O2` flags for strict error checking

## 🧠 Computer Science Principles Demonstrated

### 1. **Process Management & Job Control**
- Process creation via `fork()` and execution via `execve()`
- Process group management to isolate background jobs from terminal signals
- State machine implementation for job states: Foreground (FG), Background (BG), Stopped (ST)
- Job lifecycle management with proper cleanup and zombie process prevention

### 2. **Concurrency & Synchronization**
- Signal blocking/unblocking to prevent race conditions
- Synchronization between parent and child processes using volatile variables
- Asynchronous signal handling with proper reentrancy considerations
- Concurrent job execution with proper state tracking

### 3. **Signal Handling**
- **SIGCHLD**: Reaping terminated/stopped child processes
- **SIGINT**: Forwarding Ctrl-C to foreground job only
- **SIGTSTP**: Stopping foreground job with Ctrl-Z
- **SIGUSR1**: Synchronization signal for process group setup
- Safe signal handler design with `sig_atomic_t` variables

### 4. **I/O Redirection & Piping**
- File descriptor manipulation using `dup2()` for redirection
- Inter-process communication via anonymous pipes
- Multi-stage pipeline implementation with proper process chaining
- Input/output redirection validation and error handling

### 5. **Command Parsing & Execution**
- String parsing with support for quoted arguments
- Argument vector construction for program execution
- Background job detection via `&` operator
- Built-in command routing vs external command execution

### 6. **Error Handling & Robustness**
- Comprehensive error checking for all system calls
- Graceful degradation on invalid commands
- Proper cleanup of resources (file descriptors, process groups)
- Validation of command-line syntax before execution

### 7. **Memory Management**
- Stack-based job management with fixed-size arrays
- String manipulation with buffer bounds checking
- Proper handling of command-line arguments and environment variables

## 📋 Features

### Built-in Commands
- `quit` - Exit the shell
- `jobs` - List all running and stopped jobs
- `fg <pid|%jid>` - Bring a background/stopped job to foreground
- `bg <pid|%jid>` - Resume a stopped job in background

### External Command Execution
- Execute any UNIX command from `PATH`
- Support for command-line arguments
- Proper error reporting for non-existent commands

### Job Control
- **Foreground jobs**: Block shell until completion
- **Background jobs**: Execute asynchronously with job ID reporting
- **Stopped jobs**: Suspend and resume capability
- Job state transitions handled correctly

### Advanced Features
- **I/O Redirection**: 
  - `< file` - Redirect input from file
  - `> file` - Redirect output to file
- **Piping**: 
  - `cmd1 | cmd2 | cmd3` - Multi-stage pipelines
  - Sequential execution with proper synchronization

## 🚀 How to Use

### Building the Project

```bash
cd Project-Student-Bash-Shell
make
```

This compiles:
- `tsh` - The main shell executable
- Helper test programs: `myspin`, `mysplit`, `mystop`, `myint`

### Running the Shell

```bash
./tsh
```

You'll see the prompt:
```
tsh> 
```

### Usage Examples

#### Basic Commands
```bash
tsh> /bin/ls -la          # List directory contents
tsh> /bin/echo "Hello"    # Print message
tsh> jobs                 # List all jobs
tsh> quit                 # Exit shell
```

#### Background Jobs
```bash
tsh> /bin/sleep 10 &      # Run in background
[1] (12345) /bin/sleep 10 &
tsh> jobs                 # Check job status
[1] (12345) Running /bin/sleep 10 &
```

#### Job Control
```bash
tsh> /bin/sleep 20        # Start foreground job
^Z                         # Press Ctrl-Z to stop
Job [1] (12345) stopped by signal 20
tsh> fg %1                # Resume in foreground
tsh> bg %1                # Resume in background
```

#### I/O Redirection
```bash
tsh> /bin/cat < input.txt          # Read from file
tsh> /bin/ls > output.txt          # Write to file
tsh> /bin/wc < input.txt > count.txt  # Both redirections
```

#### Piping
```bash
tsh> /bin/ls | /bin/grep c          # Simple pipe
tsh> /bin/ls | /bin/grep e | /bin/grep drive  # Multi-stage pipe
```

### Signal Handling
- **Ctrl-C**: Interrupts foreground job only
- **Ctrl-Z**: Stops foreground job (can be resumed with `fg` or `bg`)
- **Ctrl-D**: Exits the shell (EOF)

### Command-Line Options
```bash
./tsh -h          # Print help message
./tsh -v          # Verbose mode (debug output)
./tsh -p          # No prompt (for automated testing)
```

## 🧪 Testing

The project includes comprehensive trace-driven testing:

```bash
make test01       # Run trace01 test
make test02       # Run trace02 test
# ... through test20
```

Or run all tests manually:
```bash
make test01 test02 test03 ... test20
```

Compare with reference implementation:
```bash
make rtest01      # Run reference shell on trace01
```

The test suite validates:
- Basic command execution
- Built-in commands
- Signal handling
- Job control (fg/bg)
- I/O redirection
- Piping
- Error handling
- Edge cases

## 📁 Project Structure

```
CSC209H5-Winter-2024-Student-UNIX-Shell/
├── README.md                          # This file
├── Project-Student-Bash-Shell/
│   ├── tsh.c                          # Main shell implementation
│   ├── makefile                       # Build configuration
│   ├── sdriver.pl                     # Test driver script
│   ├── trace*.txt                     # Test trace files (01-20)
│   ├── myint.c, myspin.c, etc.        # Helper test programs
│   └── __starter_code_DO_NOT_MODIFY/  # Original starter code
```

## 🎓 Course Context

This project was completed for **CSC209H5 - Software Tools and Systems Programming** at the University of Toronto (Winter 2024). It demonstrates mastery of:
- Low-level systems programming in C
- UNIX process model and job control
- Asynchronous programming with signals
- Complex state management
- Robust error handling and edge case coverage

## 💡 Key Implementation Highlights

1. **Signal Safety**: Proper signal blocking/unblocking prevents race conditions during job list manipulation
2. **Process Groups**: Each child process gets its own process group to isolate signals
3. **Zombie Prevention**: Comprehensive child reaping with `waitpid()` and `WNOHANG`
4. **Pipeline Synchronization**: Sequential execution of pipeline stages with proper error propagation
5. **Error Validation**: Extensive command-line syntax checking before execution

## 📝 Notes

- The shell uses `execve()` directly, not `execvp()`, requiring full paths or proper PATH setup
- Maximum of 16 concurrent jobs (configurable via `MAXJOBS`)
- Command line limited to 1024 characters (`MAXLINE`)
- Maximum 128 arguments per command (`MAXARGS`)
