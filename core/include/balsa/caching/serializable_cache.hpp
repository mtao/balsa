#if !defined(BALSA_CACHING_SERIALIZABLE_CACHE_HPP)
#define BALSA_CACHING_SERIALIZABLE_CACHE_HPP

#include <list>
#include <vector>
#include <algorithm>

#include <memory>
#include <filesystem>

namespace balsa::caching {


template<typename ObjectType>
struct SerializerBase {
    using ObjPtr = std::shared_ptr<const ObjectType>;
    virtual ~SerializerBase() = default;
    virtual ObjPtr deserialize(const std::filesystem::path &path) const = 0;
    virtual void serialize(const ObjectType &obj, const std::filesystem::path &path, bool overwrite = false) const = 0;
};

template<typename ObjectType>
class SerializableCache {
  public:
    using ObjPtr = std::shared_ptr<const ObjectType>;
    struct CacheNode {
      public:
        ObjPtr object() const { return _object; }
        const std::filesystem::path &path() const { return _path; }
        void set(ObjPtr obj) { _object = std::move(obj); }

      private:
        ObjPtr _object;
        std::filesystem::path _path;
        friend class SerializableCache;
    };
    enum class CostMode {
        Size,// using the CachedObject::size() member function
        Count,// Use the total number of elements
    };


    ObjPtr read(const std::filesystem::path &path) const;

    void set_serializer(std::shared_ptr<SerializerBase<ObjectType>> serializer) {
        _serializer = std::move(serializer);
    }

    void set_read_only(bool read_only) { _read_only = read_only; }
    bool read_only() const { return _read_only; }

    std::shared_ptr<SerializerBase<ObjectType>> serializer() const { return _serializer; }

  private:
    std::shared_ptr<SerializerBase<ObjectType>> _serializer;
    mutable std::list<CacheNode> _cache;

    bool _read_only = true;
    CostMode _cost_mode = CostMode::Count;
};

template<typename ObjectType>
auto SerializableCache<ObjectType>::read(const std::filesystem::path &path) const -> ObjPtr {
    // Check if already cached
    for (auto &node : _cache) {
        if (node._path == path) {
            // Move to front (LRU)
            return node.object();
        }
    }

    // Not cached — deserialize from disk
    if (!_serializer) {
        return nullptr;
    }

    ObjPtr obj = _serializer->deserialize(path);
    if (obj) {
        CacheNode node;
        node._path = path;
        node._object = obj;
        _cache.push_front(std::move(node));
    }
    return obj;
}

}// namespace balsa::caching
#endif
