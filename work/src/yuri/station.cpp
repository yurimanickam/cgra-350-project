
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
        IM_COL32(200, 200, 200, 255), // Corridor - light gray
        IM_COL32(100, 255, 100, 255), // Habitat - green
        IM_COL32(100, 150, 255, 255), // Docking - blue
        IM_COL32(255, 220, 100, 255)  // Power - yellow
    };

    constexpr int NUM_MODULE_TYPES = 4;

    // Junction color
    const ImU32 JUNCTION_COLOR = IM_COL32(150, 150, 160, 255);
    const ImU32 VERTICAL_JUNCTION_COLOR = IM_COL32(255, 150, 100, 255); // Orange for vertical junctions

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
    m_moduleMesh = createCylinderMesh(m_renderParams.moduleRadius, 1.0f, 16, false);
    m_junctionMesh = createSphereMesh(m_renderParams.junctionRadius, 16, 16);
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
    m_modules.clear();
    m_junctions.clear();
    m_stateStack.clear();

    TurtleState state;
    state.position = vec2(0.0f);
    state.angle = 0.0f;
    state.length = m_params.baseLength;
    state.generation = 0;
    state.verticalOffset = 0.0f;
    state.hasVerticalChild = false;

    // Add starting junction
    addJunction(state.position, state.generation);

    for (char command : sequence) {
        switch (command) {
        case 'F': {
            const float actualLength = std::max(m_params.minLength, state.length);
            const vec2 direction(std::cos(state.angle), std::sin(state.angle));
            const vec2 newPos = state.position + direction * actualLength;

            if (!isOverlapping(newPos, m_params.minLength * 0.5f)) {
                // Create a module (the tube segment)
                const int moduleType = getRandomInt(0, NUM_MODULE_TYPES - 1);
                addModule(state.position, newPos, state.angle, actualLength, moduleType, state.generation);

                // Add junction at the end of the module
                addJunction(newPos, state.generation);

                // Possibly create vertical modules (only if we haven't already in this branch)
                if (m_params.allowVerticalModules &&
                    !state.hasVerticalChild &&
                    getRandomFloat(0.0f, 1.0f) < m_params.verticalProbability) {

                    // Randomly choose up or down
                    bool pointingUp = getRandomFloat(0.0f, 1.0f) > 0.5f;
                    const float vertLength = actualLength * 0.7f; // Slightly shorter
                    const int vertModuleType = getRandomInt(0, NUM_MODULE_TYPES - 1);

                    addVerticalModule(newPos, state.verticalOffset, pointingUp, vertLength, vertModuleType, state.generation);

                    // Mark that this branch has spawned a vertical module
                    state.hasVerticalChild = true;
                }

                // Possibly create loop connections
                if (m_params.allowLoops &&
                    getRandomFloat(0.0f, 1.0f) < m_params.connectionProbability) {
                    connectNearbyJunctions(newPos, state.generation);
                }

                state.position = newPos;
                state.length *= m_params.lengthDecay;
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
            }
            break;

        default:
            break;
        }
    }
}

void Station::addModule(const glm::vec2& startPos, const glm::vec2& endPos, float rotation, float length, int moduleType, int generation) {
    StationModule module;
    module.startPos = startPos;
    module.endPos = endPos;
    module.rotation = rotation;
    module.length = length;
    module.moduleType = moduleType;
    module.generation = generation;
    module.verticalOffset = 0.0f;
    module.isVertical = false;
    m_modules.push_back(module);
}

void Station::addVerticalModule(const glm::vec2& basePos, float baseVerticalOffset, bool pointingUp, float length, int moduleType, int generation) {
    StationModule module;
    module.startPos = basePos;
    module.endPos = basePos; // Same horizontal position
    module.rotation = 0.0f;
    module.length = length;
    module.moduleType = moduleType;
    module.generation = generation;
    module.verticalOffset = baseVerticalOffset;
    module.isVertical = true;

    // Add vertical junction at the end of the vertical module
    float endVerticalOffset = pointingUp ? (baseVerticalOffset + length) : (baseVerticalOffset - length);
    addVerticalJunction(basePos, endVerticalOffset, generation);

    m_modules.push_back(module);
}

void Station::addJunction(const glm::vec2& position, int generation) {
    // Check if junction already exists at this position (with same vertical offset)
    for (const auto& junction : m_junctions) {
        if (length(junction.position - position) < 0.1f && std::abs(junction.verticalOffset) < 0.1f) {
            return; // Junction already exists here
        }
    }

    ModuleJunction junction;
    junction.position = position;
    junction.generation = generation;
    junction.verticalOffset = 0.0f;
    m_junctions.push_back(junction);
}

void Station::addVerticalJunction(const glm::vec2& position, float verticalOffset, int generation) {
    // Check if junction already exists at this position and vertical offset
    for (const auto& junction : m_junctions) {
        if (length(junction.position - position) < 0.1f && std::abs(junction.verticalOffset - verticalOffset) < 0.1f) {
            return; // Junction already exists here
        }
    }

    ModuleJunction junction;
    junction.position = position;
    junction.generation = generation;
    junction.verticalOffset = verticalOffset;
    m_junctions.push_back(junction);
}

void Station::connectNearbyJunctions(const glm::vec2& newJunctionPos, int generation) {
    const int nearJunction = findNearestJunction(newJunctionPos, m_params.baseLength * 2.0f);
    if (nearJunction >= 0) {
        const glm::vec2& targetPos = m_junctions[nearJunction].position;
        if (length(targetPos - newJunctionPos) > 0.5f) { // Avoid self-connections
            // Create a connecting module
            const vec2 dir = targetPos - newJunctionPos;
            const float len = length(dir);
            const float angle = std::atan2(dir.y, dir.x);
            const int moduleType = 0; // Corridor for loop connections
            addModule(newJunctionPos, targetPos, angle, len, moduleType, generation);
        }
    }
}

bool Station::isOverlapping(const vec2& pos, float minDist) const {
    for (const auto& junction : m_junctions) {
        if (length(junction.position - pos) < minDist) {
            return true;
        }
    }
    return false;
}

int Station::findNearestJunction(const glm::vec2& position, float maxDistance) const {
    int nearest = -1;
    float minDist = maxDistance;

    for (size_t i = 0; i < m_junctions.size(); ++i) {
        // Only consider junctions at the same vertical level for horizontal connections
        if (std::abs(m_junctions[i].verticalOffset) > 0.1f) {
            continue;
        }

        const float dist = length(m_junctions[i].position - position);
        if (dist < minDist && dist > 0.1f) { // Avoid finding the same junction
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
    case 0: return glm::vec3(0.8f, 0.8f, 0.8f); // Corridor - light gray
    case 1: return glm::vec3(0.4f, 0.9f, 0.4f); // Habitat - green
    case 2: return glm::vec3(0.4f, 0.6f, 0.9f); // Docking - blue
    case 3: return glm::vec3(0.9f, 0.86f, 0.4f); // Power - yellow
    default: return glm::vec3(0.7f, 0.7f, 0.7f);
    }
}

glm::mat4 Station::calculateModuleTransform(const StationModule& module, float gapSize) const {
    if (module.isVertical) {
        // Vertical module transform
        const vec3 basePos3D(module.startPos.x, module.verticalOffset, module.startPos.y);

        // Determine direction (up or down) based on stored data
        // We need to check if there's an end junction above or below
        float endY = module.verticalOffset;
        for (const auto& junction : m_junctions) {
            if (length(junction.position - module.startPos) < 0.1f &&
                std::abs(junction.verticalOffset - module.verticalOffset) > 0.1f) {
                endY = junction.verticalOffset;
                break;
            }
        }

        const bool pointingUp = endY > module.verticalOffset;
        const float actualLength = std::max(0.1f, std::abs(endY - module.verticalOffset) - 2.0f * gapSize);

        const vec3 centerPos = basePos3D + vec3(0.0f, (pointingUp ? 1.0f : -1.0f) * (gapSize + actualLength * 0.5f), 0.0f);

        mat4 transform = translate(mat4(1.0f), centerPos);

        // Rotate cylinder to point vertically
        // Default cylinder is along X-axis, rotate to Y-axis
        if (pointingUp) {
            transform = rotate(transform, HALF_PI, vec3(0.0f, 0.0f, 1.0f));
        }
        else {
            transform = rotate(transform, -HALF_PI, vec3(0.0f, 0.0f, 1.0f));
        }

        transform = scale(transform, vec3(actualLength, 1.0f, 1.0f));

        return transform;
    }
    else {
        // Horizontal module transform (original code)
        const vec3 fromPos3D(module.startPos.x, 0.0f, module.startPos.y);
        const vec3 toPos3D(module.endPos.x, 0.0f, module.endPos.y);

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
}

void Station::render3DStation(const glm::mat4& view, const glm::mat4& proj, GLuint shader) {
    if (!m_drawStation || m_modules.empty()) {
        return;
    }

    if (m_meshNeedsRebuild) {
        rebuildMeshes();
    }

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, GL_FALSE, value_ptr(proj));

    const float gapSize = m_renderParams.junctionRadius * m_renderParams.gapMultiplier;

    // Render all modules (the tubes - these are the functional parts)
    for (const auto& module : m_modules) {
        const mat4 modelTransform = calculateModuleTransform(module, gapSize);
        const mat4 modelView = view * modelTransform;

        glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, GL_FALSE, value_ptr(modelView));

        vec3 color = getModuleColor(module.moduleType);

        // Make vertical modules slightly brighter to distinguish them
        if (module.isVertical) {
            color = glm::min(color * 1.2f, vec3(1.0f));
        }

        glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));

        m_moduleMesh.draw();
    }

    // Render all junctions (the connection points - spheres)
    for (const auto& junction : m_junctions) {
        const vec3 pos3D(junction.position.x, junction.verticalOffset, junction.position.y);
        const mat4 modelTransform = translate(mat4(1.0f), pos3D);
        const mat4 modelView = view * modelTransform;

        glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, GL_FALSE, value_ptr(modelView));

        // Use different color for vertical junctions
        const vec3 junctionColor = (std::abs(junction.verticalOffset) > 0.1f)
            ? vec3(0.9f, 0.6f, 0.4f)  // Orange for vertical junctions
            : vec3(0.6f, 0.6f, 0.65f); // Gray for horizontal junctions

        glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(junctionColor));

        m_junctionMesh.draw();
    }
}

void Station::renderGUI() {
    applyUIStyle();

    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiSetCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(900, 720), ImGuiSetCond_FirstUseEver);

    ImGui::Begin("Space Station Generator", nullptr);

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
            ImGui::SetTooltip("Number of L-System iterations (higher = more complex station)");
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
            ImGui::SetTooltip("Enable additional connecting modules between nearby junctions to create loops");
        }

        if (m_params.allowLoops) {
            ImGui::Spacing();
            ImGui::Text("Loop Probability");
            ImGui::PushItemWidth(-1);
            needsRegeneration |= ImGui::SliderFloat("##ConnProb", &m_params.connectionProbability, 0.0f, 0.5f, "%.2f");
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Chance of creating an additional connecting module");
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // NEW: Vertical modules section
        needsRegeneration |= ImGui::Checkbox("Allow Vertical Modules", &m_params.allowVerticalModules);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enable modules that extend upward or downward (one level deep)");
        }

        if (m_params.allowVerticalModules) {
            ImGui::Spacing();
            ImGui::Text("Vertical Probability");
            ImGui::PushItemWidth(-1);
            needsRegeneration |= ImGui::SliderFloat("##VertProb", &m_params.verticalProbability, 0.0f, 0.5f, "%.2f");
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Chance of creating a vertical module (limited to one per branch)");
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
        if (ImGui::SliderFloat("##JunctionRadius", &m_renderParams.junctionRadius, 0.5f, 10.0f, "Junctions: %.1f")) {
            m_meshNeedsRebuild = true;
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Size of spherical junction connectors");
        }

        ImGui::PushItemWidth(-1);
        if (ImGui::SliderFloat("##ModuleRadius", &m_renderParams.moduleRadius, 0.2f, 5.0f, "Modules: %.1f")) {
            m_meshNeedsRebuild = true;
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Thickness/radius of module tubes");
        }

        ImGui::PushItemWidth(-1);
        ImGui::SliderFloat("##GapMult", &m_renderParams.gapMultiplier, 0.0f, 2.5f, "Gap: %.2fx");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Space between modules and junctions");
        }

        ImGui::Spacing();

        if (ImGui::Button("Reset 3D Settings", ImVec2(-1, 0))) {
            m_renderParams.junctionRadius = 1.0f;
            m_renderParams.moduleRadius = 1.5f;
            m_renderParams.gapMultiplier = 0.0f;
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

        ImGui::Text("Total Modules:");
        ImGui::NextColumn();
        ImGui::Text("%zu", m_modules.size());
        ImGui::NextColumn();

        // Count vertical modules
        int verticalCount = 0;
        for (const auto& module : m_modules) {
            if (module.isVertical) verticalCount++;
        }

        ImGui::Text("Vertical Modules:");
        ImGui::NextColumn();
        ImGui::Text("%d", verticalCount);
        ImGui::NextColumn();

        ImGui::Text("Junctions:");
        ImGui::NextColumn();
        ImGui::Text("%zu", m_junctions.size());
        ImGui::NextColumn();

        ImGui::Text("Sequence Length:");
        ImGui::NextColumn();
        ImGui::Text("%zu", m_currentSequence.length());
        ImGui::NextColumn();

        ImGui::Columns(1);

        ImGui::Spacing();
        ImGui::Text("Module Type Breakdown");
        ImGui::Separator();

        std::vector<int> moduleCounts(NUM_MODULE_TYPES, 0);
        for (const auto& module : m_modules) {
            if (module.moduleType < NUM_MODULE_TYPES) {
                moduleCounts[module.moduleType]++;
            }
        }

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
    ImGui::Text("Legend:");
    ImGui::Columns(2, "legend", false);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const float legendLineLength = 20.0f;
    const float legendLineThickness = 4.0f;
    const float legendCircleSize = 6.0f;

    const char* moduleNames[] = { "Corridor", "Habitat", "Docking Bay", "Power" };
    for (int i = 0; i < NUM_MODULE_TYPES; ++i) {
        ImVec2 cursor = ImGui::GetCursorScreenPos();

        // Draw module as a line segment
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

    // Add junction to legend
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

    // Add vertical junction to legend
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

    // Main preview canvas
    drawVisualization();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextWrapped("Tip: Use the zoom controls to explore the station layout. "
        "Colored lines represent functional modules (tubes), gray circles show horizontal junctions, "
        "and orange circles show vertical connection points.");
}

void Station::calculateBounds(vec2& minBounds, vec2& maxBounds) const {
    minBounds = vec2(FLT_MAX);
    maxBounds = vec2(-FLT_MAX);

    for (const auto& junction : m_junctions) {
        minBounds = glm::min(minBounds, junction.position);
        maxBounds = glm::max(maxBounds, junction.position);
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

    // Start clipping
    draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), true);

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

    // Draw modules (these are the main tubes - the functional parts)
    const float moduleLineThickness = 6.0f * glm::clamp(m_previewZoom, 0.5f, 2.0f);

    for (const auto& module : m_modules) {
        ImU32 color = (module.moduleType >= 0 && module.moduleType < NUM_MODULE_TYPES)
            ? MODULE_COLORS[module.moduleType]
            : IM_COL32(200, 100, 100, 255);

        if (module.isVertical) {
            // Draw vertical modules as a vertical line with a special marker
            const ImVec2 p = worldToScreen(module.startPos);

            // Draw a vertical indicator (upward/downward arrow-like symbol)
            const float arrowSize = 8.0f * glm::clamp(m_previewZoom, 0.5f, 2.0f);

            // Shadow
            draw_list->AddLine(
                ImVec2(p.x + 1, p.y - arrowSize + 1),
                ImVec2(p.x + 1, p.y + arrowSize + 1),
                IM_COL32(0, 0, 0, 80),
                moduleLineThickness
            );

            // Vertical line
            draw_list->AddLine(
                ImVec2(p.x, p.y - arrowSize),
                ImVec2(p.x, p.y + arrowSize),
                color,
                moduleLineThickness
            );

            // Draw small horizontal bar to indicate vertical module
            draw_list->AddLine(
                ImVec2(p.x - arrowSize * 0.5f, p.y),
                ImVec2(p.x + arrowSize * 0.5f, p.y),
                color,
                moduleLineThickness * 0.7f
            );
        }
        else {
            // Horizontal modules (original code)
            const ImVec2 p1 = worldToScreen(module.startPos);
            const ImVec2 p2 = worldToScreen(module.endPos);

            // Draw shadow
            draw_list->AddLine(
                ImVec2(p1.x + 1, p1.y + 1),
                ImVec2(p2.x + 1, p2.y + 1),
                IM_COL32(0, 0, 0, 80),
                moduleLineThickness
            );

            // Draw module
            draw_list->AddLine(p1, p2, color, moduleLineThickness);
        }
    }

    // Draw junctions (connection points - small circles)
    const float junctionRadius = 4.0f * glm::clamp(m_previewZoom, 0.5f, 2.0f);

    for (const auto& junction : m_junctions) {
        const ImVec2 center = worldToScreen(junction.position);

        // Choose color based on whether it's a vertical junction
        const ImU32 jColor = (std::abs(junction.verticalOffset) > 0.1f)
            ? VERTICAL_JUNCTION_COLOR
            : JUNCTION_COLOR;

        // Draw shadow
        draw_list->AddCircleFilled(
            ImVec2(center.x + 1, center.y + 1),
            junctionRadius,
            IM_COL32(0, 0, 0, 80),
            12
        );

        // Draw junction
        draw_list->AddCircleFilled(center, junctionRadius, jColor, 12);

        const ImU32 borderColor = (std::abs(junction.verticalOffset) > 0.1f)
            ? IM_COL32(180, 100, 50, 255)
            : IM_COL32(80, 80, 90, 255);

        draw_list->AddCircle(center, junctionRadius, borderColor, 12, 1.5f);
    }

    // End clipping
    draw_list->PopClipRect();

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
