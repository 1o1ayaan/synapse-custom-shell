#ifndef AI_AGENT_H
#define AI_AGENT_H

#include <string>

// Sends a prompt to the Gemini API and returns the plain text response.
std::string ask_gemini(const std::string& prompt);

#endif // AI_AGENT_H
