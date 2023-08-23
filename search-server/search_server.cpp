#include <algorithm>
#include <deque>
#include <execution>
#include <numeric>
#include <vector>
#include <string>
#include <string_view>
#include <iostream>

#include "search_server.h"
#include "document.h"
#include "log_duration.h"

using namespace std::string_literals;
using namespace std::string_view_literals;

SearchServer::SearchServer(std::string_view stop_words_text)
    : SearchServer(
        SplitIntoWordsView(stop_words_text))  // Invoke delegating constructor from string container
{
}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status,
    const std::vector<int>& ratings) {

    if (document_id < 0) {
        throw std::invalid_argument("Invalid document_id"s);
    }
    if (documents_.count(document_id) > 0) {
        throw std::invalid_argument("Invalid document_id. ID already exists"s);
    }
    storage_.emplace_back(document);

    auto words = SplitIntoWordsNoStop(storage_.back());

    const double inv_word_count = 1.0 / words.size();
    for (std::string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.emplace(document_id);


}

void SearchServer::PrintDocument(int document_id) {
    if (document_to_word_freqs_.count(document_id)) {
        std::cout << "Size: "s << document_to_word_freqs_[document_id].size() << std::endl;
        std::cout << "Words: "s;
        for (auto [word, _] : document_to_word_freqs_[document_id]) {
            std::cout << word << ' ';
        }std::cout << std::endl;
    }
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

// Последовательная явная версия поиска документов
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy& policy,
    std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, status);
}

// Параллельная версия поиска документов
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy& policy,
    std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy,
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

// Возвращает количество документов
int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::set<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

//Метод получения частот слов по id документа
const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {

    if (document_ids_.count(document_id) != 0) {
        return document_to_word_freqs_.at(document_id);
    }
    // Возвращает пустой список, если не нашелся документ
    static std::map<std::string_view, double> word_frequencies;
    return word_frequencies;
}

//Метод удаления документов из поискового сервера
void SearchServer::RemoveDocument(int document_id) {
    if (document_to_word_freqs_.count(document_id) == 0) {
        throw std::invalid_argument("Invalid ID. ID is doesn't exist"s);
    }

    // Удаляем документы из списка документов и частот для каждого слово
    // Определяем все слова, имеющиеся в документе
    for (const auto& [word, _] : document_to_word_freqs_[document_id]) {

        // Удаляем документы из списка документов и частот для каждого слова
        word_to_document_freqs_[word].erase(document_id);
    }
    // Удаляем документ из списка документов
    documents_.erase(document_id);

    // Удаляем документ из списка слов и частот для всех документов
    document_to_word_freqs_.erase(document_id);

    // Удаляем id документа из списка id документов
    document_ids_.erase(document_id);

}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy& policy, int document_id) {
    SearchServer::RemoveDocument(document_id);
}

// Параллельный метод удаления документов из поискового сервера
void SearchServer::RemoveDocument(const std::execution::parallel_policy& policy, int document_id) {
    if (document_to_word_freqs_.count(document_id) == 0) {
        throw std::invalid_argument("Invalid ID. ID is doesn't exist"s);
    }
    const auto& curr_map = document_to_word_freqs_[document_id];
    std::vector<std::string_view> words_to_remove_ptr(curr_map.size());

    // Определяем список указателей на слова, которые содержатся в 
    // документе с document_id 
    std::transform(policy,
        curr_map.begin(),
        curr_map.end(),
        words_to_remove_ptr.begin(),
        [](auto& word_freq) { return word_freq.first; }
    );

    // Удаляем документ из списка документов для каждого слова
    std::for_each(policy,
        words_to_remove_ptr.cbegin(), words_to_remove_ptr.cend(),
        [this, document_id](std::string_view word_ptr) {
            word_to_document_freqs_[word_ptr].erase(document_id);
        }
    );

    //Удаляем документ из списка документов
    documents_.erase(document_id);

    // Удаляем документ из списка слов и частот для всех документов
    document_to_word_freqs_.erase(document_id);

    // Удаляем id документа из списка id документов
    document_ids_.erase(document_id);
}

// Последовательная версия поиска совпадающих слов документа
SearchServer::MyTuple SearchServer::MatchDocument(std::string_view raw_query,
    int document_id) const {
    // Если несуществующий document_id, выбрасывается исключение std::out_of_range
    if (documents_.count(document_id) == 0) {
        throw std::out_of_range("Invalid document id. Document id is doesn't exist"s);
    }
    // Если неверный запрос, то выбросится исключение std::invalid_argument
    const auto& query = ParseQuery(raw_query, true);

    const auto& curr_map = document_to_word_freqs_.at(document_id);

    std::vector<std::string_view> matched_words;
    matched_words.reserve(query.plus_words.size());

    // Возвращаем пустой результат при наличии минус-слова
    for (std::string_view word : query.minus_words) {
        if (curr_map.count(word) != 0) {
            matched_words.clear();
            return { matched_words, documents_.at(document_id).status };
        }
    }

    // Добавляем только те плюс-слова, что имеются в документе
    for (std::string_view word : query.plus_words) {
        if (curr_map.count(word) != 0) {
            matched_words.push_back(word);
        }
    }

    return { matched_words, documents_.at(document_id).status };
}

SearchServer::MyTuple SearchServer::MatchDocument(const std::execution::sequenced_policy& policy, std::string_view raw_query, int document_id) const {
    return MatchDocument(raw_query, document_id);
}

// Параллельная версия поиска совпадающих слов документа
SearchServer::MyTuple SearchServer::MatchDocument(const std::execution::parallel_policy& policy, std::string_view raw_query, int document_id) const {

    // Если несуществующий document_id, выбрасывается исключение std::out_of_range
    if (document_to_word_freqs_.count(document_id) == 0) {
        throw std::out_of_range("Invalid document id. Document id is doesn't exist"s);
    }
    // Если неверный запрос, то выбросится исключение std::invalid_argument
    const auto& query = ParseQuery(raw_query, false);

    const auto& curr_map = document_to_word_freqs_.at(document_id);

    // Проверяем документ на наличеие минус-слов
    if (std::any_of(policy,
        query.minus_words.begin(), query.minus_words.end(),
        [&curr_map](std::string_view word_ref) {
            return  (curr_map.count(word_ref) != 0); })) {

        // Возвращаем пустой вектор слов при наличии минус-слова
        std::vector<std::string_view> matched_words;
        return { matched_words, documents_.at(document_id).status };
    }

    std::vector<std::string_view> matched_words;
    matched_words.reserve(query.plus_words.size());

    // Добавляем в результат только те плюс-слова, что имеются в документе
    auto end = std::copy_if(policy,
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        [&curr_map](std::string_view word_ref) {

            return (curr_map.count(word_ref) != 0);
        });

    // Удаляем лишние элементы
    matched_words.erase(end, matched_words.end());
    // Сортируем и удаляем дубликаты
    std::sort(matched_words.begin(), matched_words.end());
    end = std::unique(matched_words.begin(), end);
    matched_words.erase(end, matched_words.end());

    return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(std::string_view word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWordsView(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word ["s + std::string{word} + "] is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);

    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string_view word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    // Не обрабатываем пустые слова, а также слова, состоящие из двойного минуса или
    // имеющие спецсимволы
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw std::invalid_argument("Query word ["s + std::string{text} + "] is invalid"s);
    }
    return { word, is_minus, IsStopWord(word) };
}

// Разбивает запрос на плюс- и минус-слова
// bool remove_duplicates используется для однопоточной версии
SearchServer::Query SearchServer::ParseQuery(std::string_view text, const bool remove_duplicates) const {
    SearchServer::Query result;
    auto words = SplitIntoWordsView(text);


    for (std::string_view word : words) {
        // Если слово некорректное, то будет выброшено исключение
        auto query_word = ParseQueryWord(word);

        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.push_back(query_word.data);
            }
            else {
                result.plus_words.push_back(query_word.data);
            }
        }
    }

    // Сортируем и удаляем дубликаты слов
    if (remove_duplicates) {
        std::sort(result.minus_words.begin(), result.minus_words.end());
        std::sort(result.plus_words.begin(), result.plus_words.end());
        result.minus_words.erase(std::unique(result.minus_words.begin(), result.minus_words.end()), result.minus_words.end());
        result.plus_words.erase(std::unique(result.plus_words.begin(), result.plus_words.end()), result.plus_words.end());
    }

    return result;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return log(SearchServer::GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings) {
    search_server.AddDocument(document_id, document, status, ratings);
}