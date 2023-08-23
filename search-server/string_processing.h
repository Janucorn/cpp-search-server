#pragma once
#include <set>
#include <string>
#include <string_view>
#include <vector>

std::vector<std::string_view> SplitIntoWordsView(std::string_view text);

std::vector<std::string> SplitIntoWords(const std::string& text);

std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(std::string_view text);

std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const std::string& text);

template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string, std::less<>> non_empty_strings;
    for (std::string_view str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert({ str.data(), str.size() });
        }
    }
    return non_empty_strings;
}