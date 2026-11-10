#pragma once

#include <cstdint>
#include <map>
#include <string>

// In-memory stand-in for the ESP32 key-value store; only what host-built code uses.
class Preferences {
public:
    uint32_t getUInt(const char* key, uint32_t fallback = 0) const {
        const auto found = numbers_.find(key);
        return found == numbers_.end() ? fallback : found->second;
    }
    size_t putUInt(const char* key, uint32_t value) {
        numbers_[key] = value;
        return sizeof(value);
    }

private:
    std::map<std::string, uint32_t> numbers_;
};
