#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

namespace minidb::storage::detail {

inline char ascii_lower(char character) noexcept {
    if (character >= 'A' && character <= 'Z') {
        return static_cast<char>(character + ('a' - 'A'));
    }
    return character;
}

inline std::string normalized_name(std::string_view name);

inline bool is_identifier(std::string_view name) {
    if (name.empty()) {
        return false;
    }

    const auto is_letter = [](unsigned char character) {
        return (character >= 'A' && character <= 'Z') ||
               (character >= 'a' && character <= 'z');
    };
    const auto is_digit = [](unsigned char character) {
        return character >= '0' && character <= '9';
    };

    const auto first = static_cast<unsigned char>(name.front());
    if (!is_letter(first) && first != '_') {
        return false;
    }

    const bool valid_syntax =
        std::all_of(name.begin() + 1, name.end(), [&](char character) {
            const auto value = static_cast<unsigned char>(character);
            return is_letter(value) || is_digit(value) || value == '_';
        });
    if (!valid_syntax) {
        return false;
    }

    const std::string normalized = normalized_name(name);
    if (normalized == "con" || normalized == "prn" || normalized == "aux" ||
        normalized == "nul") {
        return false;
    }
    if (normalized.size() == 4 &&
        (normalized.compare(0, 3, "com") == 0 ||
         normalized.compare(0, 3, "lpt") == 0) &&
        normalized[3] >= '1' && normalized[3] <= '9') {
        return false;
    }
    return true;
}

inline std::string normalized_name(std::string_view name) {
    std::string normalized;
    normalized.reserve(name.size());
    for (const unsigned char character : name) {
        normalized.push_back(ascii_lower(static_cast<char>(character)));
    }
    return normalized;
}

inline bool equal_ascii_case_insensitive(std::string_view left,
                                         std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        const auto left_character =
            ascii_lower(left[index]);
        const auto right_character =
            ascii_lower(right[index]);
        if (left_character != right_character) {
            return false;
        }
    }
    return true;
}

}  // namespace minidb::storage::detail
