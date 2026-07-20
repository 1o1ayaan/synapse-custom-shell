#ifndef AI_AGENT_H
#define AI_AGENT_H

#include <string>

// Sends a prompt to the Gemini API and returns the plain text response.
std::string ask_gemini(const std::string& prompt);

// Asks Gemini to explain a given shell command.
std::string ask_gemini_explain(const std::string& command);

// Asks Gemini to auto-suggest a fix for a failed shell command.
std::string ask_gemini_autofix(const std::string& failed_command);

// Autonomous agent command via Groq
std::string get_agent_command(const std::string& user_request);

#endif // AI_AGENT_H
