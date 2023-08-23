#pragma once
#include "document.h"
#include "string_processing.h"
#include "read_input_functions.h"
#include "concurrent_map.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <execution>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <utility>
#include <stdexcept>


const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);
    void PrintDocument(int document_id);
   
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    // Последовательная явная версия поиска топ-документов
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy& policy,
        std::string_view raw_query, DocumentPredicate document_predicate) const;

    // Последовательная явная версия поиска топ-документов
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy& policy,
        std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    // Параллельная версия поиска топ-документов
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy& policy, 
        std::string_view raw_query, DocumentPredicate document_predicate) const;

    // Параллельная версия поиска документов
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy& policy,
        std::string_view raw_query, DocumentStatus status = DocumentStatus::ACTUAL) const;

    // Возвращает количество документов
    int GetDocumentCount() const;

    std::set<int>::const_iterator begin() const;

    std::set<int>::const_iterator end() const;

    //Метод получения частот слов по id документа
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    //Метод удаления документов из поискового сервера
    void RemoveDocument(int document_id);

    ////Метод удаления документов из поискового сервера
    void RemoveDocument(const std::execution::sequenced_policy& policy, int document_id);

    // Параллельный метод удаления документов из поискового сервера
    void RemoveDocument(const std::execution::parallel_policy& policy, int document_id);

    using MyTuple = std::tuple<std::vector<std::string_view>, DocumentStatus>;
    MyTuple MatchDocument(const std::string_view raw_query, int document_id) const;

    MyTuple MatchDocument(const std::execution::sequenced_policy& policy, const std::string_view raw_query, int document_id) const;

    MyTuple MatchDocument(const std::execution::parallel_policy& policy, const std::string_view raw_query, int document_id) const;

private:
    // Данные документа:
    // rating - рейтинг;
    // status - статус;
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    // Список стоп-слов
    const std::set<std::string, std::less<>> stop_words_;

    // Список слов и их частот для каждого документа
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;

    // Список документов и частот для каждого слова
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;

    // Список рейтингов и статусов для каждого документа
    std::map<int, DocumentData> documents_;

    // Список id документов
    std::set<int> document_ids_;

    // Временное хранилище документа
    std::deque<std::string> storage_;

    bool IsStopWord(std::string_view word) const;

    static bool IsValidWord(std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(const std::string& text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    // Разбивает запрос на плюс- и минус-слова
    Query ParseQuery(std::string_view text, const bool remove_duplicates = true) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Query& query,
        DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy& policy,
        const Query& query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(
        MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    if (!std::all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        using namespace std::string_literals;
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
    DocumentPredicate document_predicate) const {
    auto query = ParseQuery(raw_query);

    auto matched_documents = FindAllDocuments(query, document_predicate);

    sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < std::numeric_limits<double>::epsilon()) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

// Последовательная явная версия поиска топ-документов
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy& policy,
    std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(raw_query, document_predicate);
}

// Параллельная версия поиска топ-документов
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy& policy,
    std::string_view raw_query, DocumentPredicate document_predicate) const {
    auto query = ParseQuery(raw_query);

    std::vector<Document> matched_documents;

    matched_documents = FindAllDocuments(policy, query, document_predicate);
    
    std::sort(policy, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < std::numeric_limits<double>::epsilon()) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    
    return matched_documents;
}

// Последовательная версия поиска документов
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query,
    DocumentPredicate document_predicate) const {

    std::map<int, double> document_to_relevance;
    for (std::string_view word : query.plus_words) {
        // Обрабатываем только те плюс-слова, что имеются в списке слов
        if (word_to_document_freqs_.count(word) != 0) {

            // Рассчитываем IDF частоту слова
            const double inverse_document_freq = SearchServer::ComputeWordInverseDocumentFreq(word);
            // для каждого слова имеющего id и частоту freq в документе
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
    }

    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) != 0) {

            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

// Параллельная версия поиска документов
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy& policy,
    const Query& query, DocumentPredicate document_predicate) const {

    ConcurrentMap<int, double> document_to_relevance(query.plus_words.size());
    std::set<int> id_of_minus_word;
    
	// Определяем список id документов, содержащих минус-слова
    for_each(query.minus_words.begin(), query.minus_words.end(),
        [&](const std::string_view word) {

            if (word_to_document_freqs_.count(word) != 0)
                for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                    id_of_minus_word.insert(document_id);
                }
        });

	// Обрабатываем слова из списка плюс-слов
    std::for_each(policy, query.plus_words.begin(), query.plus_words.end(),
        [&](const std::string_view& word) {
            if (word_to_document_freqs_.count(word) != 0) {

                const double inverse_document_freq = SearchServer::ComputeWordInverseDocumentFreq(word);
                
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                    // Игнорируем документы, которые содержат минус-слова
					if (id_of_minus_word.count(document_id)) { continue; }
                    
                    const auto& document_data = documents_.at(document_id);
                    if (document_predicate(document_id, document_data.status, document_data.rating)) {
                       document_to_relevance[document_id].ref_to_value  += term_freq * inverse_document_freq;
                    }
                }
            }
        });

	// Формируем итоговый список найденных документов
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings);