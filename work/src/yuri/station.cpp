#include "station.hpp"
#include <algorithm>
#include <cmath>
#include <cgra/cgra_mesh.hpp>
#include <cgra/cgra_geometry.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <iostream>
#include <random>

#include "yuri/objloader.hpp"
#include "matt/pbr.hpp"

using namespace glm;

namespace {
    constexpr float HALF_PI = glm::half_pi<float>();
    constexpr float TWO_PI = glm::two_pi<float>();
    constexpr float PI = glm::pi<float>();
    constexpr float MODEL_LENGTH = 10.0f;
    constexpr float JUNCTION_SIZE = 5.9f;
    constexpr float JUNCTION_RADIUS = 2.95f; // Half of JUNCTION_SIZE

    // Solar panel dimensions
    constexpr float SOLAR_PANEL_LENGTH = 4.0f;
    constexpr float SOLAR_PANEL_WIDTH = 2.0f;
    constexpr float SOLAR_PANEL_HEIGHT = 0.2f;

    // module colors for preview (kept for reference to materials)
    const ImU32 MODULE_COLORS[] = {
        IM_COL32(200, 200, 200, 255),
        IM_COL32(100, 255, 100, 255),
        IM_COL32(100, 150, 255, 255),
        IM_COL32(255, 220, 100, 255)
    };

    constexpr int NUM_MODULE_TYPES = 4;
    constexpr int NUM_MATERIALS_PER_OBJECT = 4;
    constexpr int NUM_PBR_MATERIALS = 6; // 0=gold, 1=plastic, 2=cloth, 3=panel, 4=solar, 5=metal

    // junction colors
    const ImU32 JUNCTION_COLOR = IM_COL32(150, 150, 160, 255);
    const ImU32 VERTICAL_JUNCTION_COLOR = IM_COL32(255, 150, 100, 255);
    const ImU32 SOLAR_PANEL_COLOR = IM_COL32(50, 100, 255, 255);

    // some ui colors
    const ImVec4 COLOR_HEADER = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    const ImVec4 COLOR_ACTIVE = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    const ImVec4 COLOR_HOVER = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);

    // Material names for UI
    const char* MATERIAL_NAMES[] = { "Gold", "Plastic", "Cloth", "Panel", "Solar", "Metal" };
}

Station::Station()
    : m_module1(nullptr)
    , m_module2(nullptr)
    , m_module3(nullptr)
    , m_junctionModel(nullptr)
    , m_solarPanelModel(nullptr)
    , m_modelsLoaded(false)
{
    initializeLSystem();
    rebuildMeshes();

    // Initialize material assignments with default values
    initializeDefaultMaterials();

    // Load all module models, junction, and solar panel
    loadModuleModels();
}

Station::~Station() {
    destroyModels();
}

void Station::destroyModels() {
    if (m_module1) {
        m_module1->destroy();
        delete m_module1;
        m_module1 = nullptr;
    }
    if (m_module2) {
        m_module2->destroy();
        delete m_module2;
        m_module2 = nullptr;
    }
    if (m_module3) {
        m_module3->destroy();
        delete m_module3;
        m_module3 = nullptr;
    }
    if (m_junctionModel) {
        m_junctionModel->destroy();
        delete m_junctionModel;
        m_junctionModel = nullptr;
    }
    if (m_solarPanelModel) {
        m_solarPanelModel->destroy();
        delete m_solarPanelModel;
        m_solarPanelModel = nullptr;
    }
}

void Station::initializeDefaultMaterials() {
    // Initialize with default pattern: 0,1,2,0
    m_module1Materials = { 5, 1, 2, 3 };
    m_module2Materials = { 5, 1, 2, 3 };
    m_module3Materials = { 5, 1, 2, 3 };
    m_junctionMaterials = { 1, 0, 2, 4 };
    m_solarPanelMaterials = { 4, 0, 0, 0 };
}

void Station::randomizeMaterials() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, NUM_PBR_MATERIALS - 1);

    // Randomize all material slots for all objects
    for (int i = 0; i < NUM_MATERIALS_PER_OBJECT; ++i) {
        m_module1Materials[i] = dis(gen);
        m_module2Materials[i] = dis(gen);
        m_module3Materials[i] = dis(gen);
        m_junctionMaterials[i] = dis(gen);
        m_solarPanelMaterials[i] = dis(gen);
    }

    std::cout << "Station: Randomized all material assignments" << std::endl;
}

void Station::resetMaterials() {
    initializeDefaultMaterials();
    std::cout << "Station: Reset all material assignments to default" << std::endl;
}

void Station::loadModuleModels() {
    destroyModels();

    // Load the three module sizes
    m_module1 = new cgra::multi_mesh_model();
    *m_module1 = cgra::load_multi_mesh_model(CGRA_SRCDIR + std::string("/res/assets/module1.obj"));

    m_module2 = new cgra::multi_mesh_model();
    *m_module2 = cgra::load_multi_mesh_model(CGRA_SRCDIR + std::string("/res/assets/module2.obj"));

    m_module3 = new cgra::multi_mesh_model();
    *m_module3 = cgra::load_multi_mesh_model(CGRA_SRCDIR + std::string("/res/assets/module3.obj"));

    // Load junction model
    m_junctionModel = new cgra::multi_mesh_model();
    *m_junctionModel = cgra::load_multi_mesh_model(CGRA_SRCDIR + std::string("/res/assets/junction.obj"));

    // Load solar panel model
    m_solarPanelModel = new cgra::multi_mesh_model();
    *m_solarPanelModel = cgra::load_multi_mesh_model(CGRA_SRCDIR + std::string("/res/assets/solarPanel2.obj"));

    if (!m_module1->mesh_groups.empty() &&
        !m_module2->mesh_groups.empty() &&
        !m_module3->mesh_groups.empty() &&
        !m_junctionModel->mesh_groups.empty() &&
        !m_solarPanelModel->mesh_groups.empty()) {
        m_modelsLoaded = true;
        std::cout << "Station: All module models, junction, and solar panel loaded successfully" << std::endl;
    }
    else {
        m_modelsLoaded = false;
        std::cout << "Station: Failed to load one or more models" << std::endl;
    }
}

int Station::getMaterialIndexForObject(ObjectType objType, size_t materialSlot) const {
    if (materialSlot >= NUM_MATERIALS_PER_OBJECT) {
        return 1; // Default to plastic
    }

    switch (objType) {
    case ObjectType::MODULE1:
        return m_module1Materials[materialSlot];
    case ObjectType::MODULE2:
        return m_module2Materials[materialSlot];
    case ObjectType::MODULE3:
        return m_module3Materials[materialSlot];
    case ObjectType::JUNCTION:
        return m_junctionMaterials[materialSlot];
    case ObjectType::SOLAR_PANEL:
        return m_solarPanelMaterials[materialSlot];
    default:
        return 1; // Default to plastic
    }
}

cgra::multi_mesh_model* Station::getModelForLength(float length) const {
    if (length <= 15.0f) return m_module1;      // 10 units
    else if (length <= 25.0f) return m_module2; // 20 units
    else return m_module3;                       // 30 units
}

Station::ObjectType Station::getObjectTypeForLength(float length) const {
    if (length <= 15.0f) return ObjectType::MODULE1;
    else if (length <= 25.0f) return ObjectType::MODULE2;
    else return ObjectType::MODULE3;
}

glm::mat4 Station::calculateModuleTransform(const StationModule& module) const {
    const auto& junctions = m_lsystem.getJunctions();

    if (module.isVertical) {
        // Find the target vertical junction
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

        // Calculate center position accounting for junction radius
        float moduleLength = module.length;
        float centerOffset = (moduleLength / 2.0f) + JUNCTION_RADIUS;
        position.y += pointingUp ? centerOffset : -centerOffset;

        // Start with translation
        mat4 transform = translate(mat4(1.0f), position);

        // Rotate to point vertically
        if (pointingUp) {
            // Point up: rotate -90 around Z axis
            transform = rotate(transform, -HALF_PI, vec3(0.0f, 0.0f, 1.0f));
        }
        else {
            // Point down: rotate 90 around Z axis
            transform = rotate(transform, HALF_PI, vec3(0.0f, 0.0f, 1.0f));
        }

        return transform;
    }
    else {
        // Horizontal module
        vec3 fromPos3D(module.startPos.x, 0.0f, module.startPos.y);
        vec3 toPos3D(module.endPos.x, 0.0f, module.endPos.y);

        // Calculate direction and center position
        vec3 direction = normalize(toPos3D - fromPos3D);
        float moduleLength = module.length;

        // The module center is offset by: (module_length / 2) + junction_radius
        float centerOffset = (moduleLength / 2.0f) + JUNCTION_RADIUS;
        vec3 position = fromPos3D + direction * centerOffset;

        // Start with translation
        mat4 transform = translate(mat4(1.0f), position);

        // Calculate rotation angle from direction
        // The modules point along +X axis by default
        float angle = atan2(direction.z, direction.x);

        // Rotate around Y axis to align with direction
        transform = rotate(transform, angle, vec3(0.0f, 1.0f, 0.0f));

        return transform;
    }
}

glm::mat4 Station::calculateJunctionTransform(const ModuleJunction& junction) const {
    vec3 position(junction.position.x, junction.verticalOffset, junction.position.y);
    return translate(mat4(1.0f), position);
}

glm::mat4 Station::calculateSolarPanelTransform(const StationModule& module, bool isLeftPanel) const {
    const auto& junctions = m_lsystem.getJunctions();

    if (module.isVertical) {
        // Find the target vertical junction to determine direction
        float endY = module.verticalOffset;
        for (const auto& junction : junctions) {
            if (length(junction.position - module.startPos) < 0.1f &&
                std::abs(junction.verticalOffset - module.verticalOffset) > 0.1f) {
                endY = junction.verticalOffset;
                break;
            }
        }

        bool pointingUp = endY > module.verticalOffset;
        glm::vec3 centerPos(module.startPos.x, module.verticalOffset, module.startPos.y);
        float moduleLength = module.length;
        float centerOffset = (moduleLength / 2.0f) + JUNCTION_RADIUS;
        centerPos.y += pointingUp ? centerOffset : -centerOffset;

        // Start with translation to center position
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), centerPos);

        // Rotate the module coordinate system to point vertically
        if (pointingUp) {
            // Module points up: rotate -90° around Z axis
            transform = glm::rotate(transform, -HALF_PI, glm::vec3(0.0f, 0.0f, 1.0f));
        }
        else {
            // Module points down: rotate 90° around Z axis
            transform = glm::rotate(transform, HALF_PI, glm::vec3(0.0f, 0.0f, 1.0f));
        }

        // Offset perpendicular to the module axis (along local Z)
        float sideOffset = isLeftPanel ? -m_renderParams.solarPanelOffset : m_renderParams.solarPanelOffset;
        transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, sideOffset));

        // Rotate panels to face inward (toward the module)
        if (isLeftPanel) {
            transform = glm::rotate(transform, PI, glm::vec3(0.0f, 1.0f, 0.0f));
        }

        return transform;
    }
    else {
        // Horizontal module
        glm::vec3 fromPos3D(module.startPos.x, 0.0f, module.startPos.y);
        glm::vec3 toPos3D(module.endPos.x, 0.0f, module.endPos.y);

        glm::vec3 direction = glm::normalize(toPos3D - fromPos3D);
        float moduleLength = module.length;
        float centerOffset = (moduleLength / 2.0f) + JUNCTION_RADIUS;
        glm::vec3 centerPos = fromPos3D + direction * centerOffset;

        // Start with translation to center position
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), centerPos);

        // Rotate to align with module direction
        float angle = atan2(direction.z, direction.x);
        transform = glm::rotate(transform, angle, glm::vec3(0.0f, 1.0f, 0.0f));

        // Offset the solar panel perpendicular to the module
        float sideOffset = isLeftPanel ? -m_renderParams.solarPanelOffset : m_renderParams.solarPanelOffset;
        transform = glm::translate(transform, glm::vec3(0.0f, 0.0f, sideOffset));

        // Rotate panels to face inward (toward the module)
        if (isLeftPanel) {
            transform = glm::rotate(transform, PI, glm::vec3(0.0f, 1.0f, 0.0f));
        }

        return transform;
    }
}

void Station::renderModelWithMaterials(cgra::multi_mesh_model* model, ObjectType objType,
    const glm::mat4& modelTransform, GLuint pbrShader,
    const glm::mat4& view, const glm::mat4& proj, const glm::vec3& camPos) {
    if (!model || model->mesh_groups.empty()) return;

    glUseProgram(pbrShader);
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "projection"), 1, GL_FALSE, value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "view"), 1, GL_FALSE, value_ptr(view));
    glUniform3fv(glGetUniformLocation(pbrShader, "camPos"), 1, value_ptr(camPos));
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "model"), 1, GL_FALSE, value_ptr(modelTransform));
    glUniformMatrix3fv(glGetUniformLocation(pbrShader, "normalMatrix"), 1, GL_FALSE,
        value_ptr(transpose(inverse(mat3(modelTransform)))));

    // Render each material group with its assigned PBR material
    for (size_t i = 0; i < model->mesh_groups.size() && i < NUM_MATERIALS_PER_OBJECT; ++i) {
        int materialIndex = getMaterialIndexForObject(objType, i);

        switch (materialIndex) {
        case 0:
            bindPBRTextures(gold);
            break;
        case 1:
            bindPBRTextures(plastic);
            break;
        case 2:
            bindPBRTextures(cloth);
            break;
        case 3:
            bindPBRTextures(panel);
            break;
        case 4:
            bindPBRTextures(solar);
            break;
        case 5:
            bindPBRTextures(metal);
            break;

        default:
            bindPBRTextures(plastic);
            break;
        }

        model->mesh_groups[i].mesh.draw();
    }
}

void Station::renderMultiMaterialModel(const glm::mat4& view, const glm::mat4& proj,
    GLuint pbrShader, const glm::vec3& camPos) {
    if (!m_drawStation || !m_modelsLoaded) {
        return;
    }

    const auto& modules = m_lsystem.getModules();
    const auto& junctions = m_lsystem.getJunctions();

    // Render all modules
    for (const auto& module : modules) {
        cgra::multi_mesh_model* model = getModelForLength(module.length);
        ObjectType objType = getObjectTypeForLength(module.length);
        mat4 transform = calculateModuleTransform(module);
        renderModelWithMaterials(model, objType, transform, pbrShader, view, proj, camPos);

        // Render solar panels if this module has them
        if (module.hasSolarPanels && m_solarPanelModel) {
            // Render left panel
            mat4 leftPanelTransform = calculateSolarPanelTransform(module, true);
            renderModelWithMaterials(m_solarPanelModel, ObjectType::SOLAR_PANEL,
                leftPanelTransform, pbrShader, view, proj, camPos);

            // Render right panel
            mat4 rightPanelTransform = calculateSolarPanelTransform(module, false);
            renderModelWithMaterials(m_solarPanelModel, ObjectType::SOLAR_PANEL,
                rightPanelTransform, pbrShader, view, proj, camPos);
        }
    }

    // Render all junctions
    for (const auto& junction : junctions) {
        mat4 transform = calculateJunctionTransform(junction);
        renderModelWithMaterials(m_junctionModel, ObjectType::JUNCTION,
            transform, pbrShader, view, proj, camPos);
    }
}

cgra::gl_mesh Station::createSphereMesh(float radius, int stacks, int slices) {
    using namespace cgra;
    mesh_builder builder(GL_TRIANGLES);

    for (int i = 0; i <= stacks; ++i) {
        float phi = PI * float(i) / float(stacks);
        float y = radius * std::cos(phi);
        float radiusAtY = radius * std::sin(phi);

        for (int j = 0; j <= slices; ++j) {
            float theta = TWO_PI * float(j) / float(slices);
            float x = radiusAtY * std::cos(theta);
            float z = radiusAtY * std::sin(theta);

            vec3 pos(x, y, z);
            vec3 normal = normalize(pos);
            vec2 uv(float(j) / slices, float(i) / stacks);

            builder.vertices.push_back({ pos, normal, uv });
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            int curr = i * (slices + 1) + j;
            int next = curr + slices + 1;

            builder.indices.push_back(curr);
            builder.indices.push_back(next);
            builder.indices.push_back(curr + 1);

            builder.indices.push_back(curr + 1);
            builder.indices.push_back(next);
            builder.indices.push_back(next + 1);
        }
    }

    return builder.build();
}

void Station::rebuildMeshes() {
    m_junctionMesh = createSphereMesh(m_renderParams.junctionRadius, 16, 16);
    m_meshNeedsRebuild = false;
}

void Station::initializeLSystem() {
    m_lsystem.initialize();
}

void Station::regenerate() {
    m_lsystem.regenerate();
}

void Station::render3DStation(const glm::mat4& view, const glm::mat4& proj, GLuint shader) {
    // 3D rendering is now handled entirely by renderMultiMaterialModel
    // This function is kept for compatibility but does nothing
}

void Station::applyUIStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.FramePadding = ImVec2(5, 3);
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.IndentSpacing = 20.0f;
}

void Station::renderMaterialControls(const char* objectName, std::array<int, 4>& materials) {
    ImGui::PushID(objectName);

    if (ImGui::TreeNode(objectName)) {
        ImGui::Spacing();

        for (int i = 0; i < NUM_MATERIALS_PER_OBJECT; ++i) {
            ImGui::PushID(i);

            std::string label = "Material Slot " + std::to_string(i);
            ImGui::Text("%s", label.c_str());
            ImGui::SameLine(150);

            ImGui::PushItemWidth(120);
            if (ImGui::Combo(("##mat" + std::to_string(i)).c_str(), &materials[i],
                MATERIAL_NAMES, NUM_PBR_MATERIALS)) {
                // Material changed
            }
            ImGui::PopItemWidth();

            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void Station::renderControlsPanel() {
    bool needsRegeneration = false;
    LSystemParams& params = m_lsystem.getParams();

    ImGui::PushStyleColor(ImGuiCol_Header, COLOR_HEADER);
    ImGui::Spacing();
    ImGui::Text("L-SYSTEM SPACE STATION GENERATOR");
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ========== ENABLE 3D SPACE STATION (TOP LEVEL TOGGLE) ==========
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.9f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 1.0f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.5f, 0.8f, 1.0f));

    bool toggleChanged = false;
    if (m_drawStation) {
        if (ImGui::Button("Disable 3D Space Station", ImVec2(-1, 40))) {
            m_drawStation = false;
            toggleChanged = true;
        }
    }
    else {
        if (ImGui::Button("Enable 3D Space Station", ImVec2(-1, 40))) {
            m_drawStation = true;
            toggleChanged = true;
        }
    }

    ImGui::PopStyleColor(3);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Only show everything else if 3D station is enabled
    if (m_drawStation) {
        // ========== GENERATION PARAMETERS ==========
        if (ImGui::CollapsingHeader("Generation Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();

            // Complexity
            ImGui::Text("Complexity");
            ImGui::PushItemWidth(-1);
            needsRegeneration |= ImGui::SliderInt("##Iterations", &params.iterations, 1, 6);
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Number of L-System iterations");
            ImGui::Spacing();

            // Max Module Size (changed to 1, 2, 3)
            ImGui::Text("Max Module Size");
            int moduleSizeIndex = 0;
            if (params.baseLength <= 15.0f) moduleSizeIndex = 0;      // 1 = 10 units
            else if (params.baseLength <= 25.0f) moduleSizeIndex = 1; // 2 = 20 units
            else moduleSizeIndex = 2;                                  // 3 = 30 units

            ImGui::PushItemWidth(-1);
            if (ImGui::SliderInt("##ModuleSize", &moduleSizeIndex, 0, 2,
                moduleSizeIndex == 0 ? "1 (10 units)" :
                moduleSizeIndex == 1 ? "2 (20 units)" : "3 (30 units)")) {
                // Convert index to actual length
                if (moduleSizeIndex == 0) params.baseLength = 10.0f;
                else if (moduleSizeIndex == 1) params.baseLength = 20.0f;
                else params.baseLength = 30.0f;
                needsRegeneration = true;
            }
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Maximum module size: 1=10 units, 2=20 units, 3=30 units");
            ImGui::Spacing();

            // Decay
            ImGui::Text("Decay");
            ImGui::PushItemWidth(-1);
            needsRegeneration |= ImGui::SliderFloat("##LengthDecay", &params.lengthDecay, 0.5f, 1.0f, "%.2f");
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("How much modules shrink each generation");
            ImGui::Spacing();

            // Seed
            ImGui::Text("Seed");
            ImGui::PushItemWidth(-1);
            needsRegeneration |= ImGui::SliderInt("##Seed", &params.seed, 1, 99999);
            ImGui::PopItemWidth();
            ImGui::Spacing();
        }

        // ========== INLINE TOGGLE BUTTONS ==========
        ImGui::Spacing();

        // Calculate button width for 3 buttons in a row
        float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;

        // Vertical Modules Toggle
        ImGui::PushStyleColor(ImGuiCol_Button, params.allowVerticalModules ?
            ImVec4(0.2f, 0.7f, 0.3f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, params.allowVerticalModules ?
            ImVec4(0.3f, 0.8f, 0.4f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, params.allowVerticalModules ?
            ImVec4(0.1f, 0.6f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        if (ImGui::Button(params.allowVerticalModules ? "Vertical Modules ON" : "Vertical Modules OFF",
            ImVec2(buttonWidth, 30))) {
            params.allowVerticalModules = !params.allowVerticalModules;
            needsRegeneration = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        // Solar Panels Toggle
        ImGui::PushStyleColor(ImGuiCol_Button, params.enableSolarPanels ?
            ImVec4(0.2f, 0.7f, 0.3f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, params.enableSolarPanels ?
            ImVec4(0.3f, 0.8f, 0.4f, 1.0f) : ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, params.enableSolarPanels ?
            ImVec4(0.1f, 0.6f, 0.2f, 1.0f) : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

        if (ImGui::Button(params.enableSolarPanels ? "Solar Panels ON" : "Solar Panels OFF",
            ImVec2(buttonWidth, 30))) {
            params.enableSolarPanels = !params.enableSolarPanels;
            needsRegeneration = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();

        // Randomise Seed Button
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.6f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.7f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.8f, 0.5f, 0.1f, 1.0f));

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

        // ========== MODEL PARAMETERS ==========
        if (ImGui::CollapsingHeader("Model Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();

            // Vertical Probability (only if vertical modules enabled)
            if (params.allowVerticalModules) {
                ImGui::Text("Vertical Probability");
                ImGui::PushItemWidth(-1);
                needsRegeneration |= ImGui::SliderFloat("##VertProb", &params.verticalProbability, 0.0f, 1.0f, "%.2f");
                ImGui::PopItemWidth();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chance of creating a vertical module");
                ImGui::Spacing();
            }

            // Solar Panel Parameters (only if solar panels enabled)
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

        // ========== MATERIAL PARAMETERS ==========
        if (ImGui::CollapsingHeader("Material Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Spacing();

            // Two buttons side by side
            float halfWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.8f, 0.4f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.6f, 0.2f, 1.0f));

            if (ImGui::Button("Randomise", ImVec2(halfWidth, 30))) {
                randomizeMaterials();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Randomly assign materials to all slots");
            }

            ImGui::PopStyleColor(3);
            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.3f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.4f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.6f, 0.2f, 0.1f, 1.0f));

            if (ImGui::Button("Reset", ImVec2(halfWidth, 30))) {
                resetMaterials();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Reset materials to default");
            }

            ImGui::PopStyleColor(3);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Individual Object Materials
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

        // ========== STATISTICS ==========
        if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto& modules = m_lsystem.getModules();
            const auto& junctions = m_lsystem.getJunctions();
            const auto& sequence = m_lsystem.getCurrentSequence();

            ImGui::Spacing();
            ImGui::Columns(2, "stats", false);

            ImGui::Text("Total Modules:");
            ImGui::NextColumn();
            ImGui::Text("%zu", modules.size());
            ImGui::NextColumn();

            int verticalCount = 0;
            int solarPanelCount = 0;

            for (const auto& module : modules) {
                if (module.isVertical) verticalCount++;
                if (module.hasSolarPanels) solarPanelCount++;
            }

            ImGui::Text("Vertical Modules:");
            ImGui::NextColumn();
            ImGui::Text("%d", verticalCount);
            ImGui::NextColumn();

            ImGui::Text("With Solar Panels:");
            ImGui::NextColumn();
            ImGui::Text("%d", solarPanelCount);
            ImGui::NextColumn();

            ImGui::Text("Junctions:");
            ImGui::NextColumn();
            ImGui::Text("%zu", junctions.size());
            ImGui::NextColumn();

            ImGui::Text("Sequence Length:");
            ImGui::NextColumn();
            ImGui::Text("%zu", sequence.length());
            ImGui::NextColumn();

            ImGui::Columns(1);
            ImGui::Spacing();
        }

        // ========== OBJ STATISTICS ==========
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

            ImGui::Text("10-unit Modules:");
            ImGui::NextColumn();
            ImGui::Text("%d", length10Count);
            ImGui::NextColumn();

            ImGui::Text("20-unit Modules:");
            ImGui::NextColumn();
            ImGui::Text("%d", length20Count);
            ImGui::NextColumn();

            ImGui::Text("30-unit Modules:");
            ImGui::NextColumn();
            ImGui::Text("%d", length30Count);
            ImGui::NextColumn();

            if (m_modelsLoaded) {
                ImGui::Text("Models Status:");
                ImGui::NextColumn();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Loaded");
                ImGui::NextColumn();
            }

            ImGui::Columns(1);
            ImGui::Spacing();
        }
    }

    if (needsRegeneration) regenerate();
}

void Station::renderGUI() {
    //applyUIStyle();
    ImGui::SetNextWindowPos(ImVec2(5, 540), ImGuiSetCond_Once);
    ImGui::SetNextWindowSize(ImVec2(400, 460), ImGuiSetCond_Once);
    ImGui::Begin("Space Station Generator", nullptr);

    renderControlsPanel();

    ImGui::End();
}