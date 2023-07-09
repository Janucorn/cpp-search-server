#pragma once
#include <iterator>
#include <vector>

template<typename It>
class IteratorRange {
public:
    IteratorRange(It range_begin, It range_end)
        : range_begin_(range_begin),
        range_end_(range_end),
        size_(distance(range_begin, range_end))
    {}
    auto begin() {
        return range_begin_;
    }
    auto end() {
        return range_end_;
    }
    auto size() {
        return size_;
    }
private:
    It range_begin_;
    It range_end_;
    size_t size_;
};

template<typename Iterator>
std::ostream& operator<<(std::ostream& os, IteratorRange<Iterator> it_range) {
    for (auto it = it_range.begin(); it != it_range.end(); ++it) {
        os << *it;
    }
    return os;
}

template<typename It>
class Paginator {
public:
    Paginator(It range_begin, It range_end, size_t page_size) {

        auto page_begin = range_begin;

        for (size_t pages = distance(range_begin, range_end); pages > 0;) {
            // заполняем всю страницу оставшимися результатами
            if (static_cast<size_t>(distance(page_begin, range_end)) < page_size) {
                documents_.push_back(IteratorRange{ page_begin, range_end });
                return;
            }
            auto page_end = page_begin + page_size;
            IteratorRange<It> page(page_begin, page_end);
            documents_.push_back(page);
            // смещаем начало новой страницы на размер страницы
            advance(page_begin, page_size);
            pages -= page_size;
        }
    }
    auto begin() const {
        return documents_.begin();
    }
    auto end() const {
        return documents_.end();
    }
    auto size() const {
        return documents_.size();
    }
private:
    std::vector<IteratorRange<It>> documents_;
};
 
template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}