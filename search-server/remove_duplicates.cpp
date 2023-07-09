#include "remove_duplicates.h"

#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

void RemoveDuplicates(SearchServer& search_server) {
	if (search_server.GetDocumentCount() <= 1) {
		return;
	}
	//Контейнеры для хранения слов документов
	std::vector<std::string> current_cont, tmp_cont;
	//Контейнер для хранения id дупликаторов
	std::set<int> id_to_remove;
	
	for (auto it = search_server.begin(); it != search_server.end(); ++it) {
		if (!search_server.GetWordFrequencies(*it).empty()) {
			tmp_cont.clear();

			//Заполняем временный вектор словами
			for (const auto& [word, _] : search_server.GetWordFrequencies(*it)) {
				tmp_cont.push_back(word);
			}

			//Сравниваем временный вектор с другими документами
			for (auto it_doc = std::next(it, 1); it_doc != search_server.end(); ++it_doc) {
				current_cont.clear();
				for (const auto& [word, _] : search_server.GetWordFrequencies(*it_doc)) {
					current_cont.push_back(word);
				}
				//Запоминаем id документов-дупликаторов
				if (current_cont == tmp_cont) {
					id_to_remove.insert(*it_doc);
				}
			}
		}
	}
	//Удаляем документы-дупликаторы
	for (const int id : id_to_remove) {
		using namespace std::string_literals;
		std::cout << "Found duplicate document id " << id << std::endl;
		search_server.RemoveDocument(id);
	}
	id_to_remove.clear();
}