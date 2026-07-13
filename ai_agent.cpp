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

std::string ask_gemini(const std::string& prompt) {
    const char* api_key_env = std::getenv("GEMINI_API_KEY");
    if (!api_key_env) {
        return "Error: GEMINI_API_KEY environment variable is not set.";
    }

    std::string api_key(api_key_env);
    std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-3.1-flash-lite:generateContent?key=" + api_key;

    std::string system_context = "You are the AI assistant natively integrated into Synapse, a custom POSIX-compliant C++ Linux shell. The Synapse shell supports standard Linux commands via execvp, background multithreading using the & symbol at the end of a command, and custom built-ins including cd, cd - (toggle previous directory), exit, and ask (which triggers you). Answer questions helpfully and concisely.\n\n";
    std::string full_prompt = system_context + prompt;

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
