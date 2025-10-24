#pragma once

#include <cgra/cgra_mesh.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <array>

#include "lsystem.hpp"

// Forward declaration for multi_mesh_model
namespace cgra {
    struct mesh_group;
    struct multi_mesh_model;
}

struct Rendering3DParams {
    float junctionRadius = 1.0f;
    float gapMultiplier = 0.0f;
    float solarPanelOffset = 4.5f; // How far from center the solar panels are positioned
};

class Station {
public:
    enum class ObjectType {
        MODULE1,
        MODULE2,
        MODULE3,
        JUNCTION,
        SOLAR_PANEL
    };

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

    // Multi-material model rendering (now uses 3 different module sizes + junction + solar panels)
    void renderMultiMaterialModel(const glm::mat4& view, const glm::mat4& proj, GLuint pbrShader, const glm::vec3& camPos);

    // Material management
    void randomizeMaterials();
    void resetMaterials();

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

    // 3D rendering
    bool m_drawStation = false;
    Rendering3DParams m_renderParams;
    cgra::gl_mesh m_junctionMesh;
    bool m_meshNeedsRebuild = true;

    // Multi-material model data - now with 3 module sizes + junction + solar panel
    cgra::multi_mesh_model* m_module1;      // 10 units (10x5x5)
    cgra::multi_mesh_model* m_module2;      // 20 units (20x5x5)
    cgra::multi_mesh_model* m_module3;      // 30 units (30x5x5)
    cgra::multi_mesh_model* m_junctionModel; // 5.9x5.9x5.9
    cgra::multi_mesh_model* m_solarPanelModel; // 4x2x0.2
    bool m_modelsLoaded = false;

    // Material assignments - each object has 4 material slots
    // Each slot can be assigned to one of 3 PBR materials (0=gold, 1=plastic, 2=cloth)
    std::array<int, 4> m_module1Materials;
    std::array<int, 4> m_module2Materials;
    std::array<int, 4> m_module3Materials;
    std::array<int, 4> m_junctionMaterials;
    std::array<int, 4> m_solarPanelMaterials;

    // Model loading and material assignment
    void loadModuleModels();
    void initializeDefaultMaterials();
    void destroyModels();
    int getMaterialIndexForObject(ObjectType objType, size_t materialSlot) const;

    // Get the appropriate model based on module length
    cgra::multi_mesh_model* getModelForLength(float length) const;
    ObjectType getObjectTypeForLength(float length) const;

    // Calculate transforms for modules, junctions, and solar panels
    glm::mat4 calculateModuleTransform(const StationModule& module) const;
    glm::mat4 calculateJunctionTransform(const ModuleJunction& junction) const;
    glm::mat4 calculateSolarPanelTransform(const StationModule& module, bool isLeftPanel) const;

    // Render a model with PBR materials
    void renderModelWithMaterials(cgra::multi_mesh_model* model, ObjectType objType,
        const glm::mat4& modelTransform, GLuint pbrShader,
        const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camPos);

    // GUI
    void applyUIStyle();
    void renderControlsPanel();
    void renderMaterialControls(const char* objectName, std::array<int, 4>& materials);

    // 3D rendering helpers
    void rebuildMeshes();

    bool m_showModelButton = false;
};