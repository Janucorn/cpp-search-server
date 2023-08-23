#pragma once

#include <map>
#include <mutex>
#include <string>
#include <vector>


template <typename Key, typename Value>
class ConcurrentMap {
private:
struct Bucket_ {
        std::map<Key, Value> map_;
        std::mutex mutex_;
    };
    
public:
  
	static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys");

    struct Access {
        Access(const Key& key, Bucket_& bucket)
            : guard(bucket.mutex_)
            , ref_to_value(bucket.map_[key]) {
        }
        
      std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };

    // Конструктор класса ConcurrentMap<Key, Value> принимает
    // количество подсловарей, на которые надо разбить все пространство ключей
    explicit ConcurrentMap(size_t bucket_count) 
    : buckets_(bucket_count) {
    }

    Access operator[](const Key& key) {
        const auto& index = static_cast<uint64_t>(key) % buckets_.size();
       
        return {key, buckets_[index]};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& bucket : buckets_) {
            std::lock_guard<std::mutex> guard(bucket.mutex_);
            result.merge(bucket.map_);
        }
        return result;
    }

private:
    std::vector<Bucket_> buckets_;
};
