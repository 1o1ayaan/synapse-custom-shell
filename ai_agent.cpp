#include "ai_agent.h"
#include "json.hpp"
#include <iostream>
#include <cstdlib>
#include <curl/curl.h>

using json = nlohmann::json;

// Callback function to write the received data from libcurl into a std::string
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total_size);
    return total_size;
}

std::string send_gemini_request(const std::string& full_prompt) {
    const char* api_key_env = std::getenv("GEMINI_API_KEY");
    if (!api_key_env) {
        return "Error: GEMINI_API_KEY environment variable is not set.";
    }

    std::string api_key(api_key_env);
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-3.1-flash-lite:generateContent?key=" + api_key;

    // Format the prompt into a Gemini-compatible JSON payload
    json payload = {
        {"contents", {
            {
                {"parts", {
                    {{"text", full_prompt}}
                }}
            }
        }}
    };

    std::string payload_str = payload.dump();
    std::string response_string;

    CURL* curl = curl_easy_init();
    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        // Perform the HTTP POST request
        CURLcode res = curl_easy_perform(curl);
        
        // Clean up
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            return "Error: libcurl request failed: " + std::string(curl_easy_strerror(res));
        }
    } else {
        return "Error: Failed to initialize libcurl.";
    }

    // Parse the JSON response
    try {
        json response_json = json::parse(response_string);
        
        if (response_json.contains("error")) {
            return "API Error: " + response_json["error"]["message"].get<std::string>();
        }

        if (response_json.contains("candidates") && response_json["candidates"].is_array() && !response_json["candidates"].empty()) {
            auto& candidate = response_json["candidates"][0];
            if (candidate.contains("content") && candidate["content"].contains("parts")) {
                auto& parts = candidate["content"]["parts"];
                if (parts.is_array() && !parts.empty() && parts[0].contains("text")) {
                    return parts[0]["text"].get<std::string>();
                }
            }
        }
        
        return "Error: Unexpected response format from Gemini API.";
    } catch (const json::exception& e) {
        return "Error parsing JSON response: " + std::string(e.what());
    }
}

std::string ask_gemini(const std::string& prompt) {
    std::string system_context = "You are the AI assistant natively integrated into Synapse, a custom POSIX-compliant C++ Linux shell. The Synapse shell supports standard Linux commands via execvp, background execution using the & symbol, and custom built-ins including cd, cd - (toggle previous directory), exit, jobs, and ask (which triggers you). Answer questions helpfully and concisely.\n\n";
    return send_gemini_request(system_context + prompt);
}

std::string ask_gemini_explain(const std::string& command) {
    std::string system_context = "You are a Linux command expert. Explain the following shell command concisely. Break down what the command does and what any flags/arguments mean. Do not wrap the explanation in markdown code blocks unless giving an example. Command to explain: ";
    return send_gemini_request(system_context + command);
}

std::string ask_gemini_autofix(const std::string& failed_command) {
    std::string system_context = "The following Linux command just failed to execute or returned a non-zero exit code. Suggest what the user might have meant or how to fix it in 1-2 short sentences. Make it sound helpful and brief. Failed command: ";
    return send_gemini_request(system_context + failed_command);
}

std::string get_agent_command(const std::string& user_request) {
    const char* api_key_env = std::getenv("GROQ_API_KEY");
    if (!api_key_env) {
        return "echo 'Error: GROQ_API_KEY environment variable is not set.'";
    }

    std::string api_key(api_key_env);
    std::string url = "https://api.groq.com/openai/v1/chat/completions";

    json payload = {
        {"model", "llama-3.3-70b-versatile"},
        {"messages", {
            {
                {"role", "system"},
                {"content", "You are an expert autonomous Linux terminal agent executing commands inside a custom C++ shell via /bin/sh -c.\n\n"
                            "CRITICAL PORTABILITY & SYNTAX RULES:\n\n"
                            "When asked to write code or scripts to a file, NEVER use printf or echo, as they fail on % operators, $ variables, and nested quotes. You MUST ALWAYS use the cat << 'EOF' > filename pattern. Ensure the EOF delimiter is enclosed in single quotes to prevent premature variable expansion.\n\n"
                            "When asked to compile and run code, or perform a sequence of actions, chain the commands together safely using && (e.g., cat << 'EOF' > main.cpp ... EOF && g++ main.cpp -o main && ./main).\n\n"
                            "When asked to navigate directories, output ONLY the cd command. Always wrap directory names containing spaces in double quotes (e.g., cd \"synapse shell\").\n\n"
                            "OUTPUT FORMAT:\n"
                            "Output ONLY the raw terminal command string. No markdown, no ticks, no explanation."}
            },
            {
                {"role", "user"},
                {"content", user_request}
            }
        }}
    };

    std::string payload_str = payload.dump();
    std::string response_string;

    CURL* curl = curl_easy_init();
    if (curl) {
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        std::string auth_header = "Authorization: Bearer " + api_key;
        headers = curl_slist_append(headers, auth_header.c_str());

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);

        CURLcode res = curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            return "echo 'Error: libcurl request failed: " + std::string(curl_easy_strerror(res)) + "'";
        }
    } else {
        return "echo 'Error: Failed to initialize libcurl.'";
    }

    try {
        json response_json = json::parse(response_string);
        
        if (response_json.contains("error")) {
            return "echo 'API Error: " + response_json["error"]["message"].get<std::string>() + "'";
        }

        if (response_json.contains("choices") && response_json["choices"].is_array() && !response_json["choices"].empty()) {
            auto& choice = response_json["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content")) {
                std::string command = choice["message"]["content"].get<std::string>();
                
                // Trim trailing/leading whitespace/newlines that the LLM might add
                command.erase(0, command.find_first_not_of(" \t\r\n`"));
                command.erase(command.find_last_not_of(" \t\r\n`") + 1);
                
                return command;
            }
        }
        
        return "echo 'Error: Unexpected response format from Groq API.'";
    } catch (const json::exception& e) {
        return "echo 'Error parsing JSON response: " + std::string(e.what()) + "'";
    }
}
