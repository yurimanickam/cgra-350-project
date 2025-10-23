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

#include "yuri/objloader.hpp"
#include "matt/pbr.hpp"

using namespace glm;

namespace {
    constexpr float HALF_PI = glm::half_pi<float>();
    constexpr float TWO_PI = glm::two_pi<float>();
    constexpr float PI = glm::pi<float>();
    constexpr float MODEL_LENGTH = 10.0f;
    constexpr float JUNCTION_SIZE = 5.9f;

    // module colors for preview
    const ImU32 MODULE_COLORS[] = {
        IM_COL32(200, 200, 200, 255),
        IM_COL32(100, 255, 100, 255),
        IM_COL32(100, 150, 255, 255),
        IM_COL32(255, 220, 100, 255)
    };

    constexpr int NUM_MODULE_TYPES = 4;

    // junction colors
    const ImU32 JUNCTION_COLOR = IM_COL32(150, 150, 160, 255);
    const ImU32 VERTICAL_JUNCTION_COLOR = IM_COL32(255, 150, 100, 255);

    // some ui colors
    const ImVec4 COLOR_HEADER = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    const ImVec4 COLOR_ACTIVE = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    const ImVec4 COLOR_HOVER = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
}

Station::Station()
    : m_module1(nullptr)
    , m_module2(nullptr)
    , m_module3(nullptr)
    , m_junctionModel(nullptr)
    , m_modelsLoaded(false)
{
    initializeLSystem();
    rebuildMeshes();

    // Load all module models and junction
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

    if (!m_module1->mesh_groups.empty() &&
        !m_module2->mesh_groups.empty() &&
        !m_module3->mesh_groups.empty() &&
        !m_junctionModel->mesh_groups.empty()) {
        m_modelsLoaded = true;
        assignCyclicalMaterials();
        std::cout << "Station: All module models and junction loaded successfully" << std::endl;
    }
    else {
        m_modelsLoaded = false;
        std::cout << "Station: Failed to load one or more module models" << std::endl;
    }
}

void Station::assignCyclicalMaterials() {
    m_materialAssignments.clear();

    // Assign materials for module1
    if (m_module1 && !m_module1->mesh_groups.empty()) {
        for (size_t i = 0; i < m_module1->mesh_groups.size(); ++i) {
            m_materialAssignments.push_back(i % 3);
        }
    }

    // Same pattern for all modules and junction
    std::cout << "Station: Assigned cyclical PBR materials (0=gold, 1=plastic, 2=cloth)" << std::endl;
}

int Station::getMaterialIndexForGroup(size_t groupIndex) const {
    if (groupIndex < m_materialAssignments.size()) {
        return m_materialAssignments[groupIndex];
    }
    return 1; // Default to plastic
}

cgra::multi_mesh_model* Station::getModelForLength(float length) const {
    if (length <= 15.0f) return m_module1;      // 10 units
    else if (length <= 25.0f) return m_module2; // 20 units
    else return m_module3;                       // 30 units
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

        // Calculate center position
        float moduleLength = module.length;
        float centerOffset = (moduleLength / 2.0f) + (JUNCTION_SIZE / 2.0f);
        position.y += pointingUp ? centerOffset : -centerOffset;

        // Start with translation
        mat4 transform = translate(mat4(1.0f), position);

        // Rotate to point vertically
        if (pointingUp) {
            // Point up: rotate -90° around X axis
            transform = rotate(transform, -HALF_PI, vec3(1.0f, 0.0f, 0.0f));
        }
        else {
            // Point down: rotate 90° around X axis
            transform = rotate(transform, HALF_PI, vec3(1.0f, 0.0f, 0.0f));
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
        float centerOffset = (moduleLength / 2.0f) + (JUNCTION_SIZE / 2.0f);
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

void Station::renderModelWithMaterials(cgra::multi_mesh_model* model, const glm::mat4& modelTransform,
    GLuint pbrShader, const glm::mat4& view, const glm::mat4& proj,
    const glm::vec3& camPos) {
    if (!model || model->mesh_groups.empty()) return;

    glUseProgram(pbrShader);
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "projection"), 1, GL_FALSE, value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "view"), 1, GL_FALSE, value_ptr(view));
    glUniform3fv(glGetUniformLocation(pbrShader, "camPos"), 1, value_ptr(camPos));
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "model"), 1, GL_FALSE, value_ptr(modelTransform));
    glUniformMatrix3fv(glGetUniformLocation(pbrShader, "normalMatrix"), 1, GL_FALSE,
        value_ptr(transpose(inverse(mat3(modelTransform)))));

    // Render each material group with its assigned PBR material
    for (size_t i = 0; i < model->mesh_groups.size(); ++i) {
        int materialIndex = getMaterialIndexForGroup(i);

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
        mat4 transform = calculateModuleTransform(module);
        renderModelWithMaterials(model, transform, pbrShader, view, proj, camPos);
    }

    // Render all junctions
    for (const auto& junction : junctions) {
        mat4 transform = calculateJunctionTransform(junction);
        renderModelWithMaterials(m_junctionModel, transform, pbrShader, view, proj, camPos);
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

glm::vec3 Station::getModuleColor(int moduleType) const {
    switch (moduleType) {
    case 0: return glm::vec3(0.8f, 0.8f, 0.8f);
    case 1: return glm::vec3(0.4f, 0.9f, 0.4f);
    case 2: return glm::vec3(0.4f, 0.6f, 0.9f);
    case 3: return glm::vec3(0.9f, 0.86f, 0.4f);
    default: return glm::vec3(0.7f, 0.7f, 0.7f);
    }
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

void Station::renderControlsPanel() {
    bool needsRegeneration = false;
    LSystemParams& params = m_lsystem.getParams();

    ImGui::PushStyleColor(ImGuiCol_Header, COLOR_HEADER);
    ImGui::Spacing();
    ImGui::Text("L-SYSTEM SPACE STATION GENERATOR");
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Generation params
    if (ImGui::CollapsingHeader("Generation Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        ImGui::Text("Complexity");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderInt("##Iterations", &params.iterations, 1, 6);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Number of L-System iterations (higher = more complex station)");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Module Sizing (10, 20, or 30 units)");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderFloat("##BaseLength", &params.baseLength, 10.0f, 30.0f, "Base: %.0f");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Initial module length (10, 20, or 30)");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderFloat("##LengthDecay", &params.lengthDecay, 0.5f, 1.0f, "Decay: %.2f");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("How much modules shrink each generation");
        ImGui::Spacing();
        ImGui::TextWrapped("Note: Modules are quantized to 10, 20, or 30 units");
        ImGui::Spacing();
    }

    // Topology
    if (ImGui::CollapsingHeader("Topology & Connections", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        ImGui::Checkbox("Allow Loop Connections", &params.allowLoops);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable additional connecting modules between nearby junctions");
        if (params.allowLoops) {
            ImGui::Spacing();
            ImGui::Text("Loop Probability");
            ImGui::PushItemWidth(-1);
            needsRegeneration |= ImGui::SliderFloat("##ConnProb", &params.connectionProbability, 0.0f, 0.5f, "%.2f");
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chance of creating an additional connecting module");
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        needsRegeneration |= ImGui::Checkbox("Allow Vertical Modules", &params.allowVerticalModules);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable modules that extend upward or downward");
        if (params.allowVerticalModules) {
            ImGui::Spacing();
            ImGui::Text("Vertical Probability");
            ImGui::PushItemWidth(-1);
            needsRegeneration |= ImGui::SliderFloat("##VertProb", &params.verticalProbability, 0.0f, 0.5f, "%.2f");
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chance of creating a vertical module");
        }
        ImGui::Spacing();
    }

    // 3D rendering
    if (ImGui::CollapsingHeader("3D Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        ImGui::Checkbox("Enable 3D View", &m_drawStation);
        ImGui::Spacing();

        if (m_modelsLoaded) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "? All models loaded");
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "? Models not loaded");
        }

        ImGui::Spacing();
    }

    // Materials
    if (ImGui::CollapsingHeader("Model Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        ImGui::TextWrapped("Modules are rendered using three different sized models (10, 20, 30 units) with PBR materials.");
        ImGui::Spacing();

        if (m_modelsLoaded) {
            ImGui::Text("Loaded models:");
            ImGui::BulletText("module1.obj (10 units)");
            ImGui::BulletText("module2.obj (20 units)");
            ImGui::BulletText("module3.obj (30 units)");
            ImGui::BulletText("junction.obj (5.9 units)");
            ImGui::Spacing();
            ImGui::Text("Materials cycle: 0=Gold, 1=Plastic, 2=Cloth");
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Models not loaded");
        }

        ImGui::Spacing();
    }

    // Random seed
    if (ImGui::CollapsingHeader("Random Seed", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderInt("##Seed", &params.seed, 1, 99999);
        ImGui::PopItemWidth();
        ImGui::Spacing();
        if (ImGui::Button("New Random Seed", ImVec2(-1, 0))) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 99999);
            params.seed = dis(gen);
            needsRegeneration = true;
        }
        ImGui::Spacing();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Stats
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
        int length10Count = 0;
        int length20Count = 0;
        int length30Count = 0;

        for (const auto& module : modules) {
            if (module.isVertical) verticalCount++;
            if (module.length <= 15.0f) length10Count++;
            else if (module.length <= 25.0f) length20Count++;
            else length30Count++;
        }

        ImGui::Text("Vertical Modules:");
        ImGui::NextColumn();
        ImGui::Text("%d", verticalCount);
        ImGui::NextColumn();

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
        ImGui::Text("Module Type Breakdown");
        ImGui::Separator();

        std::vector<int> moduleCounts(NUM_MODULE_TYPES, 0);
        for (const auto& module : modules)
            if (module.moduleType < NUM_MODULE_TYPES)
                moduleCounts[module.moduleType]++;

        const char* moduleNames[] = { "Corridors", "Habitats", "Docking Bays", "Power Modules" };
        const ImU32 moduleColors[] = {
            IM_COL32(200, 200, 200, 255),
            IM_COL32(100, 255, 100, 255),
            IM_COL32(100, 150, 255, 255),
            IM_COL32(255, 220, 100, 255)
        };

        for (int i = 0; i < NUM_MODULE_TYPES; ++i) {
            ImGui::BulletText("%s:", moduleNames[i]);
            ImGui::SameLine(160);
            int color = moduleColors[i];
            float r = ((color >> 0) & 0xFF) / 255.0f;
            float g = ((color >> 8) & 0xFF) / 255.0f;
            float b = ((color >> 16) & 0xFF) / 255.0f;
            ImGui::TextColored(ImVec4(r, g, b, 1.0f), "%d", moduleCounts[i]);
        }
        ImGui::Spacing();
    }

    if (needsRegeneration) regenerate();
}

void Station::renderGUI() {
    applyUIStyle();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(900, 720), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("Space Station Generator", nullptr);
    ImGui::BeginChild("LeftPane", ImVec2(420, 0), true);
    renderControlsPanel();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("RightPane", ImVec2(0, 0), true);
    renderPreviewPanel();
    ImGui::EndChild();
    ImGui::End();
}

void Station::renderPreviewPanel() {
    ImGui::Spacing();
    ImGui::Text("STATION LAYOUT PREVIEW");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Columns(3, "zoom", false);
    if (ImGui::Button("[-] Zoom Out", ImVec2(-1, 0)))
        m_previewZoom = glm::clamp(m_previewZoom / 1.2f, 0.2f, 8.0f);
    ImGui::NextColumn();

    if (ImGui::Button("[=] Reset", ImVec2(-1, 0))) {
        m_previewZoom = 1.0f;
        m_previewPan = vec2(0.0f);
    }
    ImGui::NextColumn();

    if (ImGui::Button("[+] Zoom In", ImVec2(-1, 0)))
        m_previewZoom = glm::clamp(m_previewZoom * 1.2f, 0.2f, 8.0f);
    ImGui::NextColumn();
    ImGui::Columns(1);

    ImGui::Text("Zoom: %.1fx", m_previewZoom);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Legend:");
    ImGui::Columns(2, "legend", false);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    float legendLineLength = 20.0f;
    float legendLineThickness = 4.0f;
    float legendCircleSize = 6.0f;

    const char* moduleNames[] = { "Corridor", "Habitat", "Docking Bay", "Power" };
    for (int i = 0; i < NUM_MODULE_TYPES; ++i) {
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        draw_list->AddLine(
            ImVec2(cursor.x, cursor.y + legendCircleSize),
            ImVec2(cursor.x + legendLineLength, cursor.y + legendCircleSize),
            MODULE_COLORS[i],
            legendLineThickness
        );
        ImGui::Dummy(ImVec2(legendLineLength, legendCircleSize * 2));
        ImGui::SameLine();
        ImGui::Text("%s Module", moduleNames[i]);
        ImGui::NextColumn();
    }

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    draw_list->AddCircleFilled(
        ImVec2(cursor.x + legendCircleSize, cursor.y + legendCircleSize),
        legendCircleSize,
        JUNCTION_COLOR
    );
    ImGui::Dummy(ImVec2(legendCircleSize * 2, legendCircleSize * 2));
    ImGui::SameLine();
    ImGui::Text("Junction");
    ImGui::NextColumn();

    cursor = ImGui::GetCursorScreenPos();
    draw_list->AddCircleFilled(
        ImVec2(cursor.x + legendCircleSize, cursor.y + legendCircleSize),
        legendCircleSize,
        VERTICAL_JUNCTION_COLOR
    );
    ImGui::Dummy(ImVec2(legendCircleSize * 2, legendCircleSize * 2));
    ImGui::SameLine();
    ImGui::Text("Vertical Junction");
    ImGui::NextColumn();

    ImGui::Columns(1);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    drawVisualization();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextWrapped("Tip: Use the zoom controls to explore the station layout. "
        "Modules are rendered as appropriate sized models (10, 20, or 30 units) with junctions between them.");
}

void Station::calculateBounds(vec2& minBounds, vec2& maxBounds) const {
    const auto& junctions = m_lsystem.getJunctions();

    minBounds = vec2(FLT_MAX);
    maxBounds = vec2(-FLT_MAX);
    for (const auto& junction : junctions) {
        minBounds = glm::min(minBounds, junction.position);
        maxBounds = glm::max(maxBounds, junction.position);
    }
    vec2 padding(25.0f);
    minBounds -= padding;
    maxBounds += padding;
}

void Station::drawVisualization() {
    const auto& modules = m_lsystem.getModules();
    const auto& junctions = m_lsystem.getJunctions();

    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 available = ImGui::GetContentRegionAvail();
    ImVec2 canvas_size(
        std::max(300.0f, available.x),
        std::max(400.0f, available.y - 80.0f)
    );

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), true);

    vec2 minBounds, maxBounds;
    calculateBounds(minBounds, maxBounds);
    vec2 worldSize = maxBounds - minBounds;

    float fitScale = (worldSize.x > 0.0f && worldSize.y > 0.0f)
        ? std::min(canvas_size.x / worldSize.x, canvas_size.y / worldSize.y) * 0.95f
        : 1.0f;
    float scale = fitScale * m_previewZoom;

    vec2 offset(
        (canvas_size.x - (worldSize.x * scale)) * 0.5f + m_previewPan.x,
        (canvas_size.y - (worldSize.y * scale)) * 0.5f + m_previewPan.y
    );

    draw_list->AddRectFilled(
        canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(20, 20, 25, 255)
    );
    draw_list->AddRect(
        canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(100, 100, 120, 255),
        0.0f,
        0,
        2.0f
    );

    float gridSpacing = 50.0f * scale;
    if (gridSpacing > 10.0f) {
        for (float x = fmod(offset.x, gridSpacing); x < canvas_size.x; x += gridSpacing) {
            draw_list->AddLine(
                ImVec2(canvas_pos.x + x, canvas_pos.y),
                ImVec2(canvas_pos.x + x, canvas_pos.y + canvas_size.y),
                IM_COL32(40, 40, 45, 255)
            );
        }
        for (float y = fmod(offset.y, gridSpacing); y < canvas_size.y; y += gridSpacing) {
            draw_list->AddLine(
                ImVec2(canvas_pos.x, canvas_pos.y + y),
                ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + y),
                IM_COL32(40, 40, 45, 255)
            );
        }
    }

    auto worldToScreen = [&](const vec2& worldPos) -> ImVec2 {
        return ImVec2(
            canvas_pos.x + offset.x + (worldPos.x - minBounds.x) * scale,
            canvas_pos.y + offset.y + (worldPos.y - minBounds.y) * scale
        );
        };

    float moduleLineThickness = 6.0f * glm::clamp(m_previewZoom, 0.5f, 2.0f);

    for (const auto& module : modules) {
        ImU32 color = (module.moduleType >= 0 && module.moduleType < NUM_MODULE_TYPES)
            ? MODULE_COLORS[module.moduleType]
            : IM_COL32(200, 100, 100, 255);

        if (module.isVertical) {
            ImVec2 p = worldToScreen(module.startPos);
            float arrowSize = 8.0f * glm::clamp(m_previewZoom, 0.5f, 2.0f);

            draw_list->AddLine(
                ImVec2(p.x + 1, p.y - arrowSize + 1),
                ImVec2(p.x + 1, p.y + arrowSize + 1),
                IM_COL32(0, 0, 0, 80),
                moduleLineThickness
            );
            draw_list->AddLine(
                ImVec2(p.x, p.y - arrowSize),
                ImVec2(p.x, p.y + arrowSize),
                color,
                moduleLineThickness
            );
            draw_list->AddLine(
                ImVec2(p.x - arrowSize * 0.5f, p.y),
                ImVec2(p.x + arrowSize * 0.5f, p.y),
                color,
                moduleLineThickness * 0.7f
            );
        }
        else {
            ImVec2 p1 = worldToScreen(module.startPos);
            ImVec2 p2 = worldToScreen(module.endPos);
            draw_list->AddLine(
                ImVec2(p1.x + 1, p1.y + 1),
                ImVec2(p2.x + 1, p2.y + 1),
                IM_COL32(0, 0, 0, 80),
                moduleLineThickness
            );
            draw_list->AddLine(p1, p2, color, moduleLineThickness);
        }
    }

    float junctionRadius = 4.0f * glm::clamp(m_previewZoom, 0.5f, 2.0f);

    for (const auto& junction : junctions) {
        ImVec2 center = worldToScreen(junction.position);
        ImU32 jColor = (std::abs(junction.verticalOffset) > 0.1f)
            ? VERTICAL_JUNCTION_COLOR
            : JUNCTION_COLOR;
        draw_list->AddCircleFilled(
            ImVec2(center.x + 1, center.y + 1),
            junctionRadius,
            IM_COL32(0, 0, 0, 80),
            12
        );
        draw_list->AddCircleFilled(center, junctionRadius, jColor, 12);

        ImU32 borderColor = (std::abs(junction.verticalOffset) > 0.1f)
            ? IM_COL32(180, 100, 50, 255)
            : IM_COL32(80, 80, 90, 255);
        draw_list->AddCircle(center, junctionRadius, borderColor, 12, 1.5f);
    }

    draw_list->PopClipRect();

    ImGui::SetCursorScreenPos(canvas_pos);
    ImGui::InvisibleButton("canvas", canvas_size);

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_previewPan.x += delta.x;
        m_previewPan.y += delta.y;
    }

    if (ImGui::IsItemHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_previewZoom = glm::clamp(m_previewZoom * (1.0f + wheel * 0.1f), 0.2f, 8.0f);
        }
    }
}