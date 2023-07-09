#include <algorithm>
#include <numeric>
#include <vector>

#include "search_server.h"
#include "document.h"

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(
        SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

void SearchServer::AddDocument(int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings) {
    //частота слов
    std::map<std::string, double> word_freqs;

    if (document_id < 0) {
        using namespace std::string_literals;
        throw std::invalid_argument("Invalid document_id"s);
    }
    if (documents_.count(document_id) > 0) {
        using namespace std::string_literals;
        throw std::invalid_argument("Invalid document_id. ID already exists"s);
    }
    const auto words = SplitIntoWordsNoStop(document);

    const double inv_word_count = 1.0 / words.size();
    for (const std::string& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        word_freqs[word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status, word_freqs });
    document_ids_.push_back(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::vector<int>::const_iterator SearchServer::begin() const {
    return document_ids_.begin();
}

std::vector<int>::const_iterator SearchServer::end() const {
    return document_ids_.end();
}

//Метод получения частот слов по id документа
const std::map<std::string, double>& SearchServer::GetWordFrequencies(int document_id) const {

    //Определяем наличие document_id среди документов
    auto it = std::lower_bound(SearchServer::begin(), SearchServer::end(), document_id);
    using namespace std::string_literals;

    if (it != SearchServer::end()) {
        return documents_.at(*it).word_freqs;
    }
    static std::map<std::string, double> word_frequencies;
    return word_frequencies;
}

//Метод удаления документов из поискового сервера
void SearchServer::RemoveDocument(int document_id) {
    if (documents_.count(document_id) == 0) {
        using namespace std::string_literals;
        throw std::invalid_argument("Invalid ID. ID is doesn't exist"s);
    }

    //Удаляем id документа из списка документов, в которых встречается слово
    //Определяем все слова, имеющиеся в документе
    for (const auto& [word, _] : documents_.at(document_id).word_freqs) {
        auto& id_freq = word_to_document_freqs_[word];
        //Удаляем документы
        id_freq.erase(document_id);
        //Если слово встречается в одном документе, то удаляем само слово
        if (id_freq.empty()) {
            word_to_document_freqs_.erase(word);
        }

    }
    //Удаляем документ из списка документов
    documents_.erase(document_id);
    //Удаляем id документа из списка id документов
    const auto it = std::find(document_ids_.begin(), document_ids_.end(), document_id);
    document_ids_.erase(it);
}


SearchServer::MatchedDocuments SearchServer::MatchDocument(const std::string& raw_query,
    int document_id) const {
    const auto query = ParseQuery(raw_query);

    std::vector<std::string> matched_words;
    for (const std::string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const std::string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return { matched_words, documents_.at(document_id).status };
}

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(const std::string& word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
    std::vector<std::string> words;
    for (const std::string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            using namespace std::string_literals;
            throw std::invalid_argument("Word "s + word + " is invalid"s);
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

SearchServer::QueryWord SearchServer::ParseQueryWord(const std::string& text) const {
    if (text.empty()) {
        using namespace std::string_literals;
        throw std::invalid_argument("Query word is empty"s);
    }
    std::string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        using namespace std::string_literals;
        throw std::invalid_argument("Query word "s + text + " is invalid"s);
    }
    return { word, is_minus, IsStopWord(word) };
}

SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
    SearchServer::Query result;
    for (const std::string& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            }
            else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}

// Existence required
double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
    return log(SearchServer::GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

