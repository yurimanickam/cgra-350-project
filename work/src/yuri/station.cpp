#include "station.hpp"
#include <algorithm>
#include <cmath>
#include <cgra/cgra_mesh.hpp>
#include <cgra/cgra_geometry.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>

using namespace glm;

namespace {
    constexpr float HALF_PI = glm::half_pi<float>();
    constexpr float TWO_PI = glm::two_pi<float>();
    constexpr float PI = glm::pi<float>();

    // Module type colors for visualization
    const ImU32 MODULE_COLORS[] = {
        IM_COL32(255, 255, 255, 255), // Corridor - white
        IM_COL32(100, 255, 100, 255), // Habitat - green
        IM_COL32(100, 100, 255, 255), // Docking - blue
        IM_COL32(255, 255, 100, 255)  // Power - yellow
    };

    constexpr int NUM_MODULE_TYPES = 4;

    // UI styling constants
    const ImVec4 COLOR_HEADER = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    const ImVec4 COLOR_ACTIVE = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    const ImVec4 COLOR_HOVER = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
}

Station::Station() {
    initializeLSystem();
    rebuildMeshes();
}

cgra::gl_mesh Station::createSphereMesh(float radius, int stacks, int slices) {
    using namespace cgra;
    mesh_builder builder(GL_TRIANGLES);

    for (int i = 0; i <= stacks; ++i) {
        const float phi = PI * float(i) / float(stacks);
        const float y = radius * std::cos(phi);
        const float radiusAtY = radius * std::sin(phi);

        for (int j = 0; j <= slices; ++j) {
            const float theta = TWO_PI * float(j) / float(slices);
            const float x = radiusAtY * std::cos(theta);
            const float z = radiusAtY * std::sin(theta);

            const vec3 pos(x, y, z);
            const vec3 normal = normalize(pos);
            const vec2 uv(float(j) / slices, float(i) / stacks);

            builder.vertices.push_back({ pos, normal, uv });
        }
    }

    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            const int curr = i * (slices + 1) + j;
            const int next = curr + slices + 1;

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
    m_cylinderMesh = createCylinderMesh(m_renderParams.tubeRadius, 1.0f, 16, false);
    m_nodeMesh = createSphereMesh(m_renderParams.nodeRadius, 16, 16);
    m_meshNeedsRebuild = false;
}

cgra::gl_mesh Station::createCylinderMesh(float radius, float length, int subdivisions, bool capped) {
    using namespace cgra;

    mesh_builder builder(GL_TRIANGLES);
    const float halfLength = length / 2.0f;
    const float deltaTheta = TWO_PI / float(subdivisions);

    for (int i = 0; i <= subdivisions; ++i) {
        const float theta = i * deltaTheta;
        const float y = radius * std::cos(theta);
        const float z = radius * std::sin(theta);
        const vec3 normal = normalize(vec3(0, y, z));
        const float u = float(i) / subdivisions;

        builder.vertices.push_back({ vec3(-halfLength, y, z), normal, vec2(u, 0) });
        builder.vertices.push_back({ vec3(+halfLength, y, z), normal, vec2(u, 1) });
    }

    for (int i = 0; i < subdivisions; ++i) {
        const int idx = i * 2;
        builder.indices.push_back(idx);
        builder.indices.push_back(idx + 1);
        builder.indices.push_back(idx + 2);

        builder.indices.push_back(idx + 1);
        builder.indices.push_back(idx + 3);
        builder.indices.push_back(idx + 2);
    }

    if (capped) {
        auto addCap = [&](float x, vec3 normal, bool reverseWinding) {
            const int centerIdx = builder.vertices.size();
            builder.vertices.push_back({ vec3(x, 0, 0), normal, vec2(0.5f, 0.5f) });

            for (int i = 0; i <= subdivisions; ++i) {
                const float theta = i * deltaTheta;
                const float y = radius * std::cos(theta);
                const float z = radius * std::sin(theta);
                const vec2 uv(0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta));
                builder.vertices.push_back({ vec3(x, y, z), normal, uv });
            }

            for (int i = 0; i < subdivisions; ++i) {
                if (reverseWinding) {
                    builder.indices.push_back(centerIdx);
                    builder.indices.push_back(centerIdx + i + 2);
                    builder.indices.push_back(centerIdx + i + 1);
                }
                else {
                    builder.indices.push_back(centerIdx);
                    builder.indices.push_back(centerIdx + i + 1);
                    builder.indices.push_back(centerIdx + i + 2);
                }
            }
            };

        addCap(-halfLength, vec3(-1, 0, 0), false);
        addCap(+halfLength, vec3(1, 0, 0), true);
    }

    return builder.build();
}

void Station::initializeLSystem() {
    m_rng.seed(m_params.seed);
    setupRules();
    regenerate();
}

void Station::setupRules() {
    m_rules.clear();

    m_rules.push_back({
        'X',
        {
            "F[+XL][-XR]FX",
            "F[++XL][--XR]X",
            "FF[+X]X",
            "F[+XL]F[-XR]X",
            "FFF[+X][-X]X"
        },
        1.0f
        });

    m_rules.push_back({ 'F', { "F" }, 1.0f });

    m_rules.push_back({
        'L',
        { "F", "FF", "F[+F]", "" },
        0.8f
        });

    m_rules.push_back({
        'R',
        { "F", "FF", "F[-F]", "" },
        0.8f
        });
}

void Station::regenerate() {
    m_rng.seed(m_params.seed);
    generateSequence();
    interpretSequence(m_currentSequence);
}

void Station::generateSequence() {
    m_currentSequence = "X";

    for (int i = 0; i < m_params.iterations; ++i) {
        m_currentSequence = applyRules(m_currentSequence);
    }
}

std::string Station::applyRules(const std::string& current) {
    std::string result;
    result.reserve(current.size() * 2);

    for (char c : current) {
        bool ruleApplied = false;

        for (const auto& rule : m_rules) {
            if (rule.symbol == c) {
                if (getRandomFloat(0.0f, 1.0f) < rule.probability && !rule.productions.empty()) {
                    const int idx = getRandomInt(0, rule.productions.size() - 1);
                    result += rule.productions[idx];
                }
                else {
                    result += c;
                }
                ruleApplied = true;
                break;
            }
        }

        if (!ruleApplied) {
            result += c;
        }
    }

    return result;
}

void Station::interpretSequence(const std::string& sequence) {
    m_nodes.clear();
    m_connections.clear();
    m_stateStack.clear();

    TurtleState state;
    state.position = vec2(0.0f);
    state.angle = 0.0f;
    state.length = m_params.baseLength;
    state.generation = 0;

    int currentNodeIndex = -1;

    for (char command : sequence) {
        switch (command) {
        case 'F': {
            const float actualLength = std::max(m_params.minLength, state.length);
            const vec2 direction(std::cos(state.angle), std::sin(state.angle));
            const vec2 newPos = state.position + direction * actualLength;

            if (!isOverlapping(newPos, m_params.minLength)) {
                LSystemNode node;
                node.position = newPos;
                node.rotation = state.angle;
                node.length = actualLength;
                node.generation = state.generation;
                node.moduleType = getRandomInt(0, NUM_MODULE_TYPES - 1);

                const int newNodeIndex = m_nodes.size();
                m_nodes.push_back(node);

                if (currentNodeIndex >= 0) {
                    addConnection(currentNodeIndex, newNodeIndex);
                }

                if (m_params.allowLoops &&
                    getRandomFloat(0.0f, 1.0f) < m_params.connectionProbability) {
                    const int nearNode = findNearestNode(newPos, actualLength * 2.0f);
                    if (nearNode >= 0 && nearNode != newNodeIndex && nearNode != currentNodeIndex) {
                        addConnection(newNodeIndex, nearNode);
                    }
                }

                state.position = newPos;
                state.length *= m_params.lengthDecay;
                currentNodeIndex = newNodeIndex;
            }
            break;
        }
        case '+':
            state.angle += HALF_PI;
            if (state.angle >= TWO_PI) state.angle -= TWO_PI;
            break;

        case '-':
            state.angle -= HALF_PI;
            if (state.angle < 0.0f) state.angle += TWO_PI;
            break;

        case '[':
            m_stateStack.push_back(state);
            break;

        case ']':
            if (!m_stateStack.empty()) {
                state = m_stateStack.back();
                m_stateStack.pop_back();
                currentNodeIndex = findNearestNode(state.position, 0.1f);
            }
            break;

        default:
            break;
        }
    }
}

void Station::addConnection(int from, int to) {
    if (from < 0 || to < 0 || from >= m_nodes.size() || to >= m_nodes.size()) {
        return;
    }

    for (const auto& conn : m_connections) {
        if ((conn.first == from && conn.second == to) ||
            (conn.first == to && conn.second == from)) {
            return;
        }
    }

    m_connections.push_back({ from, to });
}

bool Station::isOverlapping(const vec2& pos, float minDist) const {
    for (const auto& node : m_nodes) {
        if (length(node.position - pos) < minDist) {
            return true;
        }
    }
    return false;
}

int Station::findNearestNode(const vec2& position, float maxDistance) const {
    int nearest = -1;
    float minDist = maxDistance;

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        const float dist = length(m_nodes[i].position - position);
        if (dist < minDist) {
            minDist = dist;
            nearest = static_cast<int>(i);
        }
    }

    return nearest;
}

float Station::getRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(m_rng);
}

int Station::getRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(m_rng);
}

glm::vec3 Station::getModuleColor(int moduleType) const {
    switch (moduleType) {
    case 0: return glm::vec3(0.9f, 0.9f, 0.9f); // Corridor - white
    case 1: return glm::vec3(0.4f, 0.9f, 0.4f); // Habitat - green
    case 2: return glm::vec3(0.4f, 0.4f, 0.9f); // Docking - blue
    case 3: return glm::vec3(0.9f, 0.9f, 0.4f); // Power - yellow
    default: return glm::vec3(0.7f, 0.7f, 0.7f);
    }
}

glm::mat4 Station::calculateConnectionTransform(const LSystemNode& from, const LSystemNode& to, float gapSize) const {
    const vec3 fromPos3D(from.position.x, 0.0f, from.position.y);
    const vec3 toPos3D(to.position.x, 0.0f, to.position.y);

    const vec3 direction = toPos3D - fromPos3D;
    const float fullDistance = length(direction);

    const float actualLength = std::max(0.1f, fullDistance - 2.0f * gapSize);

    const vec3 normalizedDir = normalize(direction);
    const vec3 centerPos = fromPos3D + normalizedDir * (gapSize + actualLength * 0.5f);

    const vec3 defaultDir(1.0f, 0.0f, 0.0f);

    mat4 transform = translate(mat4(1.0f), centerPos);

    const vec3 rotAxis = cross(defaultDir, normalizedDir);
    const float rotAxisLen = length(rotAxis);

    if (rotAxisLen > 0.001f) {
        const float angle = std::acos(glm::clamp(dot(defaultDir, normalizedDir), -1.0f, 1.0f));
        transform = rotate(transform, angle, normalize(rotAxis));
    }
    else if (dot(defaultDir, normalizedDir) < 0.0f) {
        transform = rotate(transform, PI, vec3(0.0f, 1.0f, 0.0f));
    }

    transform = scale(transform, vec3(actualLength, 1.0f, 1.0f));

    return transform;
}

void Station::render3DStation(const glm::mat4& view, const glm::mat4& proj, GLuint shader) {
    if (!m_drawStation || m_nodes.empty()) {
        return;
    }

    if (m_meshNeedsRebuild) {
        rebuildMeshes();
    }

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, GL_FALSE, value_ptr(proj));

    const float gapSize = m_renderParams.nodeRadius * m_renderParams.gapMultiplier;

    for (const auto& conn : m_connections) {
        const LSystemNode& fromNode = m_nodes[conn.first];
        const LSystemNode& toNode = m_nodes[conn.second];

        const mat4 modelTransform = calculateConnectionTransform(fromNode, toNode, gapSize);
        const mat4 modelView = view * modelTransform;

        glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, GL_FALSE, value_ptr(modelView));

        const vec3 color = (getModuleColor(fromNode.moduleType) + getModuleColor(toNode.moduleType)) * 0.5f;
        glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));

        m_cylinderMesh.draw();
    }

    for (const auto& node : m_nodes) {
        const vec3 pos3D(node.position.x, 0.0f, node.position.y);
        const mat4 modelTransform = translate(mat4(1.0f), pos3D);
        const mat4 modelView = view * modelTransform;

        glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, GL_FALSE, value_ptr(modelView));

        const vec3 color = getModuleColor(node.moduleType);
        glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));

        m_nodeMesh.draw();
    }
}

void Station::renderGUI() {
    applyUIStyle();

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(900, 720), ImGuiSetCond_FirstUseEver);

    ImGui::Begin("Space Station Generator", nullptr, ImGuiWindowFlags_NoCollapse);

    // Create two columns: controls on left, preview on right
    ImGui::BeginChild("LeftPane", ImVec2(420, 0), true);
    renderControlsPanel();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("RightPane", ImVec2(0, 0), true);
    renderPreviewPanel();
    ImGui::EndChild();

    ImGui::End();
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

    // === HEADER ===
    ImGui::PushStyleColor(ImGuiCol_Header, COLOR_HEADER);
    ImGui::Spacing();
    ImGui::Text("L-SYSTEM SPACE STATION GENERATOR");
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // === GENERATION PARAMETERS ===
    if (ImGui::CollapsingHeader("Generation Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();

        ImGui::Text("Complexity");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderInt("##Iterations", &m_params.iterations, 1, 6);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Number of L-System iterations (higher = more complex)");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Module Sizing");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderFloat("##BaseLength", &m_params.baseLength, 5.0f, 30.0f, "Base: %.1f");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Initial module length");
        }

        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderFloat("##LengthDecay", &m_params.lengthDecay, 0.5f, 1.0f, "Decay: %.2f");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("How much modules shrink each generation");
        }

        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderFloat("##MinLength", &m_params.minLength, 1.0f, 10.0f, "Minimum: %.1f");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Smallest allowed module size");
        }

        ImGui::Spacing();
    }

    // === TOPOLOGY ===
    if (ImGui::CollapsingHeader("Topology & Connections", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();

        ImGui::Checkbox("Allow Loop Connections", &m_params.allowLoops);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enable connections between nearby modules to create loops");
        }

        if (m_params.allowLoops) {
            ImGui::Spacing();
            ImGui::Text("Loop Probability");
            ImGui::PushItemWidth(-1);
            needsRegeneration |= ImGui::SliderFloat("##ConnProb", &m_params.connectionProbability, 0.0f, 0.5f, "%.2f");
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Chance of creating a loop connection");
            }
        }

        ImGui::Spacing();
    }

    // === 3D RENDERING ===
    if (ImGui::CollapsingHeader("3D Rendering", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();

        ImGui::Checkbox("Enable 3D View", &m_drawStation);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Geometry");
        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##NodeRadius", &m_renderParams.nodeRadius, 0.5f, 10.0f, "Nodes: %.1f")) {
            m_meshNeedsRebuild = true;
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Size of spherical junction nodes");
        }

        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##TubeRadius", &m_renderParams.tubeRadius, 0.2f, 5.0f, "Tubes: %.1f")) {
            m_meshNeedsRebuild = true;
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Thickness of connecting tubes");
        }

        ImGui::PushItemWidth(-1);
        ImGui::SliderFloat("##GapMult", &m_renderParams.gapMultiplier, 0.5f, 2.5f, "Gap: %.2fx");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Space between tubes and nodes");
        }

        ImGui::Spacing();

        if (ImGui::Button("Reset 3D Settings", ImVec2(-1, 0))) {
            m_renderParams.nodeRadius = 3.0f;
            m_renderParams.tubeRadius = 1.5f;
            m_renderParams.gapMultiplier = 1.2f;
            m_meshNeedsRebuild = true;
        }

        ImGui::Spacing();
    }

    // === RANDOM SEED ===
    if (ImGui::CollapsingHeader("Random Seed", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();

        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderInt("##Seed", &m_params.seed, 1, 99999);
        ImGui::PopItemWidth();

        ImGui::Spacing();

        if (ImGui::Button("New Random Seed", ImVec2(-1, 0))) {
            m_params.seed = getRandomInt(1, 99999);
            needsRegeneration = true;
        }

        ImGui::Spacing();
    }

    // === GENERATE BUTTON ===
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, COLOR_ACTIVE);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, COLOR_HOVER);
    if (ImGui::Button("GENERATE STATION", ImVec2(-1, 40))) {
        needsRegeneration = true;
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // === STATISTICS ===
    if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();

        ImGui::Columns(2, "stats", false);

        ImGui::Text("Total Nodes:");
        ImGui::NextColumn();
        ImGui::Text("%zu", m_nodes.size());
        ImGui::NextColumn();

        ImGui::Text("Connections:");
        ImGui::NextColumn();
        ImGui::Text("%zu", m_connections.size());
        ImGui::NextColumn();

        ImGui::Text("Sequence Length:");
        ImGui::NextColumn();
        ImGui::Text("%zu", m_currentSequence.length());
        ImGui::NextColumn();

        ImGui::Columns(1);

        ImGui::Spacing();
        ImGui::Text("Module Breakdown");
        ImGui::Separator();

        std::vector<int> moduleCounts(NUM_MODULE_TYPES, 0);
        for (const auto& node : m_nodes) {
            if (node.moduleType < NUM_MODULE_TYPES) {
                moduleCounts[node.moduleType]++;
            }
        }

        const char* moduleNames[] = { "Corridors", "Habitats", "Docking", "Power" };
        const ImU32 moduleColors[] = {
            IM_COL32(200, 200, 200, 255),
            IM_COL32(100, 255, 100, 255),
            IM_COL32(100, 100, 255, 255),
            IM_COL32(255, 255, 100, 255)
        };

        for (int i = 0; i < NUM_MODULE_TYPES; ++i) {
            ImGui::BulletText("%s:", moduleNames[i]);
            ImGui::SameLine(140);

            int color = moduleColors[i];
            float r = ((color >> 0) & 0xFF) / 255.0f;
            float g = ((color >> 8) & 0xFF) / 255.0f;
            float b = ((color >> 16) & 0xFF) / 255.0f;

            ImGui::TextColored(ImVec4(r, g, b, 1.0f), "%d", moduleCounts[i]);
        }

        ImGui::Spacing();
    }

    if (needsRegeneration) {
        regenerate();
    }
}

void Station::renderPreviewPanel() {
    ImGui::Spacing();
    ImGui::Text("STATION LAYOUT PREVIEW");
    ImGui::Separator();
    ImGui::Spacing();

    // Zoom controls
    ImGui::Columns(3, "zoom", false);
    if (ImGui::Button("[-] Zoom Out", ImVec2(-1, 0))) {
        m_previewZoom = glm::clamp(m_previewZoom / 1.2f, 0.2f, 8.0f);
    }
    ImGui::NextColumn();

    if (ImGui::Button("[=] Reset", ImVec2(-1, 0))) {
        m_previewZoom = 1.0f;
        m_previewPan = vec2(0.0f);
    }
    ImGui::NextColumn();

    if (ImGui::Button("[+] Zoom In", ImVec2(-1, 0))) {
        m_previewZoom = glm::clamp(m_previewZoom * 1.2f, 0.2f, 8.0f);
    }
    ImGui::NextColumn();
    ImGui::Columns(1);

    ImGui::Text("Zoom: %.1fx", m_previewZoom);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Legend
    ImGui::Text("Module Types:");
    ImGui::Columns(2, "legend", false);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float legendSize = 12.0f;

    const char* moduleNames[] = { "Corridor", "Habitat", "Docking", "Power" };
    for (int i = 0; i < NUM_MODULE_TYPES; ++i) {
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        draw_list->AddCircleFilled(
            ImVec2(cursor.x + legendSize * 0.5f, cursor.y + legendSize * 0.5f),
            legendSize * 0.5f,
            MODULE_COLORS[i]
        );
        ImGui::Dummy(ImVec2(legendSize, legendSize));
        ImGui::SameLine();
        ImGui::Text("%s", moduleNames[i]);
        ImGui::NextColumn();
    }
    ImGui::Columns(1);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Main preview canvas
    drawVisualization();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextWrapped("Tip: Use the zoom controls to explore the station layout. "
        "Colored circles represent different module types, and lines show connections.");
}

void Station::calculateBounds(vec2& minBounds, vec2& maxBounds) const {
    minBounds = vec2(FLT_MAX);
    maxBounds = vec2(-FLT_MAX);

    for (const auto& node : m_nodes) {
        minBounds = glm::min(minBounds, node.position);
        maxBounds = glm::max(maxBounds, node.position);
    }

    const vec2 padding(25.0f);
    minBounds -= padding;
    maxBounds += padding;
}

void Station::drawVisualization() {
    const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    const ImVec2 available = ImGui::GetContentRegionAvail();
    const ImVec2 canvas_size(
        std::max(300.0f, available.x),
        std::max(400.0f, available.y - 80.0f)
    );

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Calculate bounds
    vec2 minBounds, maxBounds;
    calculateBounds(minBounds, maxBounds);
    const vec2 worldSize = maxBounds - minBounds;

    // Calculate scale
    const float fitScale = (worldSize.x > 0.0f && worldSize.y > 0.0f)
        ? std::min(canvas_size.x / worldSize.x, canvas_size.y / worldSize.y) * 0.95f
        : 1.0f;
    const float scale = fitScale * m_previewZoom;

    // Center with pan offset
    const vec2 offset(
        (canvas_size.x - (worldSize.x * scale)) * 0.5f + m_previewPan.x,
        (canvas_size.y - (worldSize.y * scale)) * 0.5f + m_previewPan.y
    );

    // Draw background with grid
    draw_list->AddRectFilled(
        canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(20, 20, 25, 255)
    );

    // Draw border
    draw_list->AddRect(
        canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(100, 100, 120, 255),
        0.0f,
        0,
        2.0f
    );

    // Draw grid lines
    const float gridSpacing = 50.0f * scale;
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

    // Lambda to convert world to screen
    auto worldToScreen = [&](const vec2& worldPos) -> ImVec2 {
        return ImVec2(
            canvas_pos.x + offset.x + (worldPos.x - minBounds.x) * scale,
            canvas_pos.y + offset.y + (worldPos.y - minBounds.y) * scale
        );
        };

    // Draw connections with anti-aliasing
    for (const auto& conn : m_connections) {
        const ImVec2 p1 = worldToScreen(m_nodes[conn.first].position);
        const ImVec2 p2 = worldToScreen(m_nodes[conn.second].position);
        draw_list->AddLine(p1, p2, IM_COL32(80, 80, 90, 255), 3.0f);
    }

    // Draw nodes
    const float baseNodeRadius = 10.0f;
    const float nodeRadius = baseNodeRadius * glm::clamp(m_previewZoom, 0.5f, 2.0f);

    for (const auto& node : m_nodes) {
        const ImVec2 center = worldToScreen(node.position);

        const ImU32 color = (node.moduleType >= 0 && node.moduleType < NUM_MODULE_TYPES)
            ? MODULE_COLORS[node.moduleType]
            : IM_COL32(255, 100, 100, 255);

        // Draw shadow
        draw_list->AddCircleFilled(
            ImVec2(center.x + 1, center.y + 1),
            nodeRadius,
            IM_COL32(0, 0, 0, 80),
            16
        );

        // Draw node
        draw_list->AddCircleFilled(center, nodeRadius, color, 16);
        draw_list->AddCircle(center, nodeRadius, IM_COL32(0, 0, 0, 200), 16, 2.0f);

        // Draw direction indicator
        if (nodeRadius > 5.0f) {
            const float dirLen = nodeRadius * 0.75f;
            const ImVec2 dirEnd(
                center.x + std::cos(node.rotation) * dirLen,
                center.y + std::sin(node.rotation) * dirLen
            );
            draw_list->AddLine(center, dirEnd, IM_COL32(0, 0, 0, 255), 2.5f);
        }
    }

    // Handle mouse interaction for panning
    ImGui::SetCursorScreenPos(canvas_pos);
    ImGui::InvisibleButton("canvas", canvas_size);

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_previewPan.x += delta.x;
        m_previewPan.y += delta.y;
    }

    // Mouse wheel zoom
    if (ImGui::IsItemHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_previewZoom = glm::clamp(m_previewZoom * (1.0f + wheel * 0.1f), 0.2f, 8.0f);
        }
    }
}

void Station::renderControlsGUI() {
    // Legacy function - now handled by renderControlsPanel()
}

void Station::renderPreviewGUI() {
    // Legacy function - now handled by renderPreviewPanel()
}