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
    constexpr float MODEL_HEIGHT = 5.0f;
    constexpr float MODEL_WIDTH = 5.0f;

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

// Make new station
Station::Station() : m_multiModel(nullptr), m_modelLoaded(false) {
    initializeLSystem();
    rebuildMeshes();

    // Load the multi-material model
    loadMultiMaterialModel(CGRA_SRCDIR + std::string("/res/assets/OpenModule.obj"));
}

Station::~Station() {
    if (m_multiModel) {
        m_multiModel->destroy();
        delete m_multiModel;
        m_multiModel = nullptr;
    }
}

// Load multi-material model
void Station::loadMultiMaterialModel(const std::string& filepath) {
    if (m_multiModel) {
        m_multiModel->destroy();
        delete m_multiModel;
    }

    m_multiModel = new cgra::multi_mesh_model();
    *m_multiModel = cgra::load_multi_mesh_model(filepath);

    if (!m_multiModel->mesh_groups.empty()) {
        m_modelLoaded = true;
        assignCyclicalMaterials();
        std::cout << "Station: Loaded multi-material model with "
            << m_multiModel->mesh_groups.size() << " material groups" << std::endl;
    }
    else {
        m_modelLoaded = false;
        std::cout << "Station: Failed to load multi-material model" << std::endl;
    }
}

// Assign materials in cyclical pattern: 0,1,2,0,1,2...
void Station::assignCyclicalMaterials() {
    m_materialAssignments.clear();

    if (!m_multiModel || m_multiModel->mesh_groups.empty()) {
        return;
    }

    // Assign materials in pattern: 0=gold, 1=plastic, 2=cloth, repeat
    for (size_t i = 0; i < m_multiModel->mesh_groups.size(); ++i) {
        int materialIndex = i % 3; // Cycles through 0, 1, 2
        m_materialAssignments.push_back(materialIndex);

        std::cout << "Station: Assigned material " << materialIndex
            << " to group '" << m_multiModel->mesh_groups[i].material_name << "'" << std::endl;
    }
}

// Get material index for a given group
int Station::getMaterialIndexForGroup(size_t groupIndex) const {
    if (groupIndex < m_materialAssignments.size()) {
        return m_materialAssignments[groupIndex];
    }
    return 1; // Default to plastic
}

// Get positions for models in a module
std::vector<glm::vec3> Station::getModelPositionsForModule(const StationModule& module) const {
    std::vector<glm::vec3> positions;

    if (module.isVertical) {
        const auto& junctions = m_lsystem.getJunctions();
        vec3 basePos3D(module.startPos.x, module.verticalOffset, module.startPos.y);

        // Find end vertical offset
        float endY = module.verticalOffset;
        for (const auto& junction : junctions) {
            if (length(junction.position - module.startPos) < 0.1f &&
                std::abs(junction.verticalOffset - module.verticalOffset) > 0.1f) {
                endY = junction.verticalOffset;
                break;
            }
        }

        bool pointingUp = endY > module.verticalOffset;
        float direction = pointingUp ? 1.0f : -1.0f;
        float gapSize = m_renderParams.junctionRadius * m_renderParams.gapMultiplier;

        // Place models vertically
        for (int i = 0; i < module.modelCount; ++i) {
            float offset = gapSize + MODEL_LENGTH * 0.5f + i * MODEL_LENGTH;
            positions.push_back(basePos3D + vec3(0.0f, direction * offset, 0.0f));
        }
    }
    else {
        vec3 fromPos3D(module.startPos.x, 0.0f, module.startPos.y);
        vec3 toPos3D(module.endPos.x, 0.0f, module.endPos.y);
        vec3 direction = normalize(toPos3D - fromPos3D);
        float gapSize = m_renderParams.junctionRadius * m_renderParams.gapMultiplier;

        // Place models horizontally along the line
        for (int i = 0; i < module.modelCount; ++i) {
            float offset = gapSize + MODEL_LENGTH * 0.5f + i * MODEL_LENGTH;
            positions.push_back(fromPos3D + direction * offset);
        }
    }

    return positions;
}

// Calculate transform for a single model instance
glm::mat4 Station::calculateModelTransform(const glm::vec3& position, float rotation, bool isVertical, bool pointingUp) const {
    mat4 transform = translate(mat4(1.0f), position);

    if (isVertical) {
        if (pointingUp) {
            transform = rotate(transform, HALF_PI, vec3(0.0f, 0.0f, 1.0f));
        }
        else {
            transform = rotate(transform, -HALF_PI, vec3(0.0f, 0.0f, 1.0f));
        }
    }
    else {
        // Rotate around Y axis for horizontal orientation
        vec3 defaultDir(1.0f, 0.0f, 0.0f);
        float cosAngle = std::cos(rotation);
        float sinAngle = std::sin(rotation);
        vec3 normalizedDir(cosAngle, 0.0f, sinAngle);

        vec3 rotAxis = cross(defaultDir, normalizedDir);
        float rotAxisLen = length(rotAxis);

        if (rotAxisLen > 0.001f) {
            float angle = std::acos(glm::clamp(dot(defaultDir, normalizedDir), -1.0f, 1.0f));
            transform = rotate(transform, angle, normalize(rotAxis));
        }
        else if (dot(defaultDir, normalizedDir) < 0.0f) {
            transform = rotate(transform, PI, vec3(0.0f, 1.0f, 0.0f));
        }
    }

    return transform;
}

// Render multi-material model instances for all modules
void Station::renderMultiMaterialModel(const glm::mat4& view, const glm::mat4& proj, GLuint pbrShader, const glm::vec3& camPos) {
    if (!m_drawStation || !m_modelLoaded || !m_multiModel) {
        return;
    }

    const auto& modules = m_lsystem.getModules();
    const auto& junctions = m_lsystem.getJunctions();

    glUseProgram(pbrShader);
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "projection"), 1, GL_FALSE, value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "view"), 1, GL_FALSE, value_ptr(view));
    glUniform3fv(glGetUniformLocation(pbrShader, "camPos"), 1, value_ptr(camPos));

    // Render each module as instances of the multi-material model
    for (const auto& module : modules) {
        std::vector<glm::vec3> positions = getModelPositionsForModule(module);

        // Determine if pointing up for vertical modules
        bool pointingUp = false;
        if (module.isVertical) {
            for (const auto& junction : junctions) {
                if (length(junction.position - module.startPos) < 0.1f &&
                    std::abs(junction.verticalOffset - module.verticalOffset) > 0.1f) {
                    pointingUp = junction.verticalOffset > module.verticalOffset;
                    break;
                }
            }
        }

        // Render each model instance
        for (const auto& pos : positions) {
            mat4 modelTransform = calculateModelTransform(pos, module.rotation, module.isVertical, pointingUp);
            glUniformMatrix4fv(glGetUniformLocation(pbrShader, "model"), 1, GL_FALSE, value_ptr(modelTransform));
            glUniformMatrix3fv(glGetUniformLocation(pbrShader, "normalMatrix"), 1, GL_FALSE,
                value_ptr(glm::transpose(glm::inverse(glm::mat3(modelTransform)))));

            // Render each material group with its assigned PBR material
            for (size_t i = 0; i < m_multiModel->mesh_groups.size(); ++i) {
                int materialIndex = getMaterialIndexForGroup(i);

                // Bind the appropriate PBR material textures
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

                // Draw this material group
                m_multiModel->mesh_groups[i].mesh.draw();
            }
        }
    }
}

// Make sphere mesh
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

// Rebuild mesh shapes
void Station::rebuildMeshes() {
    m_junctionMesh = createSphereMesh(m_renderParams.junctionRadius, 16, 16);
    m_meshNeedsRebuild = false;
}

// Initialize L-System (delegates to LSystem)
void Station::initializeLSystem() {
    m_lsystem.initialize();
}

// Regenerate (delegates to LSystem)
void Station::regenerate() {
    m_lsystem.regenerate();
}

// Get module color
glm::vec3 Station::getModuleColor(int moduleType) const {
    switch (moduleType) {
    case 0: return glm::vec3(0.8f, 0.8f, 0.8f);
    case 1: return glm::vec3(0.4f, 0.9f, 0.4f);
    case 2: return glm::vec3(0.4f, 0.6f, 0.9f);
    case 3: return glm::vec3(0.9f, 0.86f, 0.4f);
    default: return glm::vec3(0.7f, 0.7f, 0.7f);
    }
}

// Render station 3D view (now just junctions, modules are rendered via renderMultiMaterialModel)
void Station::render3DStation(const glm::mat4& view, const glm::mat4& proj, GLuint shader) {
    const auto& junctions = m_lsystem.getJunctions();

    if (!m_drawStation || junctions.empty()) return;
    if (m_meshNeedsRebuild) rebuildMeshes();

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, GL_FALSE, value_ptr(proj));

    // Render junctions
    for (const auto& junction : junctions) {
        vec3 pos3D(junction.position.x, junction.verticalOffset, junction.position.y);
        mat4 modelTransform = translate(mat4(1.0f), pos3D);
        mat4 modelView = view * modelTransform;

        glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, GL_FALSE, value_ptr(modelView));
        vec3 junctionColor = (std::abs(junction.verticalOffset) > 0.1f) ? vec3(0.9f, 0.6f, 0.4f) : vec3(0.6f, 0.6f, 0.65f);
        glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(junctionColor));

        m_junctionMesh.draw();
    }
}

// Apply UI style
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

// Render controls panel
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
        ImGui::Text("Module Sizing (10 unit increments)");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderFloat("##BaseLength", &params.baseLength, 10.0f, 30.0f, "Base: %.0f");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Initial module length (10, 20, or 30)");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderFloat("##LengthDecay", &params.lengthDecay, 0.5f, 1.0f, "Decay: %.2f");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("How much modules shrink each generation");
        ImGui::Spacing();
        ImGui::TextWrapped("Note: Modules are quantized to 10, 20, or 30 units (1-3 model instances)");
        ImGui::Spacing();
    }

    // Topology
    if (ImGui::CollapsingHeader("Topology & Connections", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        ImGui::Checkbox("Allow Loop Connections", &params.allowLoops);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable additional connecting modules between nearby junctions to create loops");
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
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable modules that extend upward or downward (one level deep)");
        if (params.allowVerticalModules) {
            ImGui::Spacing();
            ImGui::Text("Vertical Probability");
            ImGui::PushItemWidth(-1);
            needsRegeneration |= ImGui::SliderFloat("##VertProb", &params.verticalProbability, 0.0f, 0.5f, "%.2f");
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chance of creating a vertical module (limited to one per branch)");
        }
        ImGui::Spacing();
    }

    // 3D rendering
    if (ImGui::CollapsingHeader("3D Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        ImGui::Checkbox("Enable 3D View", &m_drawStation);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Geometry");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##JunctionRadius", &m_renderParams.junctionRadius, 0.5f, 10.0f, "Junctions: %.1f")) {
            m_meshNeedsRebuild = true;
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Size of spherical junction connectors");
        ImGui::PushItemWidth(-1);
        ImGui::SliderFloat("##GapMult", &m_renderParams.gapMultiplier, 0.0f, 2.5f, "Gap: %.2fx");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Space between modules and junctions");
        ImGui::Spacing();
        if (ImGui::Button("Reset 3D Settings", ImVec2(-1, 0))) {
            m_renderParams.junctionRadius = 1.0f;
            m_renderParams.gapMultiplier = 0.0f;
            m_meshNeedsRebuild = true;
        }
        ImGui::Spacing();
    }

    // Materials
    if (ImGui::CollapsingHeader("Model Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        ImGui::TextWrapped("Modules are now rendered using the multi-material PBR model.");
        ImGui::Spacing();

        if (m_modelLoaded) {
            ImGui::Text("Materials assigned in pattern: 0,1,2,0,1,2...");
            ImGui::BulletText("0 = Gold");
            ImGui::BulletText("1 = Plastic");
            ImGui::BulletText("2 = Cloth");
            ImGui::Spacing();
            ImGui::TextWrapped("Each module displays 1-3 instances of the model based on length.");
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Model not loaded");
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
        int totalModelInstances = 0;
        for (const auto& module : modules) {
            if (module.isVertical) verticalCount++;
            totalModelInstances += module.modelCount;
        }

        ImGui::Text("Model Instances:");
        ImGui::NextColumn();
        ImGui::Text("%d", totalModelInstances);
        ImGui::NextColumn();

        ImGui::Text("Vertical Modules:");
        ImGui::NextColumn();
        ImGui::Text("%d", verticalCount);
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

// Render GUI
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

// Render preview panel
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
        "Colored lines represent functional modules, gray circles show horizontal junctions, "
        "and orange circles show vertical connection points. Each module is rendered as 1-3 model instances.");
}

// Calculate bounds
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

// Draw preview
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