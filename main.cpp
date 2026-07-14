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
#include <fcntl.h>
#include <map>
#include <signal.h>
#include "ai_agent.h"

// Mutex to ensure clean printing between main shell loop and background threads
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

// Job tracking
std::map<pid_t, std::string> background_jobs;
pthread_mutex_t jobs_mutex = PTHREAD_MUTEX_INITIALIZER;

// Global exit status for dynamic prompt color
int last_exit_status = 0;

// Signal handler to reap zombie processes
void sigchld_handler(int sig) {
    (void)sig; // unused
    int saved_errno = errno;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        pthread_mutex_lock(&jobs_mutex);
        if (background_jobs.find(pid) != background_jobs.end()) {
            background_jobs.erase(pid);
        }
        pthread_mutex_unlock(&jobs_mutex);
    }
    errno = saved_errno;
}

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

// Structure to represent a single command in a pipeline
struct Command {
    std::vector<std::string> args;
    std::string input_file;
    std::string output_file;
    bool append_output = false;
};

// Splits the input string into a vector of tokens, correctly parsing quotes and special characters
std::vector<std::string> tokenize(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current_token;
    bool in_quotes = false;

    auto flush_token = [&]() {
        if (!current_token.empty()) {
            tokens.push_back(current_token);
            current_token.clear();
        }
    };

    for (size_t i = 0; i < input.length(); ++i) {
        char c = input[i];

        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (!in_quotes && (c == '|' || c == '<' || c == '>')) {
            flush_token();
            if (c == '>' && i + 1 < input.length() && input[i+1] == '>') {
                tokens.push_back(">>");
                i++;
            } else {
                tokens.push_back(std::string(1, c));
            }
        } else if (std::isspace(static_cast<unsigned char>(c)) && !in_quotes) {
            flush_token();
        } else {
            current_token += c;
        }
    }
    flush_token();
    return tokens;
}

// Parse tokens into a pipeline of commands
std::vector<Command> parse_pipeline(const std::vector<std::string>& tokens) {
    std::vector<Command> pipeline;
    Command current_cmd;
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "|") {
            if (!current_cmd.args.empty()) {
                pipeline.push_back(current_cmd);
                current_cmd = Command();
            }
        } else if (tokens[i] == "<" && i + 1 < tokens.size()) {
            current_cmd.input_file = tokens[++i];
        } else if (tokens[i] == ">" && i + 1 < tokens.size()) {
            current_cmd.output_file = tokens[++i];
            current_cmd.append_output = false;
        } else if (tokens[i] == ">>" && i + 1 < tokens.size()) {
            current_cmd.output_file = tokens[++i];
            current_cmd.append_output = true;
        } else {
            current_cmd.args.push_back(tokens[i]);
        }
    }
    if (!current_cmd.args.empty()) {
        pipeline.push_back(current_cmd);
    }
    return pipeline;
}

// Executes a pipeline of commands
void execute_pipeline(const std::vector<Command>& pipeline, bool is_background, const std::string& original_cmd) {
    int num_cmds = pipeline.size();
    if (num_cmds == 0) return;

    int pipefds[2 * (num_cmds - 1)];
    
    // Create pipes
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipefds + i * 2) < 0) {
            perror("synapse: pipe");
            return;
        }
    }
    
    std::vector<pid_t> children;
    
    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();
        
        if (pid == 0) { // Child process
            // Restore default signal handling for child (so it can be killed with Ctrl+C)
            signal(SIGINT, SIG_DFL);
            
            // Setup input pipe
            if (i > 0) {
                if (dup2(pipefds[(i - 1) * 2], STDIN_FILENO) < 0) { perror("dup2 stdin"); exit(1); }
            }
            // Setup output pipe
            if (i < num_cmds - 1) {
                if (dup2(pipefds[i * 2 + 1], STDOUT_FILENO) < 0) { perror("dup2 stdout"); exit(1); }
            }
            
            // Setup file redirection
            if (!pipeline[i].input_file.empty()) {
                int fd = open(pipeline[i].input_file.c_str(), O_RDONLY);
                if (fd < 0) { std::cerr << "synapse: " << pipeline[i].input_file << ": " << strerror(errno) << "\n"; exit(1); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            if (!pipeline[i].output_file.empty()) {
                int flags = O_WRONLY | O_CREAT | (pipeline[i].append_output ? O_APPEND : O_TRUNC);
                int fd = open(pipeline[i].output_file.c_str(), flags, 0644);
                if (fd < 0) { std::cerr << "synapse: " << pipeline[i].output_file << ": " << strerror(errno) << "\n"; exit(1); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            
            // Close all pipes in child
            for (int j = 0; j < 2 * (num_cmds - 1); j++) {
                close(pipefds[j]);
            }
            
            // Prepare arguments
            std::vector<char*> args;
            for (const auto& arg : pipeline[i].args) {
                args.push_back(const_cast<char*>(arg.c_str()));
            }
            args.push_back(nullptr);
            
            execvp(args[0], args.data());
            std::cerr << "synapse: " << args[0] << ": command not found\n";
            exit(127);
        } else if (pid < 0) {
            perror("synapse: fork");
        } else {
            children.push_back(pid); // Parent process
        }
    }
    
    // Close all pipes in parent
    for (int i = 0; i < 2 * (num_cmds - 1); i++) {
        close(pipefds[i]);
    }
    
    if (is_background) {
        pthread_mutex_lock(&jobs_mutex);
        background_jobs[children.back()] = original_cmd;
        pthread_mutex_unlock(&jobs_mutex);
        std::cout << "[Background job started] PID: " << children.back() << std::endl;
    } else {
        int status;
        for (size_t i = 0; i < children.size(); i++) {
            waitpid(children[i], &status, 0);
            
            // Auto-fix feature on failure for the last command in pipeline
            if (i == children.size() - 1 && WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                int exit_code = WEXITSTATUS(status);
                
                std::string failed_cmd;
                for (const auto& a : pipeline[i].args) failed_cmd += a + " ";
                
                // Spin up a thread to fetch the auto-fix async so it doesn't block
                std::string* prompt_ptr = new std::string(failed_cmd + " (Exit code: " + std::to_string(exit_code) + ")");
                pthread_t ai_thread;
                if (pthread_create(&ai_thread, nullptr, [](void* arg) -> void* {
                    std::string* p = static_cast<std::string*>(arg);
                    std::string response = ask_gemini_autofix(*p);
                    delete p;
                    
                    pthread_mutex_lock(&print_mutex);
                    std::cout << "\n\033[1;33m[AI Auto-Fix]\033[0m " << response << "\nsynapse> " << std::flush;
                    pthread_mutex_unlock(&print_mutex);
                    return nullptr;
                }, prompt_ptr) != 0) {
                    delete prompt_ptr;
                } else {
                    pthread_detach(ai_thread);
                }
            }
        }
        // Update last exit status for the prompt color (using the last command in pipeline)
        if (children.size() > 0) {
            if (WIFEXITED(status)) {
                last_exit_status = WEXITSTATUS(status);
            } else {
                last_exit_status = 1; // Mark as failed if it was killed by a signal etc.
            }
        }
    }
}

int main() {
    // Ignore SIGINT in the shell, so Ctrl+C doesn't kill it
    signal(SIGINT, SIG_IGN);

    // Register SIGCHLD handler to reap zombies
    struct sigaction sa_chld;
    sa_chld.sa_handler = sigchld_handler;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa_chld, nullptr);

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
        
        std::string prompt_color = (last_exit_status == 0) ? "\033[1;32m" : "\033[1;31m";
        std::string prompt = "\001" + prompt_color + "\002synapse:\001\033[1;34m\002[" + current_pwd + "]\001\033[0m\002> ";
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

            char cur_dir[1024];
            std::string current_pwd_save = "";
            if (getcwd(cur_dir, sizeof(cur_dir)) != nullptr) {
                current_pwd_save = cur_dir;
            }

            if (chdir(target_path.c_str()) != 0) {
                std::cerr << "synapse: cd: " << target_path << ": " << strerror(errno) << std::endl;
                last_exit_status = 1;
            } else {
                oldpwd = current_pwd_save;
                if (print_path && getcwd(cur_dir, sizeof(cur_dir)) != nullptr) {
                    std::cout << cur_dir << std::endl;
                }
                last_exit_status = 0;
            }
            continue;
        }

        // Built-in command: ls
        if (tokens[0] == "ls" && tokens.size() == 1) {
            // Only use custom_ls for bare "ls", otherwise pass to real ls (for e.g. ls -la)
            custom_ls();
            continue;
        }

        // Built-in command: jobs
        if (tokens[0] == "jobs") {
            pthread_mutex_lock(&jobs_mutex);
            if (background_jobs.empty()) {
                std::cout << "No background jobs running." << std::endl;
            } else {
                for (const auto& job : background_jobs) {
                    std::cout << "[" << job.first << "] Running\t" << job.second << std::endl;
                }
            }
            pthread_mutex_unlock(&jobs_mutex);
            continue;
        }

        // Built-in command: explain
        if (tokens[0] == "explain") {
            if (tokens.size() < 2) {
                std::cerr << "synapse: explain: missing command to explain" << std::endl;
                continue;
            }
            std::string prompt;
            for (size_t i = 1; i < tokens.size(); ++i) {
                prompt += tokens[i] + (i == tokens.size() - 1 ? "" : " ");
            }
            
            std::string* prompt_ptr = new std::string(prompt);
            pthread_t ai_thread;
            if (pthread_create(&ai_thread, nullptr, [](void* arg) -> void* {
                std::string* p = static_cast<std::string*>(arg);
                std::string response = ask_gemini_explain(*p);
                delete p;
                
                pthread_mutex_lock(&print_mutex);
                std::cout << "\n\033[1;36m[AI Explanation]\033[0m\n" << response << "\nsynapse> " << std::flush;
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
            
            std::string* prompt_ptr = new std::string(prompt);
            pthread_t ai_thread;
            if (pthread_create(&ai_thread, nullptr, [](void* arg) -> void* {
                std::string* p = static_cast<std::string*>(arg);
                std::string response = ask_gemini(*p);
                delete p;
                
                pthread_mutex_lock(&print_mutex);
                std::cout << "\n\n\033[1;35m[AI]\033[0m " << response << "\nsynapse> " << std::flush;
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

        // Parse into pipeline and execute
        std::vector<Command> pipeline = parse_pipeline(tokens);
        execute_pipeline(pipeline, is_background, input);
    }

    return 0;
}
