#if !defined(BALSA_VISUALIZATION_VULKAN_MESH_SCENE_HPP)
#define BALSA_VISUALIZATION_VULKAN_MESH_SCENE_HPP

#include <memory>
#include <string>
#include <vector>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "balsa/visualization/vulkan/scene_base.hpp"
#include "balsa/visualization/vulkan/mesh_pipeline.hpp"

namespace balsa::visualization::vulkan {

class MeshDrawable;

// ── MeshScene ────────────────────────────────────────────────────────
//
// SceneBase subclass that manages a collection of MeshDrawable objects
// and a MeshPipelineManager.  Provides camera control (view/projection
// matrices) and orchestrates the per-frame update→draw loop.
//
// Owns:
//   - MeshPipelineManager (shared by all drawables for descriptor sets
//     and pipeline caching)
//   - vector<unique_ptr<MeshDrawable>> (the drawable list)
//   - View and projection matrices
//
// Lifecycle:
//   1. Construct MeshScene
//   2. Call initialize(film) — creates pipeline manager, inits drawables
//   3. Each frame: Window calls begin_render_pass → draw → end_render_pass
//   4. On teardown: release_vulkan_resources() destroys everything
//
// Thread-safety: NOT thread-safe — call only from the rendering thread.

class MeshScene : public SceneBase {
  public:
    MeshScene();
    ~MeshScene() override;

    // Non-copyable, non-movable (holds Vulkan resources + raw Film pointer)
    MeshScene(const MeshScene &) = delete;
    MeshScene &operator=(const MeshScene &) = delete;
    MeshScene(MeshScene &&) = delete;
    MeshScene &operator=(MeshScene &&) = delete;

    // ── SceneBase overrides ─────────────────────────────────────────

    // Initialize the pipeline manager and any already-added drawables.
    void initialize(Film &film) override;

    // Draw all visible drawables.
    void draw(Film &film) override;

    // Destroy all Vulkan resources (pipeline manager, drawables, UBOs).
    // Idempotent — safe to call from both the renderer and destructor.
    void release_vulkan_resources() override;

    // ── Drawable management ─────────────────────────────────────────

    // Add a drawable.  If the scene is already initialized, the drawable
    // is immediately init'd with the Film.  Returns a non-owning pointer
    // for the caller to configure (upload buffers, set render state, etc.).
    MeshDrawable *add_drawable(std::unique_ptr<MeshDrawable> drawable);

    // Convenience: create a new MeshDrawable with the given name.
    MeshDrawable *add_drawable(const std::string &name = "Mesh");

    // Remove a drawable by pointer.  Returns true if found and removed.
    bool remove_drawable(const MeshDrawable *drawable);

    // Remove a drawable by index.  Returns true if index was valid.
    bool remove_drawable(std::size_t index);

    // Number of drawables.
    std::size_t drawable_count() const { return _drawables.size(); }

    // Access drawables (for UI iteration).
    MeshDrawable *drawable(std::size_t index);
    const MeshDrawable *drawable(std::size_t index) const;

    // ── Camera ──────────────────────────────────────────────────────

    // Set view matrix directly.
    void set_view(const glm::mat4 &view) { _view = view; }

    // Set projection matrix directly.
    void set_projection(const glm::mat4 &projection) { _projection = projection; }

    // Convenience: set view via look_at parameters.
    void look_at(const glm::vec3 &eye, const glm::vec3 &center, const glm::vec3 &up);

    // Convenience: set perspective projection.
    // fov_y in radians, aspect = width/height.
    void set_perspective(float fov_y, float aspect, float near, float far);

    // Convenience: update aspect ratio (e.g. on window resize), keeping
    // the current fov, near, far.
    void update_aspect(float aspect);

    const glm::mat4 &view() const { return _view; }
    const glm::mat4 &projection() const { return _projection; }

    // ── Pipeline manager access (for advanced use) ──────────────────

    MeshPipelineManager &pipeline_manager() { return _pipeline_manager; }
    const MeshPipelineManager &pipeline_manager() const { return _pipeline_manager; }

  private:
    MeshPipelineManager _pipeline_manager;
    std::vector<std::unique_ptr<MeshDrawable>> _drawables;

    glm::mat4 _view = glm::mat4(1.0f);
    glm::mat4 _projection = glm::mat4(1.0f);

    // Cached perspective parameters for update_aspect().
    float _fov_y = 0.7854f;// ~45 degrees
    float _near = 0.01f;
    float _far = 100.0f;

    Film *_film = nullptr;
    bool _initialized = false;
};

}// namespace balsa::visualization::vulkan

#endif
