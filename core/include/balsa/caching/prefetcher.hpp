#if !defined(BALSA_CACHING_PREFETCHER_HPP)
#define BALSA_CACHING_PREFETCHER_HPP

#include <list>
#include <vector>
#include <ranges>
#include <future>

#include <memory>
#include <filesystem>

namespace balsa::caching {


template<typename ObjectType>
class Prefetcher {
  public:
    using ObjPtr = std::shared_ptr<const ObjectType>;
    struct CacheNode {
      public:
        ObjPtr object_ptr = nullptr;
        std::future<ObjPtr> future;
        std::filesystem::path path;
        double cost = 0;
    };
    enum class CostMode {
        Disk,// checks the size of the file
        Count,// Use the total number of elements
        Custom,// use the virtual cost function
    };
    enum class PrefetchMode : int {
        // Modes that assume the data is ordered
        Balanced,// try to center the cache around the current index
        Forward,// try to read as much to the future as possible
        Backward,// try to read into the past as much as possible
    };


    ObjPtr load(const std::filesystem::path &path);
    ObjPtr load(size_t index);
    double cost(const CacheNode &) const;

    virtual ObjPtr read_from_disk(const std::filesystem::path &path) const = 0;
    virtual double custom_cost(const ObjPtr &, const std::filesystem::path &) const { return 1; }

    ObjPtr fetch(CacheNode &path);
    void update_cache(size_t index);

    void prefetch(CacheNode &node);

  private:
    std::vector<CacheNode> _cache;

    size_t _current_index = 0;
    CostMode _cost_mode = CostMode::Count;
    double _cost_limit = 5;
    PrefetchMode _prefetch_mode = PrefetchMode::Forward;
};


template<typename ObjectType>
auto Prefetcher<ObjectType>::load(const std::filesystem::path &path) -> ObjPtr {

    for (auto &&[index, node] : std::views::enumerate(_cache)) {
        if (node.path == path) {
            return load(index);
        }
    }
    return nullptr;
}

template<typename ObjectType>
auto Prefetcher<ObjectType>::load(size_t index) -> ObjPtr {

    ObjPtr to_return = nullptr;
    auto &node = _cache[index];
    if (bool(node.object_ptr)) {
        to_return = node.object_ptr;
    } else {
        to_return = fetch(node);
    }
    // try to get file
    // evoke a potential cache invalidation / prefetch
    update_cache(index);
    return to_return;
}
template<typename ObjectType>
void Prefetcher<ObjectType>::update_cache(size_t index) {

    double total_cost = cost(_cache[index]);
    switch (_prefetch_mode) {
    case PrefetchMode::Balanced: {
        size_t findex = index;
        size_t bindex = index;
        bool fchanged = true;
        bool bchanged = true;
        bool below_cost_limit = true;
        while ((fchanged || bchanged) && below_cost_limit) {
            if (findex < _cache.size() - 1) {
                findex++;
                total_cost += cost(_cache[findex]);
                below_cost_limit = total_cost < _cost_limit;
            } else {
                fchanged = false;
            }

            if (bindex > 0 && below_cost_limit) {
                bindex--;
                total_cost += cost(_cache[bindex]);
                below_cost_limit = total_cost < _cost_limit;
            } else {
                bchanged = false;
            }
        }
        break;
    }
    case PrefetchMode::Forward: {
        if (index >= _cache.size() - 1) {
            return;
        }
        index++;

        for (; total_cost < _cost_limit && index < _cache.size(); ++index) {
            total_cost += cost(_cache[index]);
        }
        break;
    }
    case PrefetchMode::Backward: {
        if (index == 0) {
            return;
        }
        index--;

        // Use ptrdiff_t cast to avoid unsigned underflow when index reaches 0
        for (; total_cost < _cost_limit; --index) {
            total_cost += cost(_cache[index]);
            if (index == 0) break;
        }
        break;
    }
    }
}
template<typename ObjectType>
double Prefetcher<ObjectType>::cost(const CacheNode &node) const {
    if (node.cost != 0) {
        return node.cost;
    }
    switch (_cost_mode) {
    case CostMode::Disk: {
        return 1;
    }
    case CostMode::Count: {
        return 1;
    }
    case CostMode::Custom: {
        return custom_cost(node.object_ptr, node.path);
    }
    }
    return 1;// unreachable, but silences compiler warnings
}

}// namespace balsa::caching
#endif
