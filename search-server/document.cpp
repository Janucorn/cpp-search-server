#include "document.h"

    Document::Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    std::ostream& operator<<(std::ostream& os, const Document& document) {
        using namespace std::string_literals;
        return os << "{ "s
            << "document_id = "s << document.id << ", "s
            << "relevance = "s << document.relevance << ", "s
            << "rating = "s << document.rating << " }"s;
    }

    void PrintDocument(const Document& document) {
        using namespace std::string_literals;
        std::cout << "{ "s
            << "document_id = "s << document.id << ", "s
            << "relevance = "s << document.relevance << ", "s
            << "rating = "s << document.rating << " }"s << std::endl;
    }