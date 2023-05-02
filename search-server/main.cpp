#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>

using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
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
        } else {
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
};

struct Query {
    set<string> plus_words;
    set<string> minus_words;
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {
        ++document_count_;
        // определяем для каждого слова id документа и по умолчанию записываем индекс TF 0.0
        for (const string& word : SplitIntoWordsNoStop(document)) {
            if (word_to_documents_freq_[word].count(document_id) != 0) {
                // увеличиваем индекс TF если слово повторяется в одном документе
                word_to_documents_freq_[word][document_id] += 1.0 / SplitIntoWordsNoStop(document).size();
            } else {
                word_to_documents_freq_[word][document_id] = 1.0 /  SplitIntoWordsNoStop(document).size();
            }
        } 
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query_words = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query_words);

        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:
    set<string> stop_words_; // словарь стоп-слов
    map<string, map<int, double>> word_to_documents_freq_ ;// ключ- слово, значение - вхожий словарь: ключ -  id документов, значение - индекс TF
    int document_count_ = 0; // количество документов для вычисления индекса IDF
   
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        string minus_word;
        for (const string& word : SplitIntoWords(text)) {
            
            if (word[0] == '-') {
                minus_word = word;
                if(!IsStopWord(minus_word.substr(1))) {
                    words.push_back(word);
                }
            } else if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    // Функция разбора слова: это слово = минус-слово или стоп-слово?
    
    QueryWord ParseQueryWord(string text) const {
        QueryWord query;
        
        query.is_minus = false;
        if (text[0] == '-') {
            query.is_minus = true;
            text = text.substr(1);
            // .substr(pos) возвращает подстроку с начальной
            // позицией [pos]   
        }
        query.data = text;
        query.is_stop = IsStopWord(text);
        return query;
    }

    Query ParseQuery(const string& text) const {
        Query query_words;
        for (const string& word : SplitIntoWordsNoStop(text)) {
            QueryWord parsed_word = ParseQueryWord(word);
            if (!parsed_word.is_stop) {
                if (parsed_word.is_minus) {
                    query_words.minus_words.insert(parsed_word.data);
                }
                else {
                    query_words.plus_words.insert(parsed_word.data);
                }
            }
        }
        return query_words;
    }

    // функция расчета индекса IDF (inverse document frequency): кол-во всех документов в базе делят на
    // кол-во документов, содержащих слово
    double ComputeInverseDocumentFreq(const string& word) const {
        return log(1.0 * document_count_ / word_to_documents_freq_.at(word).size());
    }

    vector<Document> FindAllDocuments(const Query& query_words) const {
        vector<Document> matched_documents;
        map<int, double> document_to_relevance; // ключ - id документа, значение - сумма индексов TF IDF
        vector<int> erase_document_id ;
        //term_freq - это индекс TF = слово встречающееся количество раз в документе / общее число слов в документе
        for (const string& word : query_words.plus_words) {
                if (word_to_documents_freq_.count(word) != 0) {
                    for (const auto& [document_id, term_freq] : word_to_documents_freq_.at(word)) {
                        document_to_relevance[document_id] += term_freq * ComputeInverseDocumentFreq(word); 
                }
            }
        }
        for (const string& word : query_words.minus_words) {
            
                if (word_to_documents_freq_.count(word) != 0) {
                    for (const auto& [document_id, term_freq] : word_to_documents_freq_.at(word)) {
                    erase_document_id.push_back(document_id);
                    }
                
            }
        }
        for (const int& id : erase_document_id) {
            document_to_relevance.erase(id);
        }
        for (const auto& [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({document_id, relevance});
        }
        return matched_documents;
    }  
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}


/* вводные данные для примера:
is are was a an in the with near at
3
a colorful parrot with green wings and red tail is lost
a grey hound with black ears is found at the railway station
a white cat with long furry tail is found near the red square
white cat long tail
*/

/* на выходе должно получиться:
{ document_id = 2, relevance = 0.462663 }
{ document_id = 0, relevance = 0.0506831 }
*/