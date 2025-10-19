#include "station.hpp"
#include <vector>
#include <cmath>
#include <cgra/cgra_mesh.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <sstream>
#include <imgui.h>

cgra::gl_mesh Station::createCylinderMesh(float radius, float height, int subdivisions, bool capped) {
    using namespace glm;
    using namespace cgra;

    mesh_builder builder(GL_TRIANGLES);

    float halfHeight = height / 2.0f;
    float deltaTheta = 2.0f * glm::pi<float>() / float(subdivisions);

    // Vertices for the sides
    for (int i = 0; i <= subdivisions; ++i) {
        float theta = i * deltaTheta;
        float x = radius * std::cos(theta);
        float z = radius * std::sin(theta);

        vec3 normal = glm::normalize(vec3(x, 0, z));
        float u = float(i) / subdivisions;

        // Bottom vertex
        builder.vertices.push_back({ vec3(x, -halfHeight, z), normal, vec2(u, 0) });
        // Top vertex
        builder.vertices.push_back({ vec3(x, +halfHeight, z), normal, vec2(u, 1) });
    }

    // Indices for the sides
    for (int i = 0; i < subdivisions; ++i) {
        int idx = i * 2;
        // Each quad: 2 triangles
        builder.indices.push_back(idx);
        builder.indices.push_back(idx + 1);
        builder.indices.push_back(idx + 2);

        builder.indices.push_back(idx + 1);
        builder.indices.push_back(idx + 3);
        builder.indices.push_back(idx + 2);
    }

    // Caps
    if (capped) {
        int baseIndex = int(builder.vertices.size());

        // Bottom center
        builder.vertices.push_back({ vec3(0, -halfHeight, 0), vec3(0, -1, 0), vec2(0.5f, 0.5f) });
        int bottomCenterIdx = baseIndex;

        // Bottom rim
        for (int i = 0; i <= subdivisions; ++i) {
            float theta = i * deltaTheta;
            float x = radius * std::cos(theta);
            float z = radius * std::sin(theta);
            vec2 uv(0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta));
            builder.vertices.push_back({ vec3(x, -halfHeight, z), vec3(0, -1, 0), uv });
        }
        for (int i = 0; i < subdivisions; ++i) {
            builder.indices.push_back(bottomCenterIdx);
            builder.indices.push_back(bottomCenterIdx + i + 1);
            builder.indices.push_back(bottomCenterIdx + i + 2);
        }

        baseIndex = int(builder.vertices.size());
        // Top center
        builder.vertices.push_back({ vec3(0, +halfHeight, 0), vec3(0, 1, 0), vec2(0.5f, 0.5f) });
        int topCenterIdx = baseIndex;

        // Top rim
        for (int i = 0; i <= subdivisions; ++i) {
            float theta = i * deltaTheta;
            float x = radius * std::cos(theta);
            float z = radius * std::sin(theta);
            vec2 uv(0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta));
            builder.vertices.push_back({ vec3(x, +halfHeight, z), vec3(0, 1, 0), uv });
        }
        for (int i = 0; i < subdivisions; ++i) {
            builder.indices.push_back(topCenterIdx);
            builder.indices.push_back(topCenterIdx + i + 2);
            builder.indices.push_back(topCenterIdx + i + 1);
        }
    }

    return builder.build();
}

void Station::initializeLSystem() {
    m_rng.seed(m_lsystemParams.seed);
    m_axiom = "X";
    setupRules();
    generateLSystem();
}

void Station::setupRules() {
    m_rules.clear();

    // Main expansion rule - creates branching structure
    LSystemRule mainRule;
    mainRule.symbol = 'X';
    mainRule.productions = {
        "F[+XL][-XR]FX",     // Standard branching
        "F[++XL][--XR]X",    // Wider angle branches
        "FF[+X]X",           // Simple forward with branch
        "F[+XL]F[-XR]X",     // Alternating branches
        "FFF[+X][-X]X"       // Long corridor with branches
    };
    mainRule.probability = 1.0f;
    mainRule.lengthMultiplier = 1.0f;
    mainRule.angleVariation = 0.2f;
    m_rules.push_back(mainRule);

    // Forward movement rule
    LSystemRule forwardRule;
    forwardRule.symbol = 'F';
    forwardRule.productions = { "F" };
    forwardRule.probability = 1.0f;
    forwardRule.lengthMultiplier = 0.9f;
    forwardRule.angleVariation = 0.1f;
    m_rules.push_back(forwardRule);

    // Left module rule
    LSystemRule leftRule;
    leftRule.symbol = 'L';
    leftRule.productions = {
        "F",
        "FF",
        "F[+F]",
        "" // Sometimes terminate
    };
    leftRule.probability = 0.8f;
    leftRule.lengthMultiplier = 0.7f;
    leftRule.angleVariation = 0.3f;
    m_rules.push_back(leftRule);

    // Right module rule
    LSystemRule rightRule;
    rightRule.symbol = 'R';
    rightRule.productions = {
        "F",
        "FF",
        "F[-F]",
        "" // Sometimes terminate
    };
    rightRule.probability = 0.8f;
    rightRule.lengthMultiplier = 0.7f;
    rightRule.angleVariation = 0.3f;
    m_rules.push_back(rightRule);
}

void Station::generateLSystem() {
    m_currentSequence = m_axiom;

    for (int i = 0; i < m_lsystemParams.iterations; ++i) {
        m_currentSequence = applyRules(m_currentSequence);
    }

    interpretLSystem(m_currentSequence);
}

std::string Station::applyRules(const std::string& current) {
    std::string result;

    for (char c : current) {
        bool ruleFound = false;

        for (const auto& rule : m_rules) {
            if (rule.symbol == c) {
                if (getRandomFloat(0.0f, 1.0f) < rule.probability && !rule.productions.empty()) {
                    int productionIndex = getRandomInt(0, rule.productions.size() - 1);
                    result += rule.productions[productionIndex];
                }
                else {
                    result += c; // Keep original if rule doesn't fire
                }
                ruleFound = true;
                break;
            }
        }

        if (!ruleFound) {
            result += c; // Keep characters without rules
        }
    }

    return result;
}

void Station::interpretLSystem(const std::string& sequence) {
    m_nodes.clear();
    m_connections.clear();
    m_stateStack.clear();

    TurtleState currentState;
    currentState.position = glm::vec2(0.0f, 0.0f);
    currentState.angle = 0.0f;
    currentState.length = m_lsystemParams.baseLength;
    currentState.generation = 0;

    int currentNodeIndex = -1;

    for (size_t i = 0; i < sequence.length(); ++i) {
        char command = sequence[i];

        switch (command) {
        case 'F': {
            // Only allow exact 90 degree steps for the current angle
            // So, the angle is always 0, 90, 180, or 270 degrees (in radians)
            // This requires that + and - always change by 90 degrees exactly (see below in their cases)

            // Compute the new position with the current fixed angle
            float actualLength = std::max(m_lsystemParams.minLength, currentState.length);

            glm::vec2 direction(std::cos(currentState.angle), std::sin(currentState.angle));
            glm::vec2 newPos = currentState.position + direction * actualLength;

            float minNodeDistance = m_lsystemParams.minLength; // Use the minLength as the minimum distance

            if (!isOverlapping(newPos, minNodeDistance)) {
                LSystemNode node;
                node.position = newPos;
                node.rotation = currentState.angle;
                node.length = actualLength;
                node.generation = currentState.generation;
                node.moduleType = getRandomInt(0, 3); // Random module type

                int newNodeIndex = m_nodes.size();
                m_nodes.push_back(node);

                // Connect to previous node
                if (currentNodeIndex >= 0) {
                    addConnection(currentNodeIndex, newNodeIndex);
                }

                // Check for potential connections to nearby nodes
                if (m_lsystemParams.allowLoops && getRandomFloat(0.0f, 1.0f) < m_lsystemParams.connectionProbability) {
                    int nearNode = findNearestNode(newPos, actualLength * 2.0f);
                    if (nearNode >= 0 && nearNode != newNodeIndex && nearNode != currentNodeIndex) {
                        addConnection(newNodeIndex, nearNode);
                    }
                }

                currentState.position = newPos;
                currentState.length *= m_lsystemParams.lengthDecay;
                currentNodeIndex = newNodeIndex;
            }
            // If overlapping, do not place the node or advance the position
            break;
        }
        case '+': {
            // Turn left by exactly 90 degrees (in radians)
            currentState.angle += glm::half_pi<float>(); // pi/2 radians = 90 degrees
            // Normalize the angle to [0, 2pi) for consistency
            if (currentState.angle >= glm::two_pi<float>()) currentState.angle -= glm::two_pi<float>();
            break;
        }
        case '-': {
            // Turn right by exactly 90 degrees (in radians)
            currentState.angle -= glm::half_pi<float>();
            if (currentState.angle < 0.0f) currentState.angle += glm::two_pi<float>();
            break;
        }
        case '[': {
            // Push state
            m_stateStack.push_back(currentState);
            break;
        }
        case ']': {
            // Pop state
            if (!m_stateStack.empty()) {
                currentState = m_stateStack.back();
                m_stateStack.pop_back();
                // Find the node at this position to reconnect
                currentNodeIndex = findNearestNode(currentState.position, 0.1f);
            }
            break;
        }
        case 'X':
        case 'L':
        case 'R':
            // Non-terminal symbols, ignore in interpretation
            break;
        }
    }
}

void Station::addConnection(int from, int to) {
    if (from >= 0 && to >= 0 && from < m_nodes.size() && to < m_nodes.size()) {
        // Check if connection already exists
        for (const auto& conn : m_connections) {
            if ((conn.first == from && conn.second == to) ||
                (conn.first == to && conn.second == from)) {
                return; // Connection already exists
            }
        }
        m_connections.push_back({ from, to });
    }
}

bool Station::canConnect(const glm::vec2& pos1, const glm::vec2& pos2) {
    float distance = glm::length(pos1 - pos2);
    return distance > 0.1f && distance < m_lsystemParams.baseLength * 3.0f;
}

int Station::findNearestNode(const glm::vec2& position, float maxDistance) {
    int nearest = -1;
    float minDist = maxDistance;

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        float dist = glm::length(m_nodes[i].position - position);
        if (dist < minDist) {
            minDist = dist;
            nearest = i;
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

bool Station::isOverlapping(const glm::vec2& pos, float minDist) {
    for (const auto& node : m_nodes) {
        if (glm::length(node.position - pos) < minDist) {
            return true;
        }
    }
    return false;
}



void Station::renderLSystemGUI() {
    // Make all L-system controls in their own window, with a big preview canvas

    // Parameters/controls window
    ImGui::SetNextWindowPos(ImVec2(830, 5), ImGuiSetCond_Once);
    ImGui::SetNextWindowSize(ImVec2(500, 360), ImGuiSetCond_Once);
    ImGui::Begin("L-System Space Station Controls", 0);

    bool regenerate = false;

    if (ImGui::SliderInt("Iterations", &m_lsystemParams.iterations, 1, 6)) {
        regenerate = true;
    }

    if (ImGui::SliderFloat("Base Length", &m_lsystemParams.baseLength, 5.0f, 30.0f)) {
        regenerate = true;
    }

    if (ImGui::SliderFloat("Length Variation", &m_lsystemParams.lengthVariation, 0.0f, 0.8f)) {
        regenerate = true;
    }

    if (ImGui::SliderFloat("Base Angle", &m_lsystemParams.baseAngle, 30.0f, 120.0f)) {
        regenerate = true;
    }

    if (ImGui::SliderFloat("Angle Variation", &m_lsystemParams.angleVariation, 0.0f, 45.0f)) {
        regenerate = true;
    }

    if (ImGui::SliderFloat("Branch Probability", &m_lsystemParams.branchProbability, 0.0f, 1.0f)) {
        regenerate = true;
    }

    if (ImGui::SliderFloat("Connection Probability", &m_lsystemParams.connectionProbability, 0.0f, 0.5f)) {
        regenerate = true;
    }

    if (ImGui::SliderFloat("Length Decay", &m_lsystemParams.lengthDecay, 0.5f, 1.0f)) {
        regenerate = true;
    }

    if (ImGui::SliderInt("Max Connections", &m_lsystemParams.maxConnections, 1, 8)) {
        regenerate = true;
    }

    ImGui::Checkbox("Allow Loops", &m_lsystemParams.allowLoops);

    if (ImGui::SliderInt("Seed", &m_lsystemParams.seed, 1, 99999)) {
        regenerate = true;
    }

    if (ImGui::Button("Generate New Station") || regenerate) {
        initializeLSystem();
    }

    ImGui::Separator();

    // Display current sequence
    ImGui::Text("Generated Sequence:");
    ImGui::TextWrapped("%s", m_currentSequence.c_str());

    ImGui::Separator();

    // Display statistics
    ImGui::Text("Station Statistics:");
    ImGui::Text("Nodes: %zu", m_nodes.size());
    ImGui::Text("Connections: %zu", m_connections.size());
    ImGui::Text("Sequence Length: %zu", m_currentSequence.length());

    // Module type breakdown
    std::vector<int> moduleCounts(4, 0);
    for (const auto& node : m_nodes) {
        if (node.moduleType < 4) moduleCounts[node.moduleType]++;
    }
    ImGui::Text("Corridors: %d, Habitats: %d, Docking: %d, Power: %d",
        moduleCounts[0], moduleCounts[1], moduleCounts[2], moduleCounts[3]);

    ImGui::End();

    // Smaller preview/visualization window
    ImGui::SetNextWindowPos(ImVec2(830, 375), ImGuiSetCond_Once);
    ImGui::SetNextWindowSize(ImVec2(350, 350), ImGuiSetCond_Once); // smaller
    ImGui::Begin("L-System Station Layout Preview", 0);

    // Optional: show current zoom or add zoom in/out buttons
    ImGui::Text("Zoom: %.2fx", m_previewZoom);
    if (ImGui::Button("Zoom In")) setPreviewZoom(m_previewZoom * 1.1f);
    ImGui::SameLine();
    if (ImGui::Button("Zoom Out")) setPreviewZoom(m_previewZoom / 1.1f);

    drawLSystemVisualization();
    ImGui::End();
}

void Station::drawLSystemVisualization() {
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    // Smaller preview
    ImVec2 canvas_size(
        std::max(200.0f, window_size.x - 30.0f),
        std::max(200.0f, window_size.y - 40.0f)
    );

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    glm::vec2 minBounds(FLT_MAX);
    glm::vec2 maxBounds(-FLT_MAX);
    for (const auto& node : m_nodes) {
        minBounds.x = std::min(minBounds.x, node.position.x);
        minBounds.y = std::min(minBounds.y, node.position.y);
        maxBounds.x = std::max(maxBounds.x, node.position.x);
        maxBounds.y = std::max(maxBounds.y, node.position.y);
    }
    glm::vec2 padding(25.0f, 25.0f);
    minBounds -= padding;
    maxBounds += padding;
    glm::vec2 worldSize = maxBounds - minBounds;

    // Fit station to canvas, preserve aspect, then apply user zoom
    float fitScale = (worldSize.x > 0.0f && worldSize.y > 0.0f)
        ? std::min(canvas_size.x / worldSize.x, canvas_size.y / worldSize.y)
        : 1.0f;
    float scale = fitScale * m_previewZoom;

    glm::vec2 offset(
        (canvas_size.x - (worldSize.x * scale)) * 0.5f,
        (canvas_size.y - (worldSize.y * scale)) * 0.5f
    );


    // Draw white background for clarity
    draw_list->AddRectFilled(
        ImVec2(canvas_pos.x, canvas_pos.y),
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(32, 32, 32, 255)
    );

    // Draw connections
    for (const auto& connection : m_connections) {
        const auto& node1 = m_nodes[connection.first];
        const auto& node2 = m_nodes[connection.second];

        ImVec2 p1(
            canvas_pos.x + offset.x + (node1.position.x - minBounds.x) * scale,
            canvas_pos.y + offset.y + (node1.position.y - minBounds.y) * scale
        );
        ImVec2 p2(
            canvas_pos.x + offset.x + (node2.position.x - minBounds.x) * scale,
            canvas_pos.y + offset.y + (node2.position.y - minBounds.y) * scale
        );

        draw_list->AddLine(p1, p2, IM_COL32(90, 90, 90, 255), 3.0f);
    }

    // Draw nodes, use fixed size for clarity
    float nodeRadius = 8.0f;
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        const auto& node = m_nodes[i];

        ImVec2 center(
            canvas_pos.x + offset.x + (node.position.x - minBounds.x) * scale,
            canvas_pos.y + offset.y + (node.position.y - minBounds.y) * scale
        );

        // Color based on module type
        ImU32 color;
        switch (node.moduleType) {
        case 0: color = IM_COL32(255, 255, 255, 255); break; // Corridor - white
        case 1: color = IM_COL32(100, 255, 100, 255); break; // Habitat - green
        case 2: color = IM_COL32(100, 100, 255, 255); break; // Docking - blue
        case 3: color = IM_COL32(255, 255, 100, 255); break; // Power - yellow
        default: color = IM_COL32(255, 100, 100, 255); break; // Unknown - red
        }

        draw_list->AddCircleFilled(center, nodeRadius, color, 20);
        draw_list->AddCircle(center, nodeRadius, IM_COL32(0, 0, 0, 255), 20, 2.0f);

        // Draw direction indicator
        float dirLen = nodeRadius * 0.85f;
        float dirX = std::cos(node.rotation) * dirLen;
        float dirY = std::sin(node.rotation) * dirLen;
        ImVec2 dirEnd(center.x + dirX, center.y + dirY);
        draw_list->AddLine(center, dirEnd, IM_COL32(0, 0, 0, 255), 2.0f);
    }

    // Reserve space in layout for the full canvas
    ImGui::Dummy(canvas_size);
}