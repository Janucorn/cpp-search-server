#include "string_processing.h"

std::vector<std::string> SplitIntoWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

std::vector<std::string_view> SplitIntoWordsView(std::string_view str) {
    std::vector<std::string_view> result;
    // Удаляем начало до первого непробельного символа
    str.remove_prefix(std::min(str.size(), str.find_first_not_of(" ")));

    while (!str.empty()) {
        std::string_view str_to_result;
        // Определение первого пробельного символа
        auto space = str.find(' ');

        if (space == str.npos) {
            result.push_back(str);
            break;
        }
        else {
            // Определение слова
            str_to_result = str.substr(0, space);
        }
        // Определяем позицию первого непробельного символа после найденного первого пробела
        auto new_word = str.find_first_not_of(" ", space);
        // Удаляем начало строки до нового непробельного символа
        str.remove_prefix(std::min(str.size(), new_word));

        result.push_back(str_to_result);
    }
    return result;
}

std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(std::string_view text) {
    std::set<std::string, std::less<>> non_empty_strings;
    for (std::string_view word : SplitIntoWordsView(text)) {
        if (!word.empty()) {
            non_empty_strings.insert({ word.data(), word.size() });
        }
    }
    return non_empty_strings;
}

std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const std::string& text) {
    
    return MakeUniqueNonEmptyStrings(static_cast<std::string_view>(text));
}
