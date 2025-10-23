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
    float moduleSpacing = 2.0f; // Spacing between every module and junction
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

    // Multi-material model rendering (now uses 3 different module sizes + junction)
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

    // Multi-material model data - now with 3 module sizes + junction
    cgra::multi_mesh_model* m_module1;      // 10 units (10x5x5)
    cgra::multi_mesh_model* m_module2;      // 20 units (20x5x5)
    cgra::multi_mesh_model* m_module3;      // 30 units (30x5x5)
    cgra::multi_mesh_model* m_junctionModel; // 5.9x5.9x5.9
    std::vector<int> m_materialAssignments; // Cyclical pattern: 0,1,2,0,1,2...
    bool m_modelsLoaded = false;

    // Model loading and material assignment
    void loadModuleModels();
    void assignCyclicalMaterials();
    void destroyModels();
    int getMaterialIndexForGroup(size_t groupIndex) const;

    // Get the appropriate model based on module length
    cgra::multi_mesh_model* getModelForLength(float length) const;

    // Calculate transforms for modules and junctions with proper spacing
    glm::mat4 calculateModuleTransform(const StationModule& module) const;
    glm::mat4 calculateJunctionTransform(const ModuleJunction& junction) const;

    // Helper to calculate cumulative spacing offset for a module
    float calculateCumulativeSpacing(const StationModule& module) const;

    // Render a model with PBR materials
    void renderModelWithMaterials(cgra::multi_mesh_model* model, const glm::mat4& modelTransform,
        GLuint pbrShader, const glm::mat4& view, const glm::mat4& proj,
        const glm::vec3& camPos);

    // GUI
    void applyUIStyle();
    void renderControlsPanel();
    void renderPreviewPanel();
    void drawVisualization();
    void calculateBounds(glm::vec2& minBounds, glm::vec2& maxBounds) const;

    // 3D rendering helpers
    void rebuildMeshes();
    glm::vec3 getModuleColor(int moduleType) const;

    bool m_showModelButton = false;
};