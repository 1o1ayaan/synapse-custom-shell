#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <pthread.h>

// Splits the input string into a vector of tokens
std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::istringstream stream(input);
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// Executes a standard external command using fork and execvp
void execute_command(const std::vector<std::string>& tokens) {
    std::vector<char*> args;
    for (const auto& token : tokens) {
        args.push_back(const_cast<char*>(token.c_str()));
    }
    args.push_back(nullptr); // execvp requires a null-terminated array

    pid_t pid = fork();

    if (pid < 0) {
        std::cerr << "synapse: fork failed: " << strerror(errno) << std::endl;
    } else if (pid == 0) {
        // Child process
        if (execvp(args[0], args.data()) == -1) {
            std::cerr << "synapse: " << args[0] << ": command not found" << std::endl;
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process waiting for child
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            std::cerr << "synapse: waitpid failed: " << strerror(errno) << std::endl;
        }
    }
}

// Structure to pass arguments to the background thread
struct JobArgs {
    std::vector<std::string> tokens;
};

// Thread routine for executing a background job
void* background_job(void* arg) {
    JobArgs* job_args = static_cast<JobArgs*>(arg);
    execute_command(job_args->tokens);
    delete job_args;
    return nullptr;
}

int main() {
    std::string input;
    std::string oldpwd = "";
    
    // Continuous read-parse-execute loop
    while (true) {
        std::cout << "synapse> ";
        
        // Read standard input
        if (!std::getline(std::cin, input)) {
            // Handle EOF (e.g., Ctrl+D) gracefully
            std::cout << std::endl;
            break;
        }

        if (input.empty()) {
            continue;
        }

        // Split input into tokens
        std::vector<std::string> tokens = tokenize(input);
        if (tokens.empty()) {
            continue;
        }

        // Check for background execution ('&')
        bool is_background = false;
        if (tokens.back() == "&") {
            is_background = true;
            tokens.pop_back();
        }

        // After removing '&', we might have no tokens left
        if (tokens.empty()) {
            continue;
        }

        // Built-in command: exit
        if (tokens[0] == "exit") {
            break;
        }

        // Built-in command: cd
        if (tokens[0] == "cd") {
            std::string target_path;
            bool print_path = false;

            if (tokens.size() > 1) {
                if (tokens[1] == "-") {
                    if (oldpwd.empty()) {
                        std::cerr << "synapse: cd: OLDPWD not set" << std::endl;
                        continue;
                    }
                    target_path = oldpwd;
                    print_path = true;
                } else {
                    target_path = tokens[1];
                }
            } else {
                const char* home = getenv("HOME");
                if (home == nullptr) {
                    std::cerr << "synapse: cd: HOME not set" << std::endl;
                    continue;
                }
                target_path = home;
            }

            // Get current directory before changing
            char cwd[1024];
            std::string current_pwd = "";
            if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                current_pwd = cwd;
            }

            // Perform the directory change
            if (chdir(target_path.c_str()) != 0) {
                std::cerr << "synapse: cd: " << target_path << ": " << strerror(errno) << std::endl;
            } else {
                // Successfully changed directory
                oldpwd = current_pwd;
                if (print_path) {
                    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                        std::cout << cwd << std::endl;
                    }
                }
            }
            continue;
        }

        // Execute command
        if (is_background) {
            // Run in a new pthread without blocking the main loop
            JobArgs* args = new JobArgs{tokens};
            pthread_t thread;
            if (pthread_create(&thread, nullptr, background_job, args) != 0) {
                std::cerr << "synapse: failed to create background thread" << std::endl;
                delete args;
            } else {
                pthread_detach(thread);
            }
        } else {
            // Run synchronously in the current thread
            execute_command(tokens);
        }
    }

    return 0;
}
