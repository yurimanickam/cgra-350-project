#include "station.hpp"
#include <algorithm>
#include <cmath>
#include <random>
#include <iostream>

#include <cgra/cgra_mesh.hpp>
#include <cgra/cgra_geometry.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

#include "yuri/objloader.hpp"
#include "matt/pbr.hpp"

using namespace glm;

namespace {
    constexpr float HALF_PI = glm::half_pi<float>();
    constexpr float PI = glm::pi<float>();
    constexpr float JUNCTION_RADIUS = 2.95f;
    const char* MATERIAL_NAMES[] = { "Gold", "Plastic", "Cloth", "Panel", "Solar", "Metal" };

    // color stuff for ui
    const ImVec4 COLOR_HEADER = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    const ImVec4 COLOR_BUTTON_PRIMARY = ImVec4(0.2f, 0.6f, 0.9f, 1.0f);
    const ImVec4 COLOR_BUTTON_PRIMARY_HOVER = ImVec4(0.3f, 0.7f, 1.0f, 1.0f);
    const ImVec4 COLOR_BUTTON_PRIMARY_ACTIVE = ImVec4(0.1f, 0.5f, 0.8f, 1.0f);
    const ImVec4 COLOR_BUTTON_SUCCESS = ImVec4(0.2f, 0.7f, 0.3f, 1.0f);
    const ImVec4 COLOR_BUTTON_SUCCESS_HOVER = ImVec4(0.3f, 0.8f, 0.4f, 1.0f);
    const ImVec4 COLOR_BUTTON_SUCCESS_ACTIVE = ImVec4(0.1f, 0.6f, 0.2f, 1.0f);
    const ImVec4 COLOR_BUTTON_DANGER = ImVec4(0.7f, 0.3f, 0.2f, 1.0f);
    const ImVec4 COLOR_BUTTON_DANGER_HOVER = ImVec4(0.8f, 0.4f, 0.3f, 1.0f);
    const ImVec4 COLOR_BUTTON_DANGER_ACTIVE = ImVec4(0.6f, 0.2f, 0.1f, 1.0f);
    const ImVec4 COLOR_BUTTON_WARNING = ImVec4(0.9f, 0.6f, 0.2f, 1.0f);
    const ImVec4 COLOR_BUTTON_WARNING_HOVER = ImVec4(1.0f, 0.7f, 0.3f, 1.0f);
    const ImVec4 COLOR_BUTTON_WARNING_ACTIVE = ImVec4(0.8f, 0.5f, 0.1f, 1.0f);
    const ImVec4 COLOR_BUTTON_INACTIVE = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
    const ImVec4 COLOR_BUTTON_INACTIVE_HOVER = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
    const ImVec4 COLOR_BUTTON_INACTIVE_ACTIVE = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);

    // set button colors
    void PushButtonColors(const ImVec4& base, const ImVec4& hover, const ImVec4& active) {
        ImGui::PushStyleColor(ImGuiCol_Button, base);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
    }

    // load a 3d model
    cgra::multi_mesh_model* LoadModel(const std::string& filename) {
        const std::string basePath = CGRA_SRCDIR + std::string("/res/assets/");
        auto* model = new cgra::multi_mesh_model();
        *model = cgra::loadMultiModel(basePath + filename);
        return model;
    }

    // kill a model
    void DestroyModel(cgra::multi_mesh_model*& model) {
        if (model) {
            model->destroy();
            delete model;
            model = nullptr;
        }
    }
}

// constructor
Station::Station() {
    initializeLSystem();
    initializeDefaultMaterials();
    loadModuleModels();
}

// destructor
Station::~Station() {
    destroyModels();
}

// kill all models
void Station::destroyModels() {
    DestroyModel(m_module1);
    DestroyModel(m_module2);
    DestroyModel(m_module3);
    DestroyModel(m_junctionModel);
    DestroyModel(m_solarPanelModel);
}

// set default materials for stuff
void Station::initializeDefaultMaterials() {
    m_module1Materials = { 5, 1, 2, 3 };
    m_module2Materials = { 5, 1, 2, 3 };
    m_module3Materials = { 5, 1, 2, 3 };
    m_junctionMaterials = { 1, 0, 2, 4 };
    m_solarPanelMaterials = { 4, 0, 0, 0 };
}

// random materials for everything
void Station::randomizeMaterials() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, NUM_PBR_MATERIALS - 1);

    auto randomizeArray = [&](std::array<int, NUM_MATERIALS_PER_OBJECT>& materials) {
        for (int& material : materials) {
            material = dis(gen);
        }
        };

    randomizeArray(m_module1Materials);
    randomizeArray(m_module2Materials);
    randomizeArray(m_module3Materials);
    randomizeArray(m_junctionMaterials);
    randomizeArray(m_solarPanelMaterials);

    std::cout << "Station: Randomized all material assignments\n";
}

// put materials back to default
void Station::resetMaterials() {
    initializeDefaultMaterials();
    std::cout << "Station: Reset all material assignments to default\n";
}

// load models for modules
void Station::loadModuleModels() {
    destroyModels();

    m_module1 = LoadModel("module1.obj");
    m_module2 = LoadModel("module2.obj");
    m_module3 = LoadModel("module3.obj");
    m_junctionModel = LoadModel("junction.obj");
    m_solarPanelModel = LoadModel("solarPanel2.obj");

    m_modelsLoaded = !m_module1->mesh_groups.empty() &&
        !m_module2->mesh_groups.empty() &&
        !m_module3->mesh_groups.empty() &&
        !m_junctionModel->mesh_groups.empty() &&
        !m_solarPanelModel->mesh_groups.empty();

    std::cout << "Station: " << (m_modelsLoaded ? "All models loaded successfully" : "Failed to load one or more models") << "\n";
}

// get material index for the object type
int Station::getMaterialIndexForObject(ObjectType objType, size_t materialSlot) const {
    if (materialSlot >= NUM_MATERIALS_PER_OBJECT) {
        return 1;
    }

    switch (objType) {
    case ObjectType::MODULE1:     return m_module1Materials[materialSlot];
    case ObjectType::MODULE2:     return m_module2Materials[materialSlot];
    case ObjectType::MODULE3:     return m_module3Materials[materialSlot];
    case ObjectType::JUNCTION:    return m_junctionMaterials[materialSlot];
    case ObjectType::SOLAR_PANEL: return m_solarPanelMaterials[materialSlot];
    default:                      return 1;
    }
}

// pick model for a given length
cgra::multi_mesh_model* Station::getModelForLength(float length) const {
    if (length <= 15.0f) return m_module1;
    if (length <= 25.0f) return m_module2;
    return m_module3;
}

// pick object type for a given length
Station::ObjectType Station::getObjectTypeForLength(float length) const {
    if (length <= 15.0f) return ObjectType::MODULE1;
    if (length <= 25.0f) return ObjectType::MODULE2;
    return ObjectType::MODULE3;
}

// calc transform for module
glm::mat4 Station::calculateModuleTransform(const StationModule& module) const {
    const auto& junctions = m_lsystem.getJunctions();

    if (module.isVertical) {
        float endY = module.verticalOffset;
        for (const auto& junction : junctions) {
            if (length(junction.position - module.startPos) < 0.1f &&
                std::abs(junction.verticalOffset - module.verticalOffset) > 0.1f) {
                endY = junction.verticalOffset;
                break;
            }
        }

        bool pointingUp = endY > module.verticalOffset;
        vec3 position(module.startPos.x, module.verticalOffset, module.startPos.y);

        float centerOffset = (module.length / 2.0f) + JUNCTION_RADIUS;
        position.y += pointingUp ? centerOffset : -centerOffset;

        mat4 transform = translate(mat4(1.0f), position);
        transform = rotate(transform, pointingUp ? -HALF_PI : HALF_PI, vec3(0.0f, 0.0f, 1.0f));

        return transform;
    }
    else {
        vec3 fromPos3D(module.startPos.x, 0.0f, module.startPos.y);
        vec3 toPos3D(module.endPos.x, 0.0f, module.endPos.y);
        vec3 direction = normalize(toPos3D - fromPos3D);

        float centerOffset = (module.length / 2.0f) + JUNCTION_RADIUS;
        vec3 position = fromPos3D + direction * centerOffset;

        mat4 transform = translate(mat4(1.0f), position);
        float angle = atan2(direction.z, direction.x);
        transform = rotate(transform, angle, vec3(0.0f, 1.0f, 0.0f));

        return transform;
    }
}

// calc junction transform
glm::mat4 Station::calculateJunctionTransform(const ModuleJunction& junction) const {
    vec3 position(junction.position.x, junction.verticalOffset, junction.position.y);
    return translate(mat4(1.0f), position);
}

// calc solar panel transform
glm::mat4 Station::calculateSolarPanelTransform(const StationModule& module, bool isLeftPanel) const {
    const auto& junctions = m_lsystem.getJunctions();

    if (module.isVertical) {
        float endY = module.verticalOffset;
        for (const auto& junction : junctions) {
            if (length(junction.position - module.startPos) < 0.1f &&
                std::abs(junction.verticalOffset - module.verticalOffset) > 0.1f) {
                endY = junction.verticalOffset;
                break;
            }
        }

        bool pointingUp = endY > module.verticalOffset;
        vec3 centerPos(module.startPos.x, module.verticalOffset, module.startPos.y);

        float centerOffset = (module.length / 2.0f) + JUNCTION_RADIUS;
        centerPos.y += pointingUp ? centerOffset : -centerOffset;

        mat4 transform = translate(mat4(1.0f), centerPos);
        transform = rotate(transform, pointingUp ? -HALF_PI : HALF_PI, vec3(0.0f, 0.0f, 1.0f));

        float sideOffset = isLeftPanel ? -m_renderParams.solarPanelOffset : m_renderParams.solarPanelOffset;
        transform = translate(transform, vec3(0.0f, 0.0f, sideOffset));

        if (isLeftPanel) {
            transform = rotate(transform, PI, vec3(0.0f, 1.0f, 0.0f));
        }

        return transform;
    }
    else {
        vec3 fromPos3D(module.startPos.x, 0.0f, module.startPos.y);
        vec3 toPos3D(module.endPos.x, 0.0f, module.endPos.y);
        vec3 direction = normalize(toPos3D - fromPos3D);

        float centerOffset = (module.length / 2.0f) + JUNCTION_RADIUS;
        vec3 centerPos = fromPos3D + direction * centerOffset;

        mat4 transform = translate(mat4(1.0f), centerPos);
        float angle = atan2(direction.z, direction.x);
        transform = rotate(transform, angle, vec3(0.0f, 1.0f, 0.0f));

        float sideOffset = isLeftPanel ? -m_renderParams.solarPanelOffset : m_renderParams.solarPanelOffset;
        transform = translate(transform, vec3(0.0f, 0.0f, sideOffset));

        if (isLeftPanel) {
            transform = rotate(transform, PI, vec3(0.0f, 1.0f, 0.0f));
        }

        return transform;
    }
}

// draw a model with materials
void Station::renderModelWithMaterials(cgra::multi_mesh_model* model, ObjectType objType,
    const glm::mat4& modelTransform, GLuint pbrShader,
    const glm::mat4& view, const glm::mat4& proj,
    const glm::vec3& camPos) {
    if (!model || model->mesh_groups.empty()) return;

    glUseProgram(pbrShader);
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "projection"), 1, GL_FALSE, value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "view"), 1, GL_FALSE, value_ptr(view));
    glUniform3fv(glGetUniformLocation(pbrShader, "camPos"), 1, value_ptr(camPos));
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "model"), 1, GL_FALSE, value_ptr(modelTransform));
    glUniformMatrix3fv(glGetUniformLocation(pbrShader, "normalMatrix"), 1, GL_FALSE,
        value_ptr(transpose(inverse(mat3(modelTransform)))));

    // draw each mesh group
    for (size_t i = 0; i < model->mesh_groups.size() && i < NUM_MATERIALS_PER_OBJECT; ++i) {
        int materialIndex = getMaterialIndexForObject(objType, i);

        switch (materialIndex) {
        case 0: bindPBRTextures(gold); break;
        case 1: bindPBRTextures(plastic); break;
        case 2: bindPBRTextures(cloth); break;
        case 3: bindPBRTextures(panel); break;
        case 4: bindPBRTextures(solar); break;
        case 5: bindPBRTextures(metal); break;
        default: bindPBRTextures(plastic); break;
        }

        model->mesh_groups[i].mesh.draw();
    }
}

// draw the full station
void Station::renderMultiMaterialModel(const glm::mat4& view, const glm::mat4& proj,
    GLuint pbrShader, const glm::vec3& camPos) {
    if (!m_drawStation || !m_modelsLoaded) {
        return;
    }

    const auto& modules = m_lsystem.getModules();
    const auto& junctions = m_lsystem.getJunctions();

    // draw modules + solar panels
    for (const auto& module : modules) {
        cgra::multi_mesh_model* model = getModelForLength(module.length);
        ObjectType objType = getObjectTypeForLength(module.length);
        mat4 transform = calculateModuleTransform(module);
        renderModelWithMaterials(model, objType, transform, pbrShader, view, proj, camPos);

        if (module.hasSolarPanels && m_solarPanelModel) {
            mat4 leftPanelTransform = calculateSolarPanelTransform(module, true);
            renderModelWithMaterials(m_solarPanelModel, ObjectType::SOLAR_PANEL,
                leftPanelTransform, pbrShader, view, proj, camPos);

            mat4 rightPanelTransform = calculateSolarPanelTransform(module, false);
            renderModelWithMaterials(m_solarPanelModel, ObjectType::SOLAR_PANEL,
                rightPanelTransform, pbrShader, view, proj, camPos);
        }
    }

    // draw junctions
    for (const auto& junction : junctions) {
        mat4 transform = calculateJunctionTransform(junction);
        renderModelWithMaterials(m_junctionModel, ObjectType::JUNCTION,
            transform, pbrShader, view, proj, camPos);
    }
}

// init the lsystem
void Station::initializeLSystem() {
    m_lsystem.initialize();
}

// regen lsystem
void Station::regenerate() {
    m_lsystem.regenerate();
}

// show material controls for an object
void Station::renderMaterialControls(const char* objectName,
    std::array<int, NUM_MATERIALS_PER_OBJECT>& materials) {
    ImGui::PushID(objectName);

    if (ImGui::TreeNode(objectName)) {
        ImGui::Spacing();

        for (int i = 0; i < NUM_MATERIALS_PER_OBJECT; ++i) {
            ImGui::PushID(i);
            ImGui::Text("Material Slot %d", i);
            ImGui::SameLine(150);
            ImGui::PushItemWidth(120);
            ImGui::Combo(("##mat" + std::to_string(i)).c_str(), &materials[i],
                MATERIAL_NAMES, NUM_PBR_MATERIALS);
            ImGui::PopItemWidth();
            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::TreePop();
    }

    ImGui::PopID();
}

// main ui for station controls
void Station::renderControlsPanel() {
    bool needsRegeneration = false;
    LSystemParams& params = m_lsystem.getParams();

    ImGui::PushStyleColor(ImGuiCol_Header, COLOR_HEADER);
    ImGui::Spacing();
    ImGui::Text("L-SYSTEM SPACE STATION GENERATOR");
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // show/hide station
    PushButtonColors(COLOR_BUTTON_PRIMARY, COLOR_BUTTON_PRIMARY_HOVER, COLOR_BUTTON_PRIMARY_ACTIVE);
    if (ImGui::Button(m_drawStation ? "Disable 3D Space Station" : "Enable 3D Space Station", ImVec2(-1, 40))) {
        m_drawStation = !m_drawStation;
    }
    ImGui::PopStyleColor(3);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (!m_drawStation) {
        return;
    }

    // generation params
    if (ImGui::CollapsingHeader("Generation Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();

        ImGui::Text("Complexity");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderInt("##Iterations", &params.iterations, 1, 6);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Number of L-System iterations");
        ImGui::Spacing();

        ImGui::Text("Max Module Size");
        int moduleSizeIndex = params.baseLength <= 15.0f ? 0 : (params.baseLength <= 25.0f ? 1 : 2);
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderInt("##ModuleSize", &moduleSizeIndex, 0, 2,
            moduleSizeIndex == 0 ? "1 (10 units)" : (moduleSizeIndex == 1 ? "2 (20 units)" : "3 (30 units)"))) {
            params.baseLength = moduleSizeIndex == 0 ? 10.0f : (moduleSizeIndex == 1 ? 20.0f : 30.0f);
            needsRegeneration = true;
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Maximum module size");
        ImGui::Spacing();

        ImGui::Text("Decay");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderFloat("##LengthDecay", &params.lengthDecay, 0.5f, 1.0f, "%.2f");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("How much modules shrink each generation");
        ImGui::Spacing();

        ImGui::Text("Seed");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderInt("##Seed", &params.seed, 1, 99999);
        ImGui::PopItemWidth();
        ImGui::Spacing();
    }

    // buttons for toggles
    ImGui::Spacing();
    float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;

    // vertical modules toggle
    PushButtonColors(
        params.allowVerticalModules ? COLOR_BUTTON_SUCCESS : COLOR_BUTTON_INACTIVE,
        params.allowVerticalModules ? COLOR_BUTTON_SUCCESS_HOVER : COLOR_BUTTON_INACTIVE_HOVER,
        params.allowVerticalModules ? COLOR_BUTTON_SUCCESS_ACTIVE : COLOR_BUTTON_INACTIVE_ACTIVE
    );
    if (ImGui::Button(params.allowVerticalModules ? "Vertical Modules ON" : "Vertical Modules OFF",
        ImVec2(buttonWidth, 30))) {
        params.allowVerticalModules = !params.allowVerticalModules;
        needsRegeneration = true;
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    // solar panels toggle
    PushButtonColors(
        params.enableSolarPanels ? COLOR_BUTTON_SUCCESS : COLOR_BUTTON_INACTIVE,
        params.enableSolarPanels ? COLOR_BUTTON_SUCCESS_HOVER : COLOR_BUTTON_INACTIVE_HOVER,
        params.enableSolarPanels ? COLOR_BUTTON_SUCCESS_ACTIVE : COLOR_BUTTON_INACTIVE_ACTIVE
    );
    if (ImGui::Button(params.enableSolarPanels ? "Solar Panels ON" : "Solar Panels OFF",
        ImVec2(buttonWidth, 30))) {
        params.enableSolarPanels = !params.enableSolarPanels;
        needsRegeneration = true;
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();

    // random seed
    PushButtonColors(COLOR_BUTTON_WARNING, COLOR_BUTTON_WARNING_HOVER, COLOR_BUTTON_WARNING_ACTIVE);
    if (ImGui::Button("Randomise Seed", ImVec2(buttonWidth, 30))) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1, 99999);
        params.seed = dis(gen);
        needsRegeneration = true;
    }
    ImGui::PopStyleColor(3);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // model params
    if (ImGui::CollapsingHeader("Model Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();

        if (params.allowVerticalModules) {
            ImGui::Text("Vertical Probability");
            ImGui::PushItemWidth(-1);
            needsRegeneration |= ImGui::SliderFloat("##VertProb", &params.verticalProbability, 0.0f, 1.0f, "%.2f");
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chance of creating a vertical module");
            ImGui::Spacing();
        }

        if (params.enableSolarPanels) {
            ImGui::Text("Panel Probability");
            ImGui::PushItemWidth(-1);
            needsRegeneration |= ImGui::SliderFloat("##SolarProb", &params.solarPanelProbability, 0.0f, 1.0f, "%.2f");
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chance of a module having solar panels");
            ImGui::Spacing();

            ImGui::Text("Offset from Centre");
            ImGui::PushItemWidth(-1);
            ImGui::SliderFloat("##PanelOffset", &m_renderParams.solarPanelOffset, 1.0f, 5.0f, "%.1f units");
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Distance from module center to solar panels");
            ImGui::Spacing();
        }
    }

    // material params
    if (ImGui::CollapsingHeader("Material Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();

        float halfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;

        PushButtonColors(COLOR_BUTTON_SUCCESS, COLOR_BUTTON_SUCCESS_HOVER, COLOR_BUTTON_SUCCESS_ACTIVE);
        if (ImGui::Button("Randomise", ImVec2(halfWidth, 30))) {
            randomizeMaterials();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Randomly assign materials to all slots");
        ImGui::PopStyleColor(3);
        ImGui::SameLine();

        PushButtonColors(COLOR_BUTTON_DANGER, COLOR_BUTTON_DANGER_HOVER, COLOR_BUTTON_DANGER_ACTIVE);
        if (ImGui::Button("Reset", ImVec2(halfWidth, 30))) {
            resetMaterials();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset materials to default");
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (m_modelsLoaded) {
            ImGui::Text("Individual Object Materials:");
            ImGui::Spacing();

            renderMaterialControls("Module 1 (10 units)", m_module1Materials);
            renderMaterialControls("Module 2 (20 units)", m_module2Materials);
            renderMaterialControls("Module 3 (30 units)", m_module3Materials);
            renderMaterialControls("Junction", m_junctionMaterials);
            renderMaterialControls("Solar Panel", m_solarPanelMaterials);
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Load models to configure materials");
        }

        ImGui::Spacing();
    }

    // stats
    if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& modules = m_lsystem.getModules();
        const auto& junctions = m_lsystem.getJunctions();
        const auto& sequence = m_lsystem.getCurrentSequence();

        ImGui::Spacing();
        ImGui::Columns(2, "stats", false);

        int verticalCount = 0;
        int solarPanelCount = 0;
        for (const auto& module : modules) {
            if (module.isVertical) verticalCount++;
            if (module.hasSolarPanels) solarPanelCount++;
        }

        ImGui::Text("Total Modules:"); ImGui::NextColumn();
        ImGui::Text("%zu", modules.size()); ImGui::NextColumn();
        ImGui::Text("Vertical Modules:"); ImGui::NextColumn();
        ImGui::Text("%d", verticalCount); ImGui::NextColumn();
        ImGui::Text("With Solar Panels:"); ImGui::NextColumn();
        ImGui::Text("%d", solarPanelCount); ImGui::NextColumn();
        ImGui::Text("Junctions:"); ImGui::NextColumn();
        ImGui::Text("%zu", junctions.size()); ImGui::NextColumn();
        ImGui::Text("Sequence Length:"); ImGui::NextColumn();
        ImGui::Text("%zu", sequence.length()); ImGui::NextColumn();

        ImGui::Columns(1);
        ImGui::Spacing();
    }

    // obj stats
    if (ImGui::CollapsingHeader("OBJ Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& modules = m_lsystem.getModules();

        ImGui::Spacing();
        ImGui::Columns(2, "obj_stats", false);

        int length10Count = 0;
        int length20Count = 0;
        int length30Count = 0;

        for (const auto& module : modules) {
            if (module.length <= 15.0f) length10Count++;
            else if (module.length <= 25.0f) length20Count++;
            else length30Count++;
        }

        ImGui::Text("10-unit Modules:"); ImGui::NextColumn();
        ImGui::Text("%d", length10Count); ImGui::NextColumn();
        ImGui::Text("20-unit Modules:"); ImGui::NextColumn();
        ImGui::Text("%d", length20Count); ImGui::NextColumn();
        ImGui::Text("30-unit Modules:"); ImGui::NextColumn();
        ImGui::Text("%d", length30Count); ImGui::NextColumn();

        if (m_modelsLoaded) {
            ImGui::Text("Models Status:"); ImGui::NextColumn();
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Loaded"); ImGui::NextColumn();
        }

        ImGui::Columns(1);
        ImGui::Spacing();
    }

    if (needsRegeneration) {
        regenerate();
    }
}

// draw the gui
void Station::renderGUI() {
    ImGui::SetNextWindowPos(ImVec2(5, 540), ImGuiSetCond_Once);
    ImGui::SetNextWindowSize(ImVec2(400, 460), ImGuiSetCond_Once);
    ImGui::Begin("Space Station Generator", nullptr);

    renderControlsPanel();

    ImGui::End();
}