#if !defined(BALSA_GEOMETRY_IO_MSH_DATA_HPP)
#define BALSA_GEOMETRY_IO_MSH_DATA_HPP
/// @file MshData.hpp
/// Owning/borrowing wrapper around mshio::MshSpec with block views and
/// entity / physical-group queries.  read_msh / write_msh do file I/O.
///
/// balsa::geometry owns MSH parsing; mesh libraries built on balsa
/// (e.g. quiver) construct their meshes from this.

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include <expected>
#include <filesystem>
#include <string>

#include <zipper/types.hpp>

#include <mshio/MshSpec.h>
#include <mshio/mshio.h> // nodes_per_element(), get_element_dim()

namespace balsa::geometry::io {

// ── mdspan alias ────────────────────────────────────────────────────────────
template <typename T, size_t Rank>
using dextents_mdspan = ::zipper::mdspan<T, ::zipper::dextents<Rank>>;

// ── ElementBlockView ────────────────────────────────────────────────────────
/// Lightweight, non-owning view of an mshio::ElementBlock.
/// Stores a pointer to the block — the underlying MshSpec must outlive this.
struct ElementBlockView {
    const mshio::ElementBlock *block;

    /// Returns the entity dimension of this block.
    [[nodiscard]] int entity_dim() const { return block->entity_dim; }

    /// Returns the entity tag of this block.
    [[nodiscard]] int entity_tag() const { return block->entity_tag; }

    /// Returns the MSH element type code.
    [[nodiscard]] int element_type() const { return block->element_type; }

    /// Returns the number of elements in this block.
    [[nodiscard]] size_t num_elements() const {
        return block->num_elements_in_block;
    }

    /// Flat element data: [tag, n0, n1, …, nK] repeated per element.
    [[nodiscard]] std::span<const size_t> data() const { return block->data; }

    /// Number of size_t values per element (1 tag + nodes_per_element).
    [[nodiscard]] size_t stride() const {
        return 1 + mshio::nodes_per_element(block->element_type);
    }

    /// 2-D view shaped [num_elements × stride].
    [[nodiscard]] auto as_mdspan() const -> dextents_mdspan<const size_t, 2> {
        return dextents_mdspan<const size_t, 2>(
            block->data.data(), num_elements(), stride());
    }
};

// ── NodeBlockView ───────────────────────────────────────────────────────────
/// Lightweight, non-owning view of an mshio::NodeBlock.
struct NodeBlockView {
    const mshio::NodeBlock *block;

    /// Returns the entity dimension of this node block.
    [[nodiscard]] int entity_dim() const { return block->entity_dim; }

    /// Returns the entity tag of this node block.
    [[nodiscard]] int entity_tag() const { return block->entity_tag; }

    /// Whether this node block uses parametric coordinates.
    [[nodiscard]] int parametric() const { return block->parametric; }

    /// Returns the number of nodes in this block.
    [[nodiscard]] size_t num_nodes() const { return block->num_nodes_in_block; }

    /// 1-based node tags.
    [[nodiscard]] std::span<const size_t> tags() const { return block->tags; }

    /// Flat coordinate data: [x, y, z, …] per node.
    [[nodiscard]] std::span<const double> data() const { return block->data; }

    /// Number of doubles per node (3 for non-parametric, 3 + entity_dim for
    /// parametric).
    [[nodiscard]] size_t coords_per_node() const {
        return block->parametric ? (3 + static_cast<size_t>(block->entity_dim))
                                 : 3;
    }

    /// 2-D view shaped [num_nodes × coords_per_node].
    [[nodiscard]] auto as_mdspan() const -> dextents_mdspan<const double, 2> {
        return dextents_mdspan<const double, 2>(
            block->data.data(), num_nodes(), coords_per_node());
    }
};

// ── MshData ─────────────────────────────────────────────────────────────────
/// PIMPL wrapper around mshio::MshSpec.
///
/// Supports two ownership modes:
///   - **Owning**      – allocates and owns the MshSpec (default ctor, MshSpec
///                        ctor).
///   - **Non-owning**  – borrows an external MshSpec via `MshData::borrow()`.
///                        Caller must ensure the spec outlives this object.
class MshData {
  public:
    /// Construct an empty, owning MshData.
    MshData();

    /// Construct an owning MshData from a loaded MshSpec.
    explicit MshData(mshio::MshSpec spec);

    ~MshData();

    /// Move constructor.
    MshData(MshData &&other) noexcept;

    /// Move assignment.
    MshData &operator=(MshData &&other) noexcept;

    MshData(const MshData &) = delete;
    MshData &operator=(const MshData &) = delete;

    /// Create a non-owning MshData that borrows @p spec.
    /// @warning The caller must ensure @p spec outlives the returned MshData.
    [[nodiscard]] static MshData borrow(mshio::MshSpec &spec);

    /// Whether this instance owns its MshSpec.
    [[nodiscard]] bool is_owning() const;

    // ── Query helpers ───────────────────────────────────────────────────────

    /// Returns the total number of element blocks.
    [[nodiscard]] size_t num_element_blocks() const;

    /// Returns the total number of node blocks.
    [[nodiscard]] size_t num_node_blocks() const;

    /// Returns the element block at the given index.
    [[nodiscard]] ElementBlockView element_block(size_t index) const;

    /// Returns the node block at the given index.
    [[nodiscard]] NodeBlockView node_block(size_t index) const;

    /// Whether any element block has the given dimension.
    [[nodiscard]] bool has_element_dimension(int8_t dim) const;

    /// Returns the highest element dimension across all blocks.
    [[nodiscard]] int max_element_dimension() const;

    // ── Entity & Physical Group helpers ─────────────────────────────────────

    /// Physical group tags for an entity at (dim, tag). Empty if not found.
    [[nodiscard]] std::span<const int>
        entity_physical_group_tags(int dim, int tag) const;

    /// Find a PhysicalGroup by (dim, tag). nullptr if not found.
    [[nodiscard]] const mshio::PhysicalGroup *
        find_physical_group(int dim, int tag) const;

    /// Element block indices matching entity (dim, tag).
    [[nodiscard]] std::vector<size_t>
        element_block_indices_for_entity(int dim, int tag) const;

    /// Element block indices whose entity belongs to physical group
    /// (dim, physical_tag).
    [[nodiscard]] std::vector<size_t>
        element_block_indices_for_physical_group(int dim,
                                                 int physical_tag) const;

    /// All entity tags at a given dimension.
    [[nodiscard]] std::vector<int> entity_tags(int dim) const;

    /// All physical group tags at a given dimension.
    [[nodiscard]] std::vector<int> physical_group_tags(int dim) const;

    /// Max node tag across all node blocks.
    [[nodiscard]] size_t max_node_tag() const;

    /// Boundary entity tags for entity at (dim, tag).
    /// E.g. for a surface, returns its boundary_curve_tags.
    /// Returns empty for points (dim==0) or if entity not found.
    [[nodiscard]] std::vector<int> boundary_entity_tags(int dim, int tag) const;

    // ── Raw spec access (for writer / loader code) ──────────────────────────

    [[nodiscard]] mshio::MshSpec &spec();
    [[nodiscard]] const mshio::MshSpec &spec() const;

  private:
    explicit MshData(mshio::MshSpec *ptr, bool owning);

    mshio::MshSpec *m_spec = nullptr;
    bool m_owning = true;
};

/// Read an MSH file (v2/v4, ascii/binary).
auto read_msh(const std::filesystem::path &path)
    -> std::expected<MshData, std::string>;

/// Write an MSH file.
auto write_msh(const std::filesystem::path &path, const MshData &data)
    -> std::expected<void, std::string>;

} // namespace balsa::geometry::io
#endif
