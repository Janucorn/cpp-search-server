#pragma once
#include "search_server.h"
#include "document.h"

#include <string>
#include <deque>

// Определяет сколько было запросов, на которые ничего не нашлось
class RequestQueue {
    
public:
    explicit RequestQueue(const SearchServer& search_server);
    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate)
    {
        // увеличваем время за каждый запрос
        ++step_time_;
        // удаляем старые запросы
        if (step_time_ > min_in_day_) {
            if (requests_.front().count > 1) {
                --requests_.front().count;
            }
            else {
                requests_.pop_front();
            }
        }
        // результаты поиска запроса
        auto documents = search_server_.FindTopDocuments(raw_query, document_predicate);
        // обработка пустого ответа на запрос
        if (documents.empty()) {
            if (requests_.empty() || requests_.back().query != empty_request_) {
                requests_.push_back({ empty_request_, 1 });
            }
            else {
                ++requests_.back().count;
            }
        }
        // обработка непустого ответа на запрос
        else {
            if (requests_.empty() || requests_.back().query != raw_query) {
                requests_.push_back({ raw_query, 1 });
            }
            else {
                ++requests_.back().count;
            }
        }
        return documents;
    }

    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);

    std::vector<Document> AddFindRequest(const std::string& raw_query);
    
    // определение пустых результатов на запрос
    int GetNoResultRequests() const;
    
private:
    struct QueryResult {
        // определите, что должно быть в структуре
        std::string query; //запрос
        int count;  // количество запросов
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    // возможно, здесь вам понадобится что-то ещё
    const SearchServer& search_server_;
    std::vector<Document> documents_;
    int step_time_ = 0;
    
    std::string empty_request_ = "empty request";
};
