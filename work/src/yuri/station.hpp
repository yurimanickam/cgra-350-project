#pragma once

#include <cgra/cgra_mesh.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "lsystem.hpp"

// Forward declaration for multi_mesh_model
namespace cgra {
    struct mesh_group;
    struct multi_mesh_model;
}

struct Rendering3DParams {
    float junctionRadius = 1.0f;
    float gapMultiplier = 0.0f;
};

class Station {
public:
    Station();
    ~Station();

    // Mesh generation
    cgra::gl_mesh createSphereMesh(float radius, int stacks, int slices);

    // L-System generation (delegates to LSystem)
    void initializeLSystem();
    void regenerate();

    // GUI rendering
    void renderGUI();

    // 3D rendering
    void render3DStation(const glm::mat4& view, const glm::mat4& proj, GLuint shader);

    // Multi-material model rendering (now used for modules)
    void renderMultiMaterialModel(const glm::mat4& view, const glm::mat4& proj, GLuint pbrShader, const glm::vec3& camPos);

    // Accessors
    LSystemParams& getParams() { return m_lsystem.getParams(); }
    Rendering3DParams& getRenderParams() { return m_renderParams; }
    const std::vector<StationModule>& getModules() const { return m_lsystem.getModules(); }
    const std::vector<ModuleJunction>& getJunctions() const { return m_lsystem.getJunctions(); }
    bool shouldDrawStation() const { return m_drawStation; }
    bool shouldShowModel() const { return m_showModelButton; }

private:
    // L-System (delegated)
    LSystem m_lsystem;

    // Visualization
    float m_previewZoom = 1.0f;
    glm::vec2 m_previewPan = glm::vec2(0.0f);
    bool m_drawStation = false;

    // 3D rendering
    Rendering3DParams m_renderParams;
    cgra::gl_mesh m_junctionMesh;
    bool m_meshNeedsRebuild = true;

    // Multi-material model data
    cgra::multi_mesh_model* m_multiModel;
    std::vector<int> m_materialAssignments; // Cyclical pattern: 0,1,2,0,1,2...
    bool m_modelLoaded = false;

    // Model loading and material assignment
    void loadMultiMaterialModel(const std::string& filepath);
    void assignCyclicalMaterials();
    int getMaterialIndexForGroup(size_t groupIndex) const;

    // GUI
    void applyUIStyle();
    void renderControlsPanel();
    void renderPreviewPanel();
    void drawVisualization();
    void calculateBounds(glm::vec2& minBounds, glm::vec2& maxBounds) const;

    // 3D rendering helpers
    void rebuildMeshes();
    glm::vec3 getModuleColor(int moduleType) const;

    // Helper to get model positions for a module
    std::vector<glm::vec3> getModelPositionsForModule(const StationModule& module) const;
    glm::mat4 calculateModelTransform(const glm::vec3& position, float rotation, bool isVertical, bool pointingUp) const;

    bool m_showModelButton = false;
};