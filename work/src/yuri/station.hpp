#pragma once

#include <cgra/cgra_mesh.hpp>
#include <glm/glm.hpp>
#include <array>
#include <vector>

#include "lsystem.hpp"

// rendering params for the station
struct Rendering3DParams {
    float solarPanelOffset = 4.5f;
};

// main station class for generator and rendering
class Station {
public:
    // object types for station
    enum class ObjectType {
        MODULE1,
        MODULE2,
        MODULE3,
        JUNCTION,
        SOLAR_PANEL
    };

    Station();
    ~Station();

    Station(const Station&) = delete;
    Station& operator=(const Station&) = delete;

    void regenerate();
    void renderGUI();
    void renderMultiMaterialModel(const glm::mat4& view, const glm::mat4& proj,
        GLuint pbrShader, const glm::vec3& camPos);

    void randomizeMaterials();
    void resetMaterials();

    LSystemParams& getParams() { return m_lsystem.getParams(); }
    Rendering3DParams& getRenderParams() { return m_renderParams; }
    const std::vector<StationModule>& getModules() const { return m_lsystem.getModules(); }
    const std::vector<ModuleJunction>& getJunctions() const { return m_lsystem.getJunctions(); }
    bool shouldDrawStation() const { return m_drawStation; }

private:
    static constexpr int NUM_MATERIALS_PER_OBJECT = 4;
    static constexpr int NUM_PBR_MATERIALS = 6;

    LSystem m_lsystem;

    bool m_drawStation = false;
    bool m_modelsLoaded = false;
    Rendering3DParams m_renderParams;

    // models
    cgra::multi_mesh_model* m_module1 = nullptr;
    cgra::multi_mesh_model* m_module2 = nullptr;
    cgra::multi_mesh_model* m_module3 = nullptr;
    cgra::multi_mesh_model* m_junctionModel = nullptr;
    cgra::multi_mesh_model* m_solarPanelModel = nullptr;

    // materials: 0=gold, 1=plastic, 2=cloth, 3=panel, 4=solar, 5=metal
    std::array<int, NUM_MATERIALS_PER_OBJECT> m_module1Materials;
    std::array<int, NUM_MATERIALS_PER_OBJECT> m_module2Materials;
    std::array<int, NUM_MATERIALS_PER_OBJECT> m_module3Materials;
    std::array<int, NUM_MATERIALS_PER_OBJECT> m_junctionMaterials;
    std::array<int, NUM_MATERIALS_PER_OBJECT> m_solarPanelMaterials;

    void initializeLSystem();
    void loadModuleModels();
    void initializeDefaultMaterials();
    void destroyModels();

    cgra::multi_mesh_model* getModelForLength(float length) const;
    ObjectType getObjectTypeForLength(float length) const;
    int getMaterialIndexForObject(ObjectType objType, size_t materialSlot) const;

    glm::mat4 calculateModuleTransform(const StationModule& module) const;
    glm::mat4 calculateJunctionTransform(const ModuleJunction& junction) const;
    glm::mat4 calculateSolarPanelTransform(const StationModule& module, bool isLeftPanel) const;

    void renderModelWithMaterials(cgra::multi_mesh_model* model, ObjectType objType,
        const glm::mat4& modelTransform, GLuint pbrShader,
        const glm::mat4& view, const glm::mat4& proj,
        const glm::vec3& camPos);

    void renderControlsPanel();
    void renderMaterialControls(const char* objectName,
        std::array<int, NUM_MATERIALS_PER_OBJECT>& materials);
};