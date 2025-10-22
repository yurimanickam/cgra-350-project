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

// make new station
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

// Render multi-material model with PBR
void Station::renderMultiMaterialModel(const glm::mat4& view, const glm::mat4& proj, GLuint pbrShader, const glm::vec3& camPos) {
    if (!m_showModelButton || !m_modelLoaded || !m_multiModel) {
        return;
    }

    glUseProgram(pbrShader);
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "projection"), 1, GL_FALSE, value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "view"), 1, GL_FALSE, value_ptr(view));
    glUniform3fv(glGetUniformLocation(pbrShader, "camPos"), 1, value_ptr(camPos));

    mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(pbrShader, "model"), 1, GL_FALSE, value_ptr(model));
    glUniformMatrix3fv(glGetUniformLocation(pbrShader, "normalMatrix"), 1, GL_FALSE,
        value_ptr(glm::transpose(glm::inverse(glm::mat3(model)))));

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

// make sphere mesh
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

// rebuild mesh shapes
void Station::rebuildMeshes() {
    m_moduleMesh = createCylinderMesh(m_renderParams.moduleRadius, 1.0f, 16, false);
    m_junctionMesh = createSphereMesh(m_renderParams.junctionRadius, 16, 16);
    m_meshNeedsRebuild = false;
}

// make cylinder mesh
cgra::gl_mesh Station::createCylinderMesh(float radius, float length, int subdivisions, bool capped) {
    using namespace cgra;
    mesh_builder builder(GL_TRIANGLES);
    float halfLength = length / 2.0f;
    float deltaTheta = TWO_PI / float(subdivisions);

    for (int i = 0; i <= subdivisions; ++i) {
        float theta = i * deltaTheta;
        float y = radius * std::cos(theta);
        float z = radius * std::sin(theta);
        vec3 normal = normalize(vec3(0, y, z));
        float u = float(i) / subdivisions;

        builder.vertices.push_back({ vec3(-halfLength, y, z), normal, vec2(u, 0) });
        builder.vertices.push_back({ vec3(+halfLength, y, z), normal, vec2(u, 1) });
    }

    for (int i = 0; i < subdivisions; ++i) {
        int idx = i * 2;
        builder.indices.push_back(idx);
        builder.indices.push_back(idx + 1);
        builder.indices.push_back(idx + 2);

        builder.indices.push_back(idx + 1);
        builder.indices.push_back(idx + 3);
        builder.indices.push_back(idx + 2);
    }

    if (capped) {
        auto addCap = [&](float x, vec3 normal, bool reverseWinding) {
            int centerIdx = builder.vertices.size();
            builder.vertices.push_back({ vec3(x, 0, 0), normal, vec2(0.5f, 0.5f) });

            for (int i = 0; i <= subdivisions; ++i) {
                float theta = i * deltaTheta;
                float y = radius * std::cos(theta);
                float z = radius * std::sin(theta);
                vec2 uv(0.5f + 0.5f * std::cos(theta), 0.5f + 0.5f * std::sin(theta));
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

// set up lsystem
void Station::initializeLSystem() {
    m_rng.seed(m_params.seed);
    setupRules();
    regenerate();
}

// set lsystem rules
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

// make new sequence
void Station::regenerate() {
    m_rng.seed(m_params.seed);
    generateSequence();
    interpretSequence(m_currentSequence);
}

// generate sequence
void Station::generateSequence() {
    m_currentSequence = "X";
    for (int i = 0; i < m_params.iterations; ++i) {
        m_currentSequence = applyRules(m_currentSequence);
    }
}

// apply rules to sequence
std::string Station::applyRules(const std::string& current) {
    std::string result;
    result.reserve(current.size() * 2);

    for (char c : current) {
        bool ruleApplied = false;

        for (const auto& rule : m_rules) {
            if (rule.symbol == c) {
                if (getRandomFloat(0.0f, 1.0f) < rule.probability && !rule.productions.empty()) {
                    int idx = getRandomInt(0, rule.productions.size() - 1);
                    result += rule.productions[idx];
                }
                else {
                    result += c;
                }
                ruleApplied = true;
                break;
            }
        }
        if (!ruleApplied) result += c;
    }

    return result;
}

// interpret sequence
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

    addJunction(state.position, state.generation);

    for (char command : sequence) {
        switch (command) {
        case 'F': {
            float actualLength = std::max(m_params.minLength, state.length);
            vec2 direction(std::cos(state.angle), std::sin(state.angle));
            vec2 newPos = state.position + direction * actualLength;

            if (!isOverlapping(newPos, m_params.minLength * 0.5f)) {
                int moduleType = getRandomInt(0, NUM_MODULE_TYPES - 1);
                addModule(state.position, newPos, state.angle, actualLength, moduleType, state.generation);
                addJunction(newPos, state.generation);

                if (m_params.allowVerticalModules
                    && !state.hasVerticalChild
                    && getRandomFloat(0.0f, 1.0f) < m_params.verticalProbability) {

                    bool pointingUp = getRandomFloat(0.0f, 1.0f) > 0.5f;
                    float vertLength = actualLength * 0.7f;
                    int vertModuleType = getRandomInt(0, NUM_MODULE_TYPES - 1);
                    addVerticalModule(newPos, state.verticalOffset, pointingUp, vertLength, vertModuleType, state.generation);
                    state.hasVerticalChild = true;
                }

                if (m_params.allowLoops
                    && getRandomFloat(0.0f, 1.0f) < m_params.connectionProbability) {
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

// add module
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

// add vertical module
void Station::addVerticalModule(const glm::vec2& basePos, float baseVerticalOffset, bool pointingUp, float length, int moduleType, int generation) {
    StationModule module;
    module.startPos = basePos;
    module.endPos = basePos;
    module.rotation = 0.0f;
    module.length = length;
    module.moduleType = moduleType;
    module.generation = generation;
    module.verticalOffset = baseVerticalOffset;
    module.isVertical = true;

    float endVerticalOffset = pointingUp ? (baseVerticalOffset + length) : (baseVerticalOffset - length);
    addVerticalJunction(basePos, endVerticalOffset, generation);

    m_modules.push_back(module);
}

// add junction
void Station::addJunction(const glm::vec2& position, int generation) {
    for (const auto& junction : m_junctions) {
        if (length(junction.position - position) < 0.1f && std::abs(junction.verticalOffset) < 0.1f)
            return;
    }
    ModuleJunction junction;
    junction.position = position;
    junction.generation = generation;
    junction.verticalOffset = 0.0f;
    m_junctions.push_back(junction);
}

// add vertical junction
void Station::addVerticalJunction(const glm::vec2& position, float verticalOffset, int generation) {
    for (const auto& junction : m_junctions) {
        if (length(junction.position - position) < 0.1f && std::abs(junction.verticalOffset - verticalOffset) < 0.1f)
            return;
    }
    ModuleJunction junction;
    junction.position = position;
    junction.generation = generation;
    junction.verticalOffset = verticalOffset;
    m_junctions.push_back(junction);
}

// connect nearby junctions
void Station::connectNearbyJunctions(const glm::vec2& newJunctionPos, int generation) {
    int nearJunction = findNearestJunction(newJunctionPos, m_params.baseLength * 2.0f);
    if (nearJunction >= 0) {
        const glm::vec2& targetPos = m_junctions[nearJunction].position;
        if (length(targetPos - newJunctionPos) > 0.5f) {
            vec2 dir = targetPos - newJunctionPos;
            float len = length(dir);
            float angle = std::atan2(dir.y, dir.x);
            int moduleType = 0;
            addModule(newJunctionPos, targetPos, angle, len, moduleType, generation);
        }
    }
}

// check overlap
bool Station::isOverlapping(const vec2& pos, float minDist) const {
    for (const auto& junction : m_junctions) {
        if (length(junction.position - pos) < minDist)
            return true;
    }
    return false;
}

// find nearest junction
int Station::findNearestJunction(const glm::vec2& position, float maxDistance) const {
    int nearest = -1;
    float minDist = maxDistance;
    for (size_t i = 0; i < m_junctions.size(); ++i) {
        if (std::abs(m_junctions[i].verticalOffset) > 0.1f)
            continue;
        float dist = length(m_junctions[i].position - position);
        if (dist < minDist && dist > 0.1f) {
            minDist = dist;
            nearest = static_cast<int>(i);
        }
    }
    return nearest;
}

// random float
float Station::getRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(m_rng);
}

// random int
int Station::getRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(m_rng);
}

// get module color
glm::vec3 Station::getModuleColor(int moduleType) const {
    switch (moduleType) {
    case 0: return glm::vec3(0.8f, 0.8f, 0.8f);
    case 1: return glm::vec3(0.4f, 0.9f, 0.4f);
    case 2: return glm::vec3(0.4f, 0.6f, 0.9f);
    case 3: return glm::vec3(0.9f, 0.86f, 0.4f);
    default: return glm::vec3(0.7f, 0.7f, 0.7f);
    }
}

// get module transform
glm::mat4 Station::calculateModuleTransform(const StationModule& module, float gapSize) const {
    if (module.isVertical) {
        vec3 basePos3D(module.startPos.x, module.verticalOffset, module.startPos.y);
        float endY = module.verticalOffset;
        for (const auto& junction : m_junctions) {
            if (length(junction.position - module.startPos) < 0.1f &&
                std::abs(junction.verticalOffset - module.verticalOffset) > 0.1f) {
                endY = junction.verticalOffset;
                break;
            }
        }
        bool pointingUp = endY > module.verticalOffset;
        float actualLength = std::max(0.1f, std::abs(endY - module.verticalOffset) - 2.0f * gapSize);
        vec3 centerPos = basePos3D + vec3(0.0f, (pointingUp ? 1.0f : -1.0f) * (gapSize + actualLength * 0.5f), 0.0f);
        mat4 transform = translate(mat4(1.0f), centerPos);
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
        vec3 fromPos3D(module.startPos.x, 0.0f, module.startPos.y);
        vec3 toPos3D(module.endPos.x, 0.0f, module.endPos.y);
        vec3 direction = toPos3D - fromPos3D;
        float fullDistance = length(direction);
        float actualLength = std::max(0.1f, fullDistance - 2.0f * gapSize);
        vec3 normalizedDir = normalize(direction);
        vec3 centerPos = fromPos3D + normalizedDir * (gapSize + actualLength * 0.5f);
        vec3 defaultDir(1.0f, 0.0f, 0.0f);
        mat4 transform = translate(mat4(1.0f), centerPos);
        vec3 rotAxis = cross(defaultDir, normalizedDir);
        float rotAxisLen = length(rotAxis);

        if (rotAxisLen > 0.001f) {
            float angle = std::acos(glm::clamp(dot(defaultDir, normalizedDir), -1.0f, 1.0f));
            transform = rotate(transform, angle, normalize(rotAxis));
        }
        else if (dot(defaultDir, normalizedDir) < 0.0f) {
            transform = rotate(transform, PI, vec3(0.0f, 1.0f, 0.0f));
        }
        transform = scale(transform, vec3(actualLength, 1.0f, 1.0f));
        return transform;
    }
}

// render station 3d view
void Station::render3DStation(const glm::mat4& view, const glm::mat4& proj, GLuint shader) {
    if (!m_drawStation || m_modules.empty()) return;
    if (m_meshNeedsRebuild) rebuildMeshes();

    glUseProgram(shader);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjectionMatrix"), 1, GL_FALSE, value_ptr(proj));
    float gapSize = m_renderParams.junctionRadius * m_renderParams.gapMultiplier;

    for (const auto& module : m_modules) {
        mat4 modelTransform = calculateModuleTransform(module, gapSize);
        mat4 modelView = view * modelTransform;
        glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, GL_FALSE, value_ptr(modelView));

        vec3 color = getModuleColor(module.moduleType);
        if (module.isVertical)
            color = glm::min(color * 1.2f, vec3(1.0f));
        glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(color));
        m_moduleMesh.draw();
    }

    for (const auto& junction : m_junctions) {
        vec3 pos3D(junction.position.x, junction.verticalOffset, junction.position.y);
        mat4 modelTransform = translate(mat4(1.0f), pos3D);
        mat4 modelView = view * modelTransform;

        glUniformMatrix4fv(glGetUniformLocation(shader, "uModelViewMatrix"), 1, GL_FALSE, value_ptr(modelView));
        vec3 junctionColor = (std::abs(junction.verticalOffset) > 0.1f) ? vec3(0.9f, 0.6f, 0.4f) : vec3(0.6f, 0.6f, 0.65f);
        glUniform3fv(glGetUniformLocation(shader, "uColor"), 1, value_ptr(junctionColor));

        m_junctionMesh.draw();
    }
}

// apply ui style
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

// render controls panel
void Station::renderControlsPanel() {
    bool needsRegeneration = false;

    ImGui::PushStyleColor(ImGuiCol_Header, COLOR_HEADER);
    ImGui::Spacing();
    ImGui::Text("L-SYSTEM SPACE STATION GENERATOR");
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // generation params
    if (ImGui::CollapsingHeader("Generation Parameters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        ImGui::Text("Complexity");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderInt("##Iterations", &m_params.iterations, 1, 6);
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Number of L-System iterations (higher = more complex station)");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Text("Module Sizing");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderFloat("##BaseLength", &m_params.baseLength, 5.0f, 30.0f, "Base: %.1f");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Initial module length");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderFloat("##LengthDecay", &m_params.lengthDecay, 0.5f, 1.0f, "Decay: %.2f");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("How much modules shrink each generation");
        ImGui::PushItemWidth(-1);
        needsRegeneration |= ImGui::SliderFloat("##MinLength", &m_params.minLength, 1.0f, 10.0f, "Minimum: %.1f");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Smallest allowed module size");
        ImGui::Spacing();
    }

    // topology
    if (ImGui::CollapsingHeader("Topology & Connections", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        ImGui::Checkbox("Allow Loop Connections", &m_params.allowLoops);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable additional connecting modules between nearby junctions to create loops");
        if (m_params.allowLoops) {
            ImGui::Spacing();
            ImGui::Text("Loop Probability");
            ImGui::PushItemWidth(-1);
            needsRegeneration |= ImGui::SliderFloat("##ConnProb", &m_params.connectionProbability, 0.0f, 0.5f, "%.2f");
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chance of creating an additional connecting module");
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        needsRegeneration |= ImGui::Checkbox("Allow Vertical Modules", &m_params.allowVerticalModules);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enable modules that extend upward or downward (one level deep)");
        if (m_params.allowVerticalModules) {
            ImGui::Spacing();
            ImGui::Text("Vertical Probability");
            ImGui::PushItemWidth(-1);
            needsRegeneration |= ImGui::SliderFloat("##VertProb", &m_params.verticalProbability, 0.0f, 0.5f, "%.2f");
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Chance of creating a vertical module (limited to one per branch)");
        }
        ImGui::Spacing();
    }

    // 3d rendering
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
        if (ImGui::SliderFloat("##ModuleRadius", &m_renderParams.moduleRadius, 0.2f, 5.0f, "Modules: %.1f")) {
            m_meshNeedsRebuild = true;
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Thickness/radius of module tubes");
        ImGui::PushItemWidth(-1);
        ImGui::SliderFloat("##GapMult", &m_renderParams.gapMultiplier, 0.0f, 2.5f, "Gap: %.2fx");
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Space between modules and junctions");
        ImGui::Spacing();
        if (ImGui::Button("Reset 3D Settings", ImVec2(-1, 0))) {
            m_renderParams.junctionRadius = 1.0f;
            m_renderParams.moduleRadius = 1.5f;
            m_renderParams.gapMultiplier = 0.0f;
            m_meshNeedsRebuild = true;
        }
        ImGui::Spacing();
    }

    // materials
    if (ImGui::CollapsingHeader("Model Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        ImGui::TextWrapped("Toggle the multi-material PBR model display.");
        ImGui::Spacing();
        ImGui::Checkbox("Show Multi-Material Model", &m_showModelButton);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show/hide the PBR model with cyclical materials (gold/plastic/cloth)");
        ImGui::Spacing();

        if (m_modelLoaded) {
            ImGui::Text("Materials assigned in pattern: 0,1,2,0,1,2...");
            ImGui::BulletText("0 = Gold");
            ImGui::BulletText("1 = Plastic");
            ImGui::BulletText("2 = Cloth");
        }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Model not loaded");
        }

        ImGui::Spacing();
    }

    // random seed
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

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // stats
    if (ImGui::CollapsingHeader("Statistics", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        ImGui::Columns(2, "stats", false);
        ImGui::Text("Total Modules:");
        ImGui::NextColumn();
        ImGui::Text("%zu", m_modules.size());
        ImGui::NextColumn();

        int verticalCount = 0;
        for (const auto& module : m_modules)
            if (module.isVertical) verticalCount++;

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
        for (const auto& module : m_modules)
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

// render gui
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

// render preview panel
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
        "Colored lines represent functional modules (tubes), gray circles show horizontal junctions, "
        "and orange circles show vertical connection points.");
}

// calc bounds
void Station::calculateBounds(vec2& minBounds, vec2& maxBounds) const {
    minBounds = vec2(FLT_MAX);
    maxBounds = vec2(-FLT_MAX);
    for (const auto& junction : m_junctions) {
        minBounds = glm::min(minBounds, junction.position);
        maxBounds = glm::max(maxBounds, junction.position);
    }
    vec2 padding(25.0f);
    minBounds -= padding;
    maxBounds += padding;
}

// draw preview
void Station::drawVisualization() {
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

    for (const auto& module : m_modules) {
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

    for (const auto& junction : m_junctions) {
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