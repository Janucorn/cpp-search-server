#pragma once
#include "search_server.h"
#include "document.h"

#include <string>
#include <deque>

// ���������� ������� ���� ��������, �� ������� ������ �� �������
class RequestQueue {
    
public:
    explicit RequestQueue(const SearchServer& search_server);
    // ������� "������" ��� ���� ������� ������, ����� ��������� ���������� ��� ����� ����������
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate)
    {
        // ���������� ����� �� ������ ������
        ++step_time_;
        // ������� ������ �������
        if (step_time_ > min_in_day_) {
            if (requests_.front().count > 1) {
                --requests_.front().count;
            }
            else {
                requests_.pop_front();
            }
        }
        // ���������� ������ �������
        auto documents = search_server_.FindTopDocuments(raw_query, document_predicate);
        // ��������� ������� ������ �� ������
        if (documents.empty()) {
            if (requests_.empty() || requests_.back().query != empty_request_) {
                requests_.push_back({ empty_request_, 1 });
            }
            else {
                ++requests_.back().count;
            }
        }
        // ��������� ��������� ������ �� ������
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
    
    // ����������� ������ ����������� �� ������
    int GetNoResultRequests() const;
    
private:
    struct QueryResult {
        // ����������, ��� ������ ���� � ���������
        std::string query; //������
        int count;  // ���������� ��������
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    // ��������, ����� ��� ����������� ���-�� ���
    const SearchServer& search_server_;
    std::vector<Document> documents_;
    int step_time_ = 0;
    
    std::string empty_request_ = "empty request";
};
