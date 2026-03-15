#if !defined(BALSA_CACHING_PREFETCHING_SERIALIZED_CACHE_HPP)
#define BALSA_CACHING_PREFETCHING_SERIALIZED_CACHE_HPP

#include "serializable_cache.hpp"

#include <vector>
#include <future>
#include <filesystem>

namespace balsa::caching {

// A SerializableCache extended with sequential prefetching.
//
// Use case: iterating over a sequence of files (e.g. animation frames)
// where the access pattern is predictable. The cache pre-loads upcoming
// entries asynchronously based on the configured PrefetchMode.
//
// Usage:
//   PrefetchingSerializedCache<MeshType> cache;
//   cache.set_serializer(my_serializer);
//   cache.set_paths({frame0.obj, frame1.obj, ...});
//   auto mesh = cache.load(42);  // loads frame 42 + prefetches neighbors
//
template<typename ObjectType>
class PrefetchingSerializedCache : public SerializableCache<ObjectType> {
  public:
    using Base = SerializableCache<ObjectType>;
    using ObjPtr = typename Base::ObjPtr;

    enum class PrefetchMode {
        Forward,// prefetch entries ahead of the current index
        Backward,// prefetch entries behind the current index
        Balanced,// prefetch in both directions around the current index
    };

    enum class CostMode {
        Count,// each entry costs 1
        Disk,// cost is file size in bytes
    };

    // Set the ordered sequence of paths for index-based access.
    void set_paths(std::vector<std::filesystem::path> paths) {
        _paths = std::move(paths);
        _pending.clear();
        _pending.resize(_paths.size());
    }

    const std::vector<std::filesystem::path> &paths() const { return _paths; }
    size_t size() const { return _paths.size(); }

    // Load by index into the path sequence. Triggers prefetching.
    ObjPtr load(size_t index);

    // Load by path (searches the path list for the index, then delegates).
    ObjPtr load(const std::filesystem::path &path);

    void set_prefetch_mode(PrefetchMode mode) { _prefetch_mode = mode; }
    PrefetchMode prefetch_mode() const { return _prefetch_mode; }

    void set_cost_mode(CostMode mode) { _cost_mode = mode; }
    CostMode cost_mode() const { return _cost_mode; }

    void set_cost_limit(double limit) { _cost_limit = limit; }
    double cost_limit() const { return _cost_limit; }

  private:
    // Resolve a pending future at the given index, if any.
    ObjPtr resolve_pending(size_t index);

    // Launch async prefetch for a single index (no-op if already cached/pending).
    void prefetch(size_t index);

    // Prefetch neighbors of the given index based on the current mode.
    void update_prefetch(size_t index);

    // Cost of a single cache entry (for budget tracking).
    double cost_of(size_t index) const;

    std::vector<std::filesystem::path> _paths;

    // Parallel to _paths — holds in-flight async reads.
    // A default-constructed (invalid) future means no pending read.
    std::vector<std::future<ObjPtr>> _pending;

    PrefetchMode _prefetch_mode = PrefetchMode::Forward;
    CostMode _cost_mode = CostMode::Count;
    double _cost_limit = 5;
};


template<typename ObjectType>
auto PrefetchingSerializedCache<ObjectType>::load(size_t index) -> ObjPtr {
    if (index >= _paths.size()) {
        return nullptr;
    }

    const auto &path = _paths[index];

    // First check if there's a pending async read for this index.
    ObjPtr obj = resolve_pending(index);
    if (obj) {
        update_prefetch(index);
        return obj;
    }

    // Delegate to the base cache (handles LRU lookup + synchronous deserialization).
    obj = Base::read(path);

    // Trigger prefetching of neighbors.
    update_prefetch(index);

    return obj;
}

template<typename ObjectType>
auto PrefetchingSerializedCache<ObjectType>::load(const std::filesystem::path &path) -> ObjPtr {
    for (size_t i = 0; i < _paths.size(); ++i) {
        if (_paths[i] == path) {
            return load(i);
        }
    }
    // Path not in the sequence — fall back to non-prefetching base read.
    return Base::read(path);
}

template<typename ObjectType>
auto PrefetchingSerializedCache<ObjectType>::resolve_pending(size_t index) -> ObjPtr {
    if (index >= _pending.size()) {
        return nullptr;
    }
    auto &fut = _pending[index];
    if (!fut.valid()) {
        return nullptr;
    }
    // Block until the async read completes.
    ObjPtr obj = fut.get();// invalidates the future
    if (obj) {
        // Insert into the base cache so future reads hit the LRU.
        // We do this by calling Base::read which will re-deserialize — but the
        // object is already loaded, so we just return it directly.
        // (A more sophisticated version could inject directly into the LRU.)
    }
    return obj;
}

template<typename ObjectType>
void PrefetchingSerializedCache<ObjectType>::prefetch(size_t index) {
    if (index >= _paths.size()) {
        return;
    }
    // Skip if already pending.
    if (_pending[index].valid()) {
        return;
    }
    // Skip if already in the base cache (Base::read would return it synchronously,
    // but we don't want to call it here to avoid modifying LRU order).
    // For now, we optimistically launch the async read.

    const auto &path = _paths[index];
    auto serializer = this->serializer();
    if (!serializer) {
        return;
    }

    _pending[index] = std::async(std::launch::async,
                                 [serializer, path]() -> ObjPtr {
                                     return serializer->deserialize(path);
                                 });
}

template<typename ObjectType>
void PrefetchingSerializedCache<ObjectType>::update_prefetch(size_t index) {
    double total_cost = cost_of(index);

    switch (_prefetch_mode) {
    case PrefetchMode::Forward: {
        for (size_t i = index + 1; i < _paths.size() && total_cost < _cost_limit; ++i) {
            total_cost += cost_of(i);
            prefetch(i);
        }
        break;
    }
    case PrefetchMode::Backward: {
        if (index == 0) {
            return;
        }
        for (size_t i = index - 1; total_cost < _cost_limit; --i) {
            total_cost += cost_of(i);
            prefetch(i);
            if (i == 0) break;
        }
        break;
    }
    case PrefetchMode::Balanced: {
        size_t fwd = index;
        size_t bwd = index;
        bool fwd_active = true;
        bool bwd_active = true;
        while ((fwd_active || bwd_active) && total_cost < _cost_limit) {
            if (fwd_active) {
                if (fwd + 1 < _paths.size()) {
                    ++fwd;
                    total_cost += cost_of(fwd);
                    if (total_cost < _cost_limit) {
                        prefetch(fwd);
                    }
                } else {
                    fwd_active = false;
                }
            }
            if (bwd_active && total_cost < _cost_limit) {
                if (bwd > 0) {
                    --bwd;
                    total_cost += cost_of(bwd);
                    if (total_cost < _cost_limit) {
                        prefetch(bwd);
                    }
                } else {
                    bwd_active = false;
                }
            }
        }
        break;
    }
    }
}

template<typename ObjectType>
double PrefetchingSerializedCache<ObjectType>::cost_of(size_t index) const {
    switch (_cost_mode) {
    case CostMode::Count:
        return 1.0;
    case CostMode::Disk: {
        if (index < _paths.size()) {
            std::error_code ec;
            auto size = std::filesystem::file_size(_paths[index], ec);
            if (!ec) {
                return static_cast<double>(size);
            }
        }
        return 1.0;
    }
    }
    return 1.0;// unreachable, silences compiler warnings
}

}// namespace balsa::caching
#endif
