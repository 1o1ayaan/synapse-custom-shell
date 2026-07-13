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
#include <cctype>
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>
#include "ai_agent.h"

// Mutex to ensure clean printing between main shell loop and background threads
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Custom ls implementation
void custom_ls() {
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(".")) != nullptr) {
        while ((ent = readdir(dir)) != nullptr) {
            std::string name = ent->d_name;
            
            if (name.length() > 0 && name[0] == '.') {
                continue; // Skip hidden files
            }
            
            std::string color = "\033[0m"; // Default
            
            if (name == "Makefile") {
                color = "\033[1;35m";
            } else if (name == "synapse" || name == "synapse.exe") {
                color = "\033[1;31m";
            } else if (name.length() >= 4 && name.substr(name.length() - 4) == ".cpp") {
                color = "\033[1;32m";
            } else if ((name.length() >= 2 && name.substr(name.length() - 2) == ".h") || 
                       (name.length() >= 4 && name.substr(name.length() - 4) == ".hpp")) {
                color = "\033[1;36m";
            } else if (name.length() >= 2 && name.substr(name.length() - 2) == ".o") {
                color = "\033[2;37m";
            }
            
            std::cout << color << name << "\033[0m  ";
        }
        std::cout << std::endl;
        closedir(dir);
    } else {
        std::cerr << "synapse: ls: could not open directory" << std::endl;
    }
}

// Splits the input string into a vector of tokens
std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current_token;
    bool in_quotes = false;

    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];

        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (std::isspace(static_cast<unsigned char>(c)) && !in_quotes) {
            if (!current_token.empty()) {
                tokens.push_back(current_token);
                current_token.clear();
            }
        } else {
            current_token += c;
        }
    }

    if (!current_token.empty()) {
        tokens.push_back(current_token);
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
        pthread_mutex_lock(&print_mutex);
        char cwd[1024];
        std::string current_pwd = "";
        if (getcwd(cwd, sizeof(cwd)) != nullptr) {
            current_pwd = cwd;
        }
        std::string prompt = "\001\033[1;32m\002synapse:\001\033[1;34m\002[" + current_pwd + "]\001\033[0m\002> ";
        pthread_mutex_unlock(&print_mutex);
        
        char* line = readline(prompt.c_str());
        
        if (!line) {
            // Handle EOF (e.g., Ctrl+D) gracefully
            std::cout << std::endl;
            break;
        }

        input = line;
        
        if (!input.empty()) {
            add_history(line);
        }
        
        free(line);

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

        // Built-in command: ls
        if (tokens[0] == "ls") {
            custom_ls();
            continue;
        }

        // Built-in command: ask
        if (tokens[0] == "ask") {
            if (tokens.size() < 2) {
                std::cerr << "synapse: ask: missing prompt" << std::endl;
                continue;
            }
            std::string prompt;
            for (size_t i = 1; i < tokens.size(); ++i) {
                prompt += tokens[i] + (i == tokens.size() - 1 ? "" : " ");
            }
            
            // Run on a background thread
            std::string* prompt_ptr = new std::string(prompt);
            pthread_t ai_thread;
            if (pthread_create(&ai_thread, nullptr, [](void* arg) -> void* {
                std::string* p = static_cast<std::string*>(arg);
                std::string response = ask_gemini(*p);
                delete p;
                
                pthread_mutex_lock(&print_mutex);
                char cwd[1024];
                std::string current_pwd = "";
                if (getcwd(cwd, sizeof(cwd)) != nullptr) {
                    current_pwd = cwd;
                }
                std::cout << "\n\n[AI] " << response << "\n\033[1;32msynapse:\033[1;34m[" << current_pwd << "]\033[0m> " << std::flush;
                pthread_mutex_unlock(&print_mutex);
                
                return nullptr;
            }, prompt_ptr) != 0) {
                std::cerr << "synapse: failed to create AI thread" << std::endl;
                delete prompt_ptr;
            } else {
                pthread_detach(ai_thread);
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
