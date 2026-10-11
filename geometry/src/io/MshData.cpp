#include "balsa/geometry/io/MshData.hpp"

#include <mshio/exception.h>

#include <algorithm>
#include <functional>
#include <ranges>
#include <stdexcept>

namespace balsa::geometry::io {

// ── Constructors / destructor ───────────────────────────────────────────────

MshData::MshData() : m_spec(new mshio::MshSpec{}), m_owning(true) {}

MshData::MshData(mshio::MshSpec spec)
  : m_spec(new mshio::MshSpec(std::move(spec))), m_owning(true) {}

MshData::MshData(mshio::MshSpec *ptr, bool owning)
  : m_spec(ptr), m_owning(owning) {}

MshData::~MshData() {
    if (m_owning) { delete m_spec; }
}

// ── Move operations ─────────────────────────────────────────────────────────

MshData::MshData(MshData &&other) noexcept
  : m_spec(other.m_spec), m_owning(other.m_owning) {
    other.m_spec = nullptr;
    other.m_owning = false;
}

MshData &MshData::operator=(MshData &&other) noexcept {
    if (this != &other) {
        if (m_owning) { delete m_spec; }
        m_spec = other.m_spec;
        m_owning = other.m_owning;
        other.m_spec = nullptr;
        other.m_owning = false;
    }
    return *this;
}

// ── Factory ─────────────────────────────────────────────────────────────────

MshData MshData::borrow(mshio::MshSpec &spec) { return MshData(&spec, false); }

bool MshData::is_owning() const { return m_owning; }

// ── Query helpers ───────────────────────────────────────────────────────────

size_t MshData::num_element_blocks() const {
    return m_spec->elements.entity_blocks.size();
}

size_t MshData::num_node_blocks() const {
    return m_spec->nodes.entity_blocks.size();
}

ElementBlockView MshData::element_block(size_t index) const {
    assert(index < num_element_blocks());
    return {&m_spec->elements.entity_blocks[index]};
}

NodeBlockView MshData::node_block(size_t index) const {
    assert(index < num_node_blocks());
    return {&m_spec->nodes.entity_blocks[index]};
}

bool MshData::has_element_dimension(int8_t dim) const {
    return std::ranges::any_of(
        m_spec->elements.entity_blocks,
        [dim](const mshio::ElementBlock &eb) { return eb.entity_dim == dim; });
}

int MshData::max_element_dimension() const {
    const auto &blocks = m_spec->elements.entity_blocks;
    if (blocks.empty()) return 0;
    return std::ranges::max(blocks, {}, &mshio::ElementBlock::entity_dim)
        .entity_dim;
}

// ── Spec access ─────────────────────────────────────────────────────────────

mshio::MshSpec &MshData::spec() { return *m_spec; }
const mshio::MshSpec &MshData::spec() const { return *m_spec; }

// ── Entity & Physical Group helpers ─────────────────────────────────────────

std::span<const int> MshData::entity_physical_group_tags(int dim,
                                                         int tag) const {
    const auto &ents = m_spec->entities;
    const auto find_tag = [tag](const auto &entities) -> std::span<const int> {
        auto it = std::ranges::find(
            entities,
            tag,
            &std::remove_cvref_t<decltype(entities)>::value_type::tag);
        if (it != entities.end()) return it->physical_group_tags;
        return {};
    };
    switch (dim) {
    case 0:
        return find_tag(ents.points);
    case 1:
        return find_tag(ents.curves);
    case 2:
        return find_tag(ents.surfaces);
    case 3:
        return find_tag(ents.volumes);
    default:
        return {};
    }
}

const mshio::PhysicalGroup *MshData::find_physical_group(int dim,
                                                         int tag) const {
    auto it = std::ranges::find_if(
        m_spec->physical_groups,
        [dim, tag](const auto &pg) { return pg.dim == dim && pg.tag == tag; });
    return it != m_spec->physical_groups.end() ? &*it : nullptr;
}

std::vector<size_t> MshData::element_block_indices_for_entity(int dim,
                                                              int tag) const {
    const auto &blocks = m_spec->elements.entity_blocks;
    return std::views::iota(size_t{0}, blocks.size())
           | std::views::filter([&](size_t i) {
                 return blocks[i].entity_dim == dim
                        && blocks[i].entity_tag == tag;
             })
           | std::ranges::to<std::vector>();
}

std::vector<size_t>
    MshData::element_block_indices_for_physical_group(int dim,
                                                      int physical_tag) const {
    const auto &blocks = m_spec->elements.entity_blocks;
    return std::views::iota(size_t{0}, blocks.size())
           | std::views::filter([&](size_t i) {
                 if (blocks[i].entity_dim != dim) return false;
                 auto pg_tags = entity_physical_group_tags(
                     blocks[i].entity_dim, blocks[i].entity_tag);
                 return std::ranges::contains(pg_tags, physical_tag);
             })
           | std::ranges::to<std::vector>();
}

std::vector<int> MshData::entity_tags(int dim) const {
    const auto &ents = m_spec->entities;
    const auto extract_tags = [](const auto &entities) {
        return entities
               | std::views::transform(
                   &std::remove_cvref_t<decltype(entities)>::value_type::tag)
               | std::ranges::to<std::vector>();
    };
    switch (dim) {
    case 0:
        return extract_tags(ents.points);
    case 1:
        return extract_tags(ents.curves);
    case 2:
        return extract_tags(ents.surfaces);
    case 3:
        return extract_tags(ents.volumes);
    default:
        return {};
    }
}

std::vector<int> MshData::physical_group_tags(int dim) const {
    return m_spec->physical_groups
           | std::views::filter([dim](const auto &pg) { return pg.dim == dim; })
           | std::views::transform(&mshio::PhysicalGroup::tag)
           | std::ranges::to<std::vector>();
}

size_t MshData::max_node_tag() const {
    // Use the header value if available.
    if (m_spec->nodes.max_node_tag > 0) return m_spec->nodes.max_node_tag;
    // Otherwise scan all node blocks.
    auto all_tags = m_spec->nodes.entity_blocks
                    | std::views::transform(&mshio::NodeBlock::tags)
                    | std::views::join;
    return std::ranges::fold_left(
        all_tags, size_t{0}, [](size_t acc, size_t t) {
            return std::max(acc, t);
        });
}

std::vector<int> MshData::boundary_entity_tags(int dim, int tag) const {
    const auto &ents = m_spec->entities;
    const auto find_boundary = [tag](const auto &entities,
                                     auto member) -> std::vector<int> {
        auto it = std::ranges::find(
            entities,
            tag,
            &std::remove_cvref_t<decltype(entities)>::value_type::tag);
        if (it != entities.end()) return std::invoke(member, *it);
        return {};
    };
    switch (dim) {
    case 1:
        return find_boundary(ents.curves,
                             &mshio::CurveEntity::boundary_point_tags);
    case 2:
        return find_boundary(ents.surfaces,
                             &mshio::SurfaceEntity::boundary_curve_tags);
    case 3:
        return find_boundary(ents.volumes,
                             &mshio::VolumeEntity::boundary_surface_tags);
    default:
        return {};
    }
}

// ── File I/O ────────────────────────────────────────────────────────────────

auto read_msh(const std::filesystem::path &path)
    -> std::expected<MshData, std::string> {
    if (!std::filesystem::exists(path)) {
        return std::unexpected("file not found: " + path.string());
    }
    try {
        return MshData(mshio::load_msh(path.string()));
    } catch (const std::exception &e) {
        return std::unexpected(std::string(e.what()));
    }
}

auto write_msh(const std::filesystem::path &path, const MshData &data)
    -> std::expected<void, std::string> {
    try {
        mshio::save_msh(path.string(), data.spec());
    } catch (const std::exception &e) {
        return std::unexpected(std::string(e.what()));
    }
    return {};
}

} // namespace balsa::geometry::io
