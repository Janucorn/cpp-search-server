#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
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

struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    // Defines an invalid document id
   // You can refer to this constant as SearchServer::INVALID_DOCUMENT_ID
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        for (const auto& word : stop_words_) {
            if (!IsValidWord(word)) {
                throw invalid_argument("invalid symbols"s);
            }
        }
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(
            SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
        for (const auto& word : stop_words_) {
            if (!IsValidWord(word)) {
                throw invalid_argument("invalid symbols"s);
            }
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {
        if (document_id < 0) {
            throw invalid_argument("id is negative"s);
        }
        if (documents_.count(document_id) != 0) {
            throw invalid_argument("id is already exist"s);
        }

        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            if (!IsValidWord(word)) {
                throw invalid_argument("invalid symbols"s);
            }
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query,
        DocumentPredicate document_predicate) const {
        if (!ParseQuery(raw_query).has_value()) {
            throw invalid_argument("invalid symbols"s);
        }
        Query query = ParseQuery(raw_query).value();
        if (!query.minus_words.empty()) {
            for (const string& word : query.minus_words) {
                if (!IsValidWord(word)) {
                    throw invalid_argument("invalid symbols"s);
                }
            }
        }
        if (!query.plus_words.empty()) {
            for (const string& word : query.plus_words) {
                if (!IsValidWord(word)) {
                    throw invalid_argument("invalid symbols"s);
                }
            }
        }
        vector<Document> matched_document = FindAllDocuments(query, document_predicate);
        sort(matched_document.begin(), matched_document.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_document.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_document.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_document;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        /*if (!FindTopDocuments(
            raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
                return document_status == status;
            }).has_value()) {
            return nullopt;
        }*/try {
                return FindTopDocuments(
                    raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
                        return document_status == status;
                    });
            }
            catch (const invalid_argument& e) {
                throw e;
            }
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        /*if (!FindTopDocuments(raw_query, DocumentStatus::ACTUAL).has_value()) {
            return nullopt;
        }*/
        try {
            return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
        }
        catch (invalid_argument& e) {
            throw e;
        }
    }

    int GetDocumentId(int index) const {
        if (index < 0 || index > GetDocumentCount()) {
            //return SearchServer::INVALID_DOCUMENT_ID;
            throw out_of_range("id is out of range"s);
        }
        return index;
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
        int document_id) const {
        if (!ParseQuery(raw_query).has_value()) {
            throw invalid_argument("invalid symbols"s);
        }
        Query query = *ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (!IsValidWord(word)) {
                throw invalid_argument("invalid symbols"s);
            }
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
            if (!IsValidWord(word)) {
                throw invalid_argument("invalid symbols"s);
            }
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return{ matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };
    const set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    // проверка слова на отсутствие спецсимволов
    static bool IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus = false;
        bool is_stop = false;
    };

    optional<QueryWord> ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            if (text.size() == 1
                || text[1] == '-') {
                return nullopt;
            }
            is_minus = true;
            text = text.substr(1);
        }
        QueryWord query_word = { text, is_minus, IsStopWord(text) };
        return query_word;
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    optional<Query> ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            if (!ParseQueryWord(word).has_value()) {
                return nullopt;
            }
            const QueryWord query_word = *ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query,
        DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto& [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

// ==================== дл¤ примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

int main() {
    try {
        setlocale(LC_ALL, "Russian");
        SearchServer search_server("и в на"s);
        search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
        search_server.AddDocument(1, "пушистый пЄс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
        search_server.AddDocument(-1, "пушистый пЄс и модный ошейник"s, DocumentStatus::ACTUAL, { 1, 2 });
        search_server.AddDocument(3, "большой пЄс скво\x12рец"s, DocumentStatus::ACTUAL, { 1, 3, 2 });
        const auto documents = search_server.FindTopDocuments("--пушистый"s);
        for (const Document& document : documents) {
            PrintDocument(document);
        }
    } catch(invalid_argument& e) {
        cout << "ќшибка: "s << e.what() << endl;
    }
    catch (out_of_range& e) {
        cout << "ќшибка: "s << e.what() << endl;
    }
}