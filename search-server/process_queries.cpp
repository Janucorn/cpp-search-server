#include "document.h"
#include "process_queries.h"
#include "search_server.h"

#include <algorithm>
#include <execution>
#include <string>
#include <vector>

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    
    std::vector<std::vector<Document>> finded_documents(queries.size());
    
    if (!queries.empty()) {
        std::transform(std::execution::par,
            queries.begin(), queries.end(), finded_documents.begin(),
            [&search_server](const std::string& query) { return search_server.FindTopDocuments(query); }
        );
    }
    return finded_documents;
}

std::vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server, 
    const std::vector<std::string>& queries){
    
    const std::vector<std::vector<Document>> finded_documents = ProcessQueries(search_server, queries);
    
    std::vector<Document>documents;
    for (const auto& docs : finded_documents) {
        for (const auto& doc : docs) {
            documents.push_back(doc);
        }
    }
    return documents;

}