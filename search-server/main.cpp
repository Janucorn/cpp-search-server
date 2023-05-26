#include <algorithm>
//#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
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
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    template<typename KeyMapper>
    vector<Document> FindTopDocuments(const string& raw_query, KeyMapper key_mapper) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, key_mapper);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
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

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus status) const {
        return FindTopDocuments(raw_query, [&status]
        (int document_id, DocumentStatus doc_status, int rating) {
                return doc_status == status; });
    }

    int GetDocumentCount() const {
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
        int document_id) const {
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            if (word_to_document_freqs_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        for (const string& word : query.minus_words) {
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

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

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
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
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

    template<typename KeyMapper>
    vector<Document> FindAllDocuments(const Query& query, KeyMapper key_mapper) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                auto document = documents_.at(document_id);
                if (key_mapper(document_id, document.status, document.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

// ==================== для примера =========================

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

// Шаблонная функция для вывода в поток ошибок сообщения об 
// успешном выполнении теста
template <typename Func>
void TestImpl(const Func& func, const string& func_name) {
    func();
    cerr << func_name << " OK"s << endl;
}

#define RUN_TEST(func) TestImpl((func), #func)

template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str,
    const string& u_str, const string& file, const string& func,
    unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): " << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, 
    const string file, const string& func,
    unsigned line, const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

// Тест проверяет, что поисковая система исключает стоп-слова
// при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // убеждаемся, что поиск слова, не входящего в список
    // стоп-слов, находит нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT(!found_docs.empty());
        
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    // убеждаемся, что поиск этого же слова, входящего в список стоп-слов,
    // возвращает пустой результат
    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}

// Тест проверяет, что поисковая система исключает документы,
// содержащие минус-слова
void TestExcludeDocumentsWithMinusWords() {
    const string content1 = "cat in the city"s;
    const int doc_id1 = 43;
    const vector<int> ratings1 = { 1, 2, 3 };

    const string content2 = "cat with emotional damage"s;
    const int doc_id2 = 44;
    const vector<int> ratings2 = { 5, 2 };
    // убеждаемся, что документ с минус-словом не выводится в результате
    {
        SearchServer server;
        const string query = "cat in the -city"s;
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        
        const auto& documents = server.FindTopDocuments(query, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(documents.size(), 1u);
        ASSERT(documents[0].id == doc_id2);
    }
}

// Тест матчинга докуметов. При матчинге докумета по поисковому запросу
// должны быть возвращены все слова из поискового запроса, присутсвующие в документе.
// Если есть соответствие хотя бы по одному минус-слову, должен возвращаться 
// пустой список слов
void TestMatching() {
    const string content = "cat in the city"s;
    const int doc_id = 44;
    const vector<int> ratings = { 1, 2, 3 };

    // Проверка, что возвращаются только те слова, что имеются в документе
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("gray cat city"s, doc_id);
        ASSERT_EQUAL(words.size(), 2u);
        ASSERT(status == DocumentStatus::ACTUAL);
    }
    // Проверка, что стоп-слова не учитываются
    {
        SearchServer server;
        server.SetStopWords("cat"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("gray cat city"s, doc_id);
        ASSERT_EQUAL(words.size(), 1u);
        ASSERT(status == DocumentStatus::ACTUAL);
    }
    // Проверка, что возвращается список слов, 
    // с минус-словом, не имеющемся в документе
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("-gray cat city"s, doc_id);
        ASSERT_EQUAL(words.size(), 2u);
        ASSERT(status == DocumentStatus::ACTUAL);
    }
    // Проверка, что возвращается пустой список слов, 
    // с минус-словом, имеющемся в документе
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto [words, status] = server.MatchDocument("gray -cat city"s, doc_id);
        ASSERT(words.empty());
        ASSERT(status == DocumentStatus::ACTUAL);
    }
}

// Сортировка найденных документов по релевантности. 
// Возвращаемые при поиске документов результаты 
// должны быть отсортированы в порядке убывания релевантности.
void TestSortingDocumentsByRelevance() {
    const string content0 = "cat in the city"s;
    const int doc_id0 = 45;
    const vector<int> ratings0 = { 1, 2, 3 };

    const string content1 = "cat eat fish"s;
    const int doc_id1 = 46;
    const vector<int> ratings1 = { 3, 3, 3 };

    const string content2 = "the emotional damage"s;
    const int doc_id2 = 47;
    const vector<int> ratings2 = { 5 };

    {
        SearchServer server;
        server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings0);
        server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content2, DocumentStatus::ACTUAL, ratings2);
        const auto& documents = server.FindTopDocuments("cat in the cafe"s);
        ASSERT(documents.size() == 3u);
        ASSERT(documents[0].relevance >= documents[1].relevance);
        ASSERT(documents[1].relevance >= documents[2].relevance);
    }
}

// Проверка расчета рейтинга.
// Рейтинг = среднему арифметическому оценок документа
void TestRatingCompute() {
    const string content0 = "cat in the city"s;
    const int doc_id0 = 47;
    const vector<int> ratings0 = { 1, 2, 3 };

    SearchServer server;
    server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings0);
    ASSERT(!server.FindTopDocuments("cat"s).empty());
    ASSERT_EQUAL(server.FindTopDocuments("cat"s)[0].rating, ( 1 + 2 + 3) / 3);
}

void TestFilteringDocumentsByPredicate() {
    const int doc_id0 = 48;
    const string content0 = "cat in the city"s;
    const vector<int> ratings0 = { 1, 2, 3 };

    const int doc_id1 = 49;
    const string content1 = "cat with emotional damage"s;
    const vector<int> ratings1 = { 3, 3, 3 };

    const int doc_id2 = 50;
    const string content2 = "the snyder cat"s;
    const vector<int> ratings2 = { 4, 5, 6 };

    const int doc_id3 = 51;
    const string content3 = "video with cat"s;
    const vector<int> ratings3 = { 1, 1 };

    SearchServer server;
    server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings0);
    server.AddDocument(doc_id1, content1, DocumentStatus::BANNED, ratings1);
    server.AddDocument(doc_id2, content2, DocumentStatus::REMOVED, ratings2);
    server.AddDocument(doc_id3, content3, DocumentStatus::IRRELEVANT, ratings3);

    // Проверка, что сортировка найденных документов 
    // без предиката возвращает документы со статусом ACTUAL
    {
        const vector<Document>& documents = server.FindTopDocuments("gray cat"s);
        ASSERT(!documents.empty());
        ASSERT_EQUAL(documents[0].id, doc_id0);
    }

    // Проверка, что вывод по указанному в предикате статусу
    {
        const vector<Document>& documents = server.FindTopDocuments("gray cat"s,
            [](int document_id, DocumentStatus status, int rating) { return status != DocumentStatus::ACTUAL; });
        ASSERT_EQUAL(documents.size(), 3u);
        ASSERT(documents[0].id != doc_id0);
        ASSERT(documents[1].id != doc_id0);
        ASSERT(documents[2].id != doc_id0);
    }

    // Проверка вывода по четным/нечетным id 
    {
        const vector<Document>& documents = server.FindTopDocuments("gray cat"s,
            [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        ASSERT_EQUAL(documents.size(), 2u);
        ASSERT(documents[0].id % 2 == 0);
        ASSERT(documents[1].id % 2 == 0);

        const vector<Document>& documents1 = server.FindTopDocuments("gray cat"s,
            [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 1; });
        ASSERT(documents1[0].id % 2 == 1);
        ASSERT(documents1[1].id % 2 == 1);
    }
}

void TestFindDocumentsWithStatus() {
    const int doc_id0 = 48;
    const string content0 = "cat in the city"s;
    const vector<int> ratings0 = { 1, 2, 3 };

    const int doc_id1 = 49;
    const string content1 = "cat with emotional damage"s;
    const vector<int> ratings1 = { 3, 3, 3 };

    const int doc_id2 = 50;
    const string content2 = "the snyder cat"s;
    const vector<int> ratings2 = { 4, 5, 6 };

    const int doc_id3 = 51;
    const string content3 = "video with cat"s;
    const vector<int> ratings3 = { 1, 1 };

    SearchServer server;
    server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings0);
    server.AddDocument(doc_id1, content1, DocumentStatus::BANNED, ratings1);
    server.AddDocument(doc_id2, content2, DocumentStatus::REMOVED, ratings2);
    server.AddDocument(doc_id3, content3, DocumentStatus::IRRELEVANT, ratings3);
    // Проверка, что возвращен документ со статусом ACTUAL
    {
        const auto documents = server.FindTopDocuments("little cat"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(documents.size(), 1u);
        ASSERT(documents[0].id == doc_id0);
    }
    // Проверка, что возвращен документ со статусом BANNED
    {
        const auto documents = server.FindTopDocuments("little cat"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(documents.size(), 1u);
        ASSERT(documents[0].id == doc_id1);
    }
    // Проверка, что возвращен документ со статусом REMOVED
    {
        const auto documents = server.FindTopDocuments("little cat"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(documents.size(), 1u);
        ASSERT(documents[0].id == doc_id2);
    }
    // Проверка, что возвращен документ со статусом IRRELEVANT
    {
        const auto documents = server.FindTopDocuments("little cat"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(documents.size(), 1u);
        ASSERT(documents[0].id == doc_id3);
    }
}

void TestRelevanceCompute() {
    const int doc_id0 = 52;
    const string content0 = "cat in the city"s;
    const vector<int> ratings0 = { 1 };

    const int doc_id1 = 53;
    const string content1 = "little gray cat with emotional damage"s;
    const vector<int> ratings1 = { 2 };

    SearchServer server;
    server.AddDocument(doc_id0, content0, DocumentStatus::ACTUAL, ratings0);
    server.AddDocument(doc_id1, content1, DocumentStatus::ACTUAL, ratings1);
    const auto& documents = server.FindTopDocuments("with cat"s);
    // TF(cat@doc_id0) = 1/4; TF(cat@doc_id1) = 1/6; IDF(cat) = log(2/2); 
    // TF(with@doc_id1) = 1/6; IDF(with) = log(2/1)
    const double relev0 = log(1) / 4; // 0
    const double relev1 = log(2) / 6 + log(1) / 4; // 0,115524
    ASSERT_EQUAL(documents.size(), 2u);
    ASSERT(documents[0].relevance == relev1);
    ASSERT(documents[1].relevance == relev0);
}

// точка входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    // дополнительные тесты
    RUN_TEST(TestExcludeDocumentsWithMinusWords);
    RUN_TEST(TestMatching);
    RUN_TEST(TestSortingDocumentsByRelevance);
    RUN_TEST(TestRatingCompute);
    RUN_TEST(TestFilteringDocumentsByPredicate);
    RUN_TEST(TestFindDocumentsWithStatus);
    RUN_TEST(TestRelevanceCompute);
}

int main() {
    TestSearchServer();
    cout << "Search server testing finished"s << endl;

/*    SearchServer search_server;
    search_server.SetStopWords("и в на"s);

    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL,
        { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });

    
    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    */
    return 0;
}

/*Вывод: 
ACTUAL by default:
{ document_id = 1, relevance = 0.866434, rating = 5 }
{ document_id = 0, relevance = 0.173287, rating = 2 }
{ document_id = 2, relevance = 0.173287, rating = -1 }
BANNED:
{ document_id = 3, relevance = 0.231049, rating = 9 }
Even ids:
{ document_id = 0, relevance = 0.173287, rating = 2 }
{ document_id = 2, relevance = 0.173287, rating = -1 }
*/