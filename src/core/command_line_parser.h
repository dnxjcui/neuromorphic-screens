#pragma once

#include <string>
#include <vector>
#include <iostream>

namespace neuromorphic {

/**
 * Reusable command-line argument parser for all neuromorphic applications
 */
class CommandLineParser {
private:
    std::vector<std::string> m_args;
    
public:
    CommandLineParser(int argc, char* argv[]) {
        for (int i = 1; i < argc; i++) {
            m_args.push_back(argv[i]);
        }
    }
    
    bool hasFlag(const std::string& flag) const {
        for (const auto& arg : m_args) {
            if (arg == flag) return true;
        }
        return false;
    }
    
    std::string getValue(const std::string& flag) const {
        for (size_t i = 0; i + 1 < m_args.size(); i++) {
            if (m_args[i] == flag) {
                return m_args[i + 1];
            }
        }
        return "";
    }
    
    int getIntValue(const std::string& flag, int defaultValue = 0) const {
        std::string value = getValue(flag);
        return value.empty() ? defaultValue : std::stoi(value);
    }
    
    float getFloatValue(const std::string& flag, float defaultValue = 0.0f) const {
        std::string value = getValue(flag);
        return value.empty() ? defaultValue : std::stof(value);
    }
    
    bool empty() const { return m_args.empty(); }
    size_t size() const { return m_args.size(); }
};

} // namespace neuromorphic