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
}

Station::Station() {
    initializeLSystem();
    initializeMeshes();
}

void Station::initializeMeshes() {
    // Create cylinder mesh (horizontal, along X-axis)
    m_cylinderMesh = createCylinderMesh(m_cylinderRadius, 1.0f, 16, false);

    // Create sphere mesh for nodes
    using namespace cgra;
    mesh_builder builder(GL_TRIANGLES);
    const int stacks = 16;
    const int slices = 16;

    for (int i = 0; i <= stacks; ++i) {
        const float phi = PI * float(i) / float(stacks);
        const float y = m_nodeRadius * std::cos(phi);
        const float radiusAtY = m_nodeRadius * std::sin(phi);

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

    m_nodeMesh = builder.build();
}

cgra::gl_mesh Station::createCylinderMesh(float radius, float length, int subdivisions, bool capped) {
    using namespace cgra;

    mesh_builder builder(GL_TRIANGLES);
    const float halfLength = length / 2.0f;
    const float deltaTheta = TWO_PI / float(subdivisions);

    // Generate side vertices (cylinder along the X axis)
    for (int i = 0; i <= subdivisions; ++i) {
        const float theta = i * deltaTheta;
        const float y = radius * std::cos(theta);
        const float z = radius * std::sin(theta);
        const vec3 normal = normalize(vec3(0, y, z));
        const float u = float(i) / subdivisions;

        builder.vertices.push_back({ vec3(-halfLength, y, z), normal, vec2(u, 0) });
        builder.vertices.push_back({ vec3(+halfLength, y, z), normal, vec2(u, 1) });
    }

    // Generate side indices
    for (int i = 0; i < subdivisions; ++i) {
        const int idx = i * 2;
        builder.indices.push_back(idx);
        builder.indices.push_back(idx + 1);
        builder.indices.push_back(idx + 2);

        builder.indices.push_back(idx + 1);
        builder.indices.push_back(idx + 3);
        builder.indices.push_back(idx + 2);
    }

    // Generate caps if requested
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

    // Main expansion rule - creates branching structure
    m_rules.push_back({
        'X',
        {
            "F[+XL][-XR]FX",     // Standard branching
            "F[++XL][--XR]X",    // Wider angle branches
            "FF[+X]X",           // Simple forward with branch
            "F[+XL]F[-XR]X",     // Alternating branches
            "FFF[+X][-X]X"       // Long corridor with branches
        },
        1.0f
        });

    // Forward movement (terminal)
    m_rules.push_back({ 'F', { "F" }, 1.0f });

    // Left module rule
    m_rules.push_back({
        'L',
        { "F", "FF", "F[+F]", "" },
        0.8f
        });

    // Right module rule
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
    m_currentSequence = "X"; // Axiom

    for (int i = 0; i < m_params.iterations; ++i) {
        m_currentSequence = applyRules(m_currentSequence);
    }
}

std::string Station::applyRules(const std::string& current) {
    std::string result;
    result.reserve(current.size() * 2); // Pre-allocate for efficiency

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
            result += c; // Keep symbols without rules
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

                // Connect to previous node
                if (currentNodeIndex >= 0) {
                    addConnection(currentNodeIndex, newNodeIndex);
                }

                // Potential loop connections
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
            state.angle += HALF_PI; // Turn left 90 degrees
            if (state.angle >= TWO_PI) state.angle -= TWO_PI;
            break;

        case '-':
            state.angle -= HALF_PI; // Turn right 90 degrees
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
            // Non-terminal symbols (X, L, R) are ignored
            break;
        }
    }
}

void Station::addConnection(int from, int to) {
    if (from < 0 || to < 0 || from >= m_nodes.size() || to >= m_nodes.size()) {
        return;
    }

    // Check if connection already exists
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
    // Convert 2D positions to 3D (Y becomes the vertical axis)
    const vec3 fromPos3D(from.position.x, 0.0f, from.position.y);
    const vec3 toPos3D(to.position.x, 0.0f, to.position.y);

    // Calculate direction and distance
    const vec3 direction = toPos3D - fromPos3D;
    const float fullDistance = length(direction);

    // Account for gaps at both ends
    const float actualLength = std::max(0.1f, fullDistance - 2.0f * gapSize);

    // Calculate center position (accounting for gaps)
    const vec3 normalizedDir = normalize(direction);
    const vec3 centerPos = fromPos3D + normalizedDir * (gapSize + actualLength * 0.5f);

    // Calculate rotation to align cylinder with connection
    // Default cylinder is along X-axis (1, 0, 0)
    const vec3 defaultDir(1.0f, 0.0f, 0.0f);

    mat4 transform = translate(mat4(1.0f), centerPos);

    // Calculate rotation axis and angle
    const vec3 rotAxis = cross(defaultDir, normalizedDir);
    const float rotAxisLen = length(rotAxis);

    if (rotAxisLen > 0.001f) {
        const float angle = std::acos(glm::clamp(dot(defaultDir, normalizedDir), -1.0f, 1.0f));
        transform = rotate(transform, angle, normalize(rotAxis));
    }
    else if (dot(defaultDir, normalizedDir) < 0.0f) {
        // 180 degree rotation needed
        transform = rotate(transform, PI, vec3(0.0f, 1.0f, 0.0f));
    }

    // Scale to match connection length
    transform = scale(transform, vec3(actualLength, 1.0f, 1.0f));

    return transform;
}

void Station::render3DStation(const glm::mat4& view, const glm::mat4& proj, GLuint shader) {
    if (!m_drawStation || m_nodes.empty()) {
        return;
    }

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, GL_FALSE, value_ptr(proj));

    const float gapSize = m_nodeRadius * 1.2f; // Gap for node spheres

    // Draw connections as cylinders
    for (const auto& conn : m_connections) {
        const LSystemNode& fromNode = m_nodes[conn.first];
        const LSystemNode& toNode = m_nodes[conn.second];

        const mat4 modelTransform = calculateConnectionTransform(fromNode, toNode, gapSize);
        const mat4 modelView = view * modelTransform;

        glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, GL_FALSE, value_ptr(modelView));

        // Use average color of connected nodes
        const vec3 color = (getModuleColor(fromNode.moduleType) + getModuleColor(toNode.moduleType)) * 0.5f;
        glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));

        m_cylinderMesh.draw();
    }

    // Draw nodes as spheres
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
    renderControlsGUI();
    renderPreviewGUI();
}

void Station::renderControlsGUI() {
    ImGui::SetNextWindowPos(ImVec2(830, 5), ImGuiSetCond_Once);
    ImGui::SetNextWindowSize(ImVec2(450, 450), ImGuiSetCond_Once);
    ImGui::Begin("L-System Space Station");

    bool needsRegeneration = false;

    ImGui::Text("3D Rendering");
    ImGui::Separator();
    ImGui::Checkbox("Draw 3D Station", &m_drawStation);
    ImGui::SliderFloat("Cylinder Radius", &m_cylinderRadius, 0.5f, 5.0f);
    ImGui::SliderFloat("Node Radius", &m_nodeRadius, 1.0f, 8.0f);

    if (ImGui::Button("Rebuild Meshes")) {
        initializeMeshes();
    }

    ImGui::Spacing();
    ImGui::Text("Generation Parameters");
    ImGui::Separator();

    needsRegeneration |= ImGui::SliderInt("Iterations", &m_params.iterations, 1, 6);
    needsRegeneration |= ImGui::SliderFloat("Base Length", &m_params.baseLength, 5.0f, 30.0f);
    needsRegeneration |= ImGui::SliderFloat("Length Decay", &m_params.lengthDecay, 0.5f, 1.0f);
    needsRegeneration |= ImGui::SliderFloat("Min Length", &m_params.minLength, 1.0f, 10.0f);

    ImGui::Spacing();
    ImGui::Text("Connection Settings");
    ImGui::Separator();

    ImGui::Checkbox("Allow Loops", &m_params.allowLoops);
    needsRegeneration |= ImGui::SliderFloat("Connection Probability", &m_params.connectionProbability, 0.0f, 0.5f);

    ImGui::Spacing();
    ImGui::Text("Random Seed");
    ImGui::Separator();

    needsRegeneration |= ImGui::SliderInt("Seed", &m_params.seed, 1, 99999);

    if (ImGui::Button("Generate New Station", ImVec2(-1, 30))) {
        needsRegeneration = true;
    }

    if (needsRegeneration) {
        regenerate();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Statistics");
    ImGui::Separator();

    ImGui::Text("Nodes: %zu", m_nodes.size());
    ImGui::Text("Connections: %zu", m_connections.size());
    ImGui::Text("Sequence Length: %zu", m_currentSequence.length());

    // Module type breakdown
    std::vector<int> moduleCounts(NUM_MODULE_TYPES, 0);
    for (const auto& node : m_nodes) {
        if (node.moduleType < NUM_MODULE_TYPES) {
            moduleCounts[node.moduleType]++;
        }
    }

    ImGui::Text("Corridors: %d | Habitats: %d", moduleCounts[0], moduleCounts[1]);
    ImGui::Text("Docking: %d | Power: %d", moduleCounts[2], moduleCounts[3]);

    ImGui::End();
}

void Station::renderPreviewGUI() {
    ImGui::SetNextWindowPos(ImVec2(1290, 5), ImGuiSetCond_Once);
    ImGui::SetNextWindowSize(ImVec2(400, 450), ImGuiSetCond_Once);
    ImGui::Begin("Station Layout Preview");

    ImGui::Text("Zoom: %.2fx", m_previewZoom);

    if (ImGui::Button("Zoom In")) {
        m_previewZoom = glm::clamp(m_previewZoom * 1.2f, 0.2f, 8.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Zoom Out")) {
        m_previewZoom = glm::clamp(m_previewZoom / 1.2f, 0.2f, 8.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        m_previewZoom = 1.0f;
    }

    ImGui::Spacing();
    drawVisualization();

    ImGui::End();
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
    const ImVec2 window_size = ImGui::GetWindowSize();
    const ImVec2 canvas_size(
        std::max(200.0f, window_size.x - 20.0f),
        std::max(200.0f, window_size.y - 70.0f)
    );

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Calculate bounds
    vec2 minBounds, maxBounds;
    calculateBounds(minBounds, maxBounds);
    const vec2 worldSize = maxBounds - minBounds;

    // Calculate scale to fit station in canvas
    const float fitScale = (worldSize.x > 0.0f && worldSize.y > 0.0f)
        ? std::min(canvas_size.x / worldSize.x, canvas_size.y / worldSize.y)
        : 1.0f;
    const float scale = fitScale * m_previewZoom;

    // Center the visualization
    const vec2 offset(
        (canvas_size.x - (worldSize.x * scale)) * 0.5f,
        (canvas_size.y - (worldSize.y * scale)) * 0.5f
    );

    // Draw background
    draw_list->AddRectFilled(
        canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(32, 32, 32, 255)
    );

    // Lambda to convert world coordinates to screen coordinates
    auto worldToScreen = [&](const vec2& worldPos) -> ImVec2 {
        return ImVec2(
            canvas_pos.x + offset.x + (worldPos.x - minBounds.x) * scale,
            canvas_pos.y + offset.y + (worldPos.y - minBounds.y) * scale
        );
        };

    // Draw connections
    for (const auto& conn : m_connections) {
        const ImVec2 p1 = worldToScreen(m_nodes[conn.first].position);
        const ImVec2 p2 = worldToScreen(m_nodes[conn.second].position);
        draw_list->AddLine(p1, p2, IM_COL32(90, 90, 90, 255), 3.0f);
    }

    // Draw nodes
    constexpr float NODE_RADIUS = 8.0f;
    for (const auto& node : m_nodes) {
        const ImVec2 center = worldToScreen(node.position);

        // Get color based on module type
        const ImU32 color = (node.moduleType >= 0 && node.moduleType < NUM_MODULE_TYPES)
            ? MODULE_COLORS[node.moduleType]
            : IM_COL32(255, 100, 100, 255);

        draw_list->AddCircleFilled(center, NODE_RADIUS, color);
        draw_list->AddCircle(center, NODE_RADIUS, IM_COL32(0, 0, 0, 255), 0, 2.0f);

        // Draw direction indicator
        const float dirLen = NODE_RADIUS * 0.85f;
        const ImVec2 dirEnd(
            center.x + std::cos(node.rotation) * dirLen,
            center.y + std::sin(node.rotation) * dirLen
        );
        draw_list->AddLine(center, dirEnd, IM_COL32(0, 0, 0, 255), 2.0f);
    }

    ImGui::Dummy(canvas_size);
}