#pragma once

#include <cgra/cgra_mesh.hpp>
#include <glm/glm.hpp>
#include <array>
#include <vector>

#include "lsystem.hpp"

namespace cgra {
    struct multi_mesh_model;
}

/**
 * @brief Parameters controlling the 3D rendering of the station
 */
struct Rendering3DParams {
    float solarPanelOffset = 4.5f;
};

/**
 * @brief Main class for generating and rendering a procedural space station
 *
 * This class manages the L-System generation, 3D model loading, material assignment,
 * and rendering of a modular space station with customizable parameters.
 */
class Station {
public:
    /**
     * @brief Types of objects that can be rendered in the station
     */
    enum class ObjectType {
        MODULE1,      // Small module (10 units)
        MODULE2,      // Medium module (20 units)
        MODULE3,      // Large module (30 units)
        JUNCTION,     // Connection point between modules
        SOLAR_PANEL   // Solar panel attachment
    };

    Station();
    ~Station();

    // Prevent copying
    Station(const Station&) = delete;
    Station& operator=(const Station&) = delete;

    // Core functionality
    void regenerate();
    void renderGUI();
    void renderMultiMaterialModel(const glm::mat4& view, const glm::mat4& proj,
        GLuint pbrShader, const glm::vec3& camPos);

    // Material management
    void randomizeMaterials();
    void resetMaterials();

    // Accessors
    LSystemParams& getParams() { return m_lsystem.getParams(); }
    Rendering3DParams& getRenderParams() { return m_renderParams; }
    const std::vector<StationModule>& getModules() const { return m_lsystem.getModules(); }
    const std::vector<ModuleJunction>& getJunctions() const { return m_lsystem.getJunctions(); }
    bool shouldDrawStation() const { return m_drawStation; }

private:
    static constexpr int NUM_MATERIALS_PER_OBJECT = 4;
    static constexpr int NUM_PBR_MATERIALS = 6;

    // L-System generation
    LSystem m_lsystem;

    // Rendering state
    bool m_drawStation = false;
    bool m_modelsLoaded = false;
    Rendering3DParams m_renderParams;

    // 3D Models
    cgra::multi_mesh_model* m_module1 = nullptr;
    cgra::multi_mesh_model* m_module2 = nullptr;
    cgra::multi_mesh_model* m_module3 = nullptr;
    cgra::multi_mesh_model* m_junctionModel = nullptr;
    cgra::multi_mesh_model* m_solarPanelModel = nullptr;

    // Material assignments (indices into PBR material array)
    // Materials: 0=gold, 1=plastic, 2=cloth, 3=panel, 4=solar, 5=metal
    std::array<int, NUM_MATERIALS_PER_OBJECT> m_module1Materials;
    std::array<int, NUM_MATERIALS_PER_OBJECT> m_module2Materials;
    std::array<int, NUM_MATERIALS_PER_OBJECT> m_module3Materials;
    std::array<int, NUM_MATERIALS_PER_OBJECT> m_junctionMaterials;
    std::array<int, NUM_MATERIALS_PER_OBJECT> m_solarPanelMaterials;

    // Initialization helpers
    void initializeLSystem();
    void loadModuleModels();
    void initializeDefaultMaterials();
    void destroyModels();

    // Model selection helpers
    cgra::multi_mesh_model* getModelForLength(float length) const;
    ObjectType getObjectTypeForLength(float length) const;
    int getMaterialIndexForObject(ObjectType objType, size_t materialSlot) const;

    // Transform calculations
    glm::mat4 calculateModuleTransform(const StationModule& module) const;
    glm::mat4 calculateJunctionTransform(const ModuleJunction& junction) const;
    glm::mat4 calculateSolarPanelTransform(const StationModule& module, bool isLeftPanel) const;

    // Rendering helpers
    void renderModelWithMaterials(cgra::multi_mesh_model* model, ObjectType objType,
        const glm::mat4& modelTransform, GLuint pbrShader,
        const glm::mat4& view, const glm::mat4& proj,
        const glm::vec3& camPos);

    // GUI rendering
    void renderControlsPanel();
    void renderMaterialControls(const char* objectName,
        std::array<int, NUM_MATERIALS_PER_OBJECT>& materials);
};