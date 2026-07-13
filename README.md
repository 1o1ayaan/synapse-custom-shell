# Synapse Custom Shell

Synapse is a custom, POSIX-compliant Linux shell written in C++ that features native integration with Google's Gemini AI. It provides a standard command-line interface with the added superpower of having an AI assistant available directly in your terminal without blocking your workflow.

## Features

- **Standard Command Execution:** Runs all your standard external Linux commands (like `ls`, `grep`, `mkdir`, etc.) using standard `fork()` and `execvp()`.
- **Integrated AI Assistant:** Use the built-in `ask` command to query the Gemini AI (using the Gemini 3.1-flash-lite model). The AI runs on a background thread so you can continue using the shell while it generates a response!
- **Background Multithreading:** Append an `&` to any command (e.g., `sleep 10 &`) to run it in the background using `pthread` without blocking the main shell loop.
- **Built-in Commands:**
  - `cd <path>`: Change the current directory.
  - `cd -`: Toggle back to the previous directory.
  - `exit`: Safely exit the shell.
- **History & Tokenization:** Supports shell history and proper quote parsing via the GNU Readline library.

## Prerequisites

To build and run Synapse, you will need:
- A C++ compiler (like `g++` or `clang++`)
- **libcurl** (for making HTTP requests to the Gemini API)
  - Ubuntu/Debian: `sudo apt-get install libcurl4-openssl-dev`
- **GNU Readline** (for handling user input and command history)
  - Ubuntu/Debian: `sudo apt-get install libreadline-dev`

*(Note: The `json.hpp` header for JSON parsing is already included in the project directory.)*

## Installation & Setup

1. **Clone the repository:**
   ```bash
   git clone https://github.com/1o1ayaan/synapse-custom-shell.git
   cd synapse-custom-shell
   ```

2. **Build the project:**
   Compile the shell using the provided Makefile.
   ```bash
   make
   ```

3. **Set your Gemini API Key:**
   Synapse requires a valid Gemini API key to use the `ask` command. You must set it as an environment variable before running the shell.
   ```bash
   export GEMINI_API_KEY="your_api_key_here"
   ```

4. **Run Synapse:**
   ```bash
   ./synapse
   ```

## Usage Example

Once inside the Synapse shell, you will see a prompt like `synapse:[/current/directory]>`. 

```bash
# Run a standard command
synapse:[/home/user]> ls -la

# Run a command in the background
synapse:[/home/user]> sleep 5 &

# Ask the AI a question (this runs in the background so you can keep typing)
synapse:[/home/user]> ask Write a python script to reverse a string

# Change directories
synapse:[/home/user]> cd Documents
synapse:[/home/user/Documents]> cd -
synapse:[/home/user]> exit
```

## Architecture Details

- **`main.cpp`**: Contains the core Readline execution loop, input tokenization, process forking, thread management, and built-in command handlers.
- **`ai_agent.cpp` / `ai_agent.h`**: Manages the HTTP payload and connection to the Gemini API using `libcurl` and processes the JSON response. Uses a system prompt to give the AI context about the shell's features.
