#include "balsa/visualization/vulkan/mesh_scene.hpp"
#include "balsa/visualization/vulkan/mesh_drawable.hpp"
#include "balsa/visualization/vulkan/film.hpp"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <spdlog/spdlog.h>

namespace balsa::visualization::vulkan {

// ── Constructor / Destructor ─────────────────────────────────────────

MeshScene::MeshScene() = default;

MeshScene::~MeshScene() {
    release_vulkan_resources();
}

// ── SceneBase overrides ──────────────────────────────────────────────

void MeshScene::initialize(Film &film) {
    SceneBase::initialize(film);
    _film = &film;

    _pipeline_manager.init(film);
    _initialized = true;

    // Init any drawables that were added before initialize() was called.
    for (auto &d : _drawables) {
        if (d && !d->is_initialized()) {
            d->init(film, _pipeline_manager);
        }
    }

    spdlog::info("MeshScene initialized with {} drawable(s)", _drawables.size());
}

void MeshScene::draw(Film &film) {
    if (!_initialized) return;

    for (auto &d : _drawables) {
        if (!d || !d->visible) continue;
        d->update_ubos(_view, _projection);
        d->draw(film, _pipeline_manager);
    }
}

void MeshScene::release_vulkan_resources() {
    if (!_initialized) return;

    spdlog::debug("MeshScene: releasing Vulkan resources ({} drawables)", _drawables.size());

    // Release drawables first (they reference pipeline manager's descriptor pool).
    for (auto &d : _drawables) {
        if (d) d->release();
    }

    // Then release the pipeline manager (destroys pool, layouts, cached pipelines).
    _pipeline_manager.release();

    _film = nullptr;
    _initialized = false;
}

// ── Drawable management ──────────────────────────────────────────────

MeshDrawable *MeshScene::add_drawable(std::unique_ptr<MeshDrawable> drawable) {
    if (!drawable) return nullptr;

    MeshDrawable *ptr = drawable.get();

    // If the scene is already initialized, init the drawable now.
    if (_initialized && _film && !ptr->is_initialized()) {
        ptr->init(*_film, _pipeline_manager);
    }

    _drawables.push_back(std::move(drawable));
    return ptr;
}

MeshDrawable *MeshScene::add_drawable(const std::string &name) {
    auto d = std::make_unique<MeshDrawable>();
    d->name = name;
    return add_drawable(std::move(d));
}

bool MeshScene::remove_drawable(const MeshDrawable *drawable) {
    auto it = std::find_if(_drawables.begin(), _drawables.end(), [drawable](const auto &p) { return p.get() == drawable; });
    if (it == _drawables.end()) return false;

    if (*it) (*it)->release();
    _drawables.erase(it);
    return true;
}

bool MeshScene::remove_drawable(std::size_t index) {
    if (index >= _drawables.size()) return false;

    if (_drawables[index]) _drawables[index]->release();
    _drawables.erase(_drawables.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

MeshDrawable *MeshScene::drawable(std::size_t index) {
    return index < _drawables.size() ? _drawables[index].get() : nullptr;
}

const MeshDrawable *MeshScene::drawable(std::size_t index) const {
    return index < _drawables.size() ? _drawables[index].get() : nullptr;
}

// ── Camera helpers ───────────────────────────────────────────────────

void MeshScene::look_at(const glm::vec3 &eye, const glm::vec3 &center, const glm::vec3 &up) {
    _view = glm::lookAt(eye, center, up);
}

void MeshScene::set_perspective(float fov_y, float aspect, float near, float far) {
    _fov_y = fov_y;
    _near = near;
    _far = far;
    _projection = glm::perspective(fov_y, aspect, near, far);

    // Vulkan clip space has Y inverted compared to OpenGL.
    _projection[1][1] *= -1.0f;
}

void MeshScene::update_aspect(float aspect) {
    _projection = glm::perspective(_fov_y, aspect, _near, _far);
    _projection[1][1] *= -1.0f;
}

}// namespace balsa::visualization::vulkan
