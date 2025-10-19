#include "station.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
#include <random>
#include <sstream>
#include <cgra/cgra_mesh.hpp>
#include <cgra/cgra_gui.hpp>
#include <glm/gtc/constants.hpp>

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

void Station::generateLSystem() {
    resetLSystem();
    initializeRoots();
    growBranches();
    findConnections();
    updateLSystemString();
}

void Station::resetLSystem() {
    m_modules.clear();
    m_connections.clear();
    m_occupancyGrid.assign(m_gridSize, std::vector<int>(m_gridSize, -1));
    m_lSystemString.clear();
    m_nextModuleId = 0;
    m_rng.seed(std::random_device{}());
}

void Station::initializeRoots() {
    // Define the 8 possible faces of a 2x2x2 cube in 2D (6 faces projected to 2D)
    std::vector<glm::vec2> possibleFaces = {
        glm::vec2(1, 0),   // Right
        glm::vec2(-1, 0),  // Left
        glm::vec2(0, 1),   // Up
        glm::vec2(0, -1),  // Down
        glm::vec2(1, 1),   // Up-Right diagonal
        glm::vec2(-1, 1),  // Up-Left diagonal
        glm::vec2(1, -1),  // Down-Right diagonal
        glm::vec2(-1, -1)  // Down-Left diagonal
    };

    // Shuffle and select 4 random faces
    std::shuffle(possibleFaces.begin(), possibleFaces.end(), m_rng);

    glm::vec2 center = glm::vec2(m_gridSize / 2, m_gridSize / 2);

    for (int i = 0; i < m_numRoots && i < possibleFaces.size(); ++i) {
        Module root;
        root.id = m_nextModuleId++;
        root.position = center;
        root.direction = glm::normalize(possibleFaces[i]);
        root.length = std::uniform_int_distribution<int>(m_minBranchLength, m_maxBranchLength)(m_rng);
        root.isRoot = true;
        root.parentId = -1;

        m_modules.push_back(root);
        occupyGridCells(root);
    }
}

void Station::growBranches() {
    for (int iteration = 0; iteration < m_maxIterations; ++iteration) {
        std::vector<Module> newModules;

        for (const auto& module : m_modules) {
            // Skip if this module is too recent (avoid infinite branching)
            if (iteration == 0 && module.isRoot) continue;

            // Probability check for branching
            if (std::uniform_real_distribution<float>(0.0f, 1.0f)(m_rng) > m_branchProbability) {
                continue;
            }

            // Get end position of current module
            glm::vec2 endPos = module.position + module.direction * float(module.length);
            glm::ivec2 endGrid = worldToGrid(endPos);

            // Try to create branches at 90-degree angles
            std::vector<glm::vec2> validDirections = getValidDirections();
            std::shuffle(validDirections.begin(), validDirections.end(), m_rng);

            for (const auto& newDir : validDirections) {
                // Skip if direction is same as current or opposite
                if (glm::dot(newDir, module.direction) > 0.7f || glm::dot(newDir, module.direction) < -0.7f) {
                    continue;
                }

                int newLength = std::uniform_int_distribution<int>(m_minBranchLength, m_maxBranchLength)(m_rng);

                if (isValidPosition(endGrid, newLength, newDir) &&
                    hasMinimumSpacing(endGrid, newDir, newLength)) {

                    Module newModule;
                    newModule.id = m_nextModuleId++;
                    newModule.position = endPos;
                    newModule.direction = newDir;
                    newModule.length = newLength;
                    newModule.isRoot = false;
                    newModule.parentId = module.id;

                    newModules.push_back(newModule);
                    break; // Only one branch per module per iteration
                }
            }
        }

        // Add new modules and occupy grid cells
        for (const auto& newModule : newModules) {
            m_modules.push_back(newModule);
            occupyGridCells(newModule);
        }

        if (newModules.empty()) break; // No more valid branches
    }
}

void Station::findConnections() {
    m_connections.clear();

    for (size_t i = 0; i < m_modules.size(); ++i) {
        for (size_t j = i + 1; j < m_modules.size(); ++j) {
            const auto& moduleA = m_modules[i];
            const auto& moduleB = m_modules[j];

            // Skip if they're already parent-child
            if (moduleA.parentId == moduleB.id || moduleB.parentId == moduleA.id) {
                continue;
            }

            // Check if modules are close enough to connect
            glm::vec2 endA = moduleA.position + moduleA.direction * float(moduleA.length);
            glm::vec2 endB = moduleB.position + moduleB.direction * float(moduleB.length);

            float distStartToStart = glm::distance(moduleA.position, moduleB.position);
            float distEndToEnd = glm::distance(endA, endB);
            float distStartToEnd = glm::distance(moduleA.position, endB);
            float distEndToStart = glm::distance(endA, moduleB.position);

            float minDist = std::min({ distStartToStart, distEndToEnd, distStartToEnd, distEndToStart });

            if (minDist <= 2.0f && std::uniform_real_distribution<float>(0.0f, 1.0f)(m_rng) < m_connectionProbability) {
                Connection conn;
                conn.moduleA = moduleA.id;
                conn.moduleB = moduleB.id;

                // Use the closest points as connection point
                if (minDist == distStartToStart) {
                    conn.connectionPoint = (moduleA.position + moduleB.position) * 0.5f;
                }
                else if (minDist == distEndToEnd) {
                    conn.connectionPoint = (endA + endB) * 0.5f;
                }
                else if (minDist == distStartToEnd) {
                    conn.connectionPoint = (moduleA.position + endB) * 0.5f;
                }
                else {
                    conn.connectionPoint = (endA + moduleB.position) * 0.5f;
                }

                m_connections.push_back(conn);
            }
        }
    }
}

void Station::updateLSystemString() {
    std::ostringstream oss;
    oss << "Space Station L-System Generated:\n";
    oss << "Modules: " << m_modules.size() << "\n";
    oss << "Connections: " << m_connections.size() << "\n\n";

    for (const auto& module : m_modules) {
        oss << "Module " << module.id << ": ";
        if (module.isRoot) oss << "ROOT ";
        oss << "Pos(" << module.position.x << "," << module.position.y << ") ";
        oss << "Dir(" << module.direction.x << "," << module.direction.y << ") ";
        oss << "Len:" << module.length;
        if (module.parentId >= 0) oss << " Parent:" << module.parentId;
        oss << "\n";
    }

    oss << "\nConnections:\n";
    for (const auto& conn : m_connections) {
        oss << "Connect " << conn.moduleA << " <-> " << conn.moduleB << "\n";
    }

    m_lSystemString = oss.str();
}

glm::vec2 Station::gridToWorld(int x, int y) const {
    return glm::vec2(x, y);
}

glm::ivec2 Station::worldToGrid(glm::vec2 pos) const {
    return glm::ivec2(std::round(pos.x), std::round(pos.y));
}

bool Station::isValidPosition(glm::ivec2 gridPos, int length, glm::vec2 direction) const {
    for (int i = 0; i <= length; ++i) {
        glm::ivec2 checkPos = gridPos + glm::ivec2(direction * float(i));
        if (checkPos.x < 0 || checkPos.x >= m_gridSize ||
            checkPos.y < 0 || checkPos.y >= m_gridSize) {
            return false;
        }
    }
    return true;
}

bool Station::hasMinimumSpacing(glm::ivec2 gridPos, glm::vec2 direction, int length) const {
    // Check perpendicular spacing
    glm::vec2 perpDir = glm::vec2(-direction.y, direction.x);

    for (int i = 0; i <= length; ++i) {
        glm::ivec2 centerPos = gridPos + glm::ivec2(direction * float(i));

        for (int offset = -m_minSpacing; offset <= m_minSpacing; ++offset) {
            if (offset == 0) continue; // Center is allowed to be occupied by current module

            glm::ivec2 checkPos = centerPos + glm::ivec2(perpDir * float(offset));
            if (checkPos.x >= 0 && checkPos.x < m_gridSize &&
                checkPos.y >= 0 && checkPos.y < m_gridSize) {
                if (m_occupancyGrid[checkPos.x][checkPos.y] != -1) {
                    return false;
                }
            }
        }
    }
    return true;
}

void Station::occupyGridCells(const Module& module) {
    for (int i = 0; i <= module.length; ++i) {
        glm::vec2 pos = module.position + module.direction * float(i);
        glm::ivec2 gridPos = worldToGrid(pos);
        if (gridPos.x >= 0 && gridPos.x < m_gridSize &&
            gridPos.y >= 0 && gridPos.y < m_gridSize) {
            m_occupancyGrid[gridPos.x][gridPos.y] = module.id;
        }
    }
}

std::vector<glm::vec2> Station::getValidDirections() const {
    return {
        glm::vec2(1, 0),   // Right
        glm::vec2(-1, 0),  // Left
        glm::vec2(0, 1),   // Up
        glm::vec2(0, -1)   // Down
    };
}

void Station::renderLSystemGUI() {
    ImGui::SetNextWindowPos(ImVec2(820, 5), ImGuiSetCond_Once);
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiSetCond_Once);
    ImGui::Begin("Space Station L-System Generator", 0);

    ImGui::Text("L-System Parameters");
    ImGui::SliderInt("Number of Roots", &m_numRoots, 2, 6);
    ImGui::SliderInt("Min Branch Length", &m_minBranchLength, 1, 5);
    ImGui::SliderInt("Max Branch Length", &m_maxBranchLength, 2, 8);
    ImGui::SliderInt("Min Spacing", &m_minSpacing, 1, 4);
    ImGui::SliderInt("Max Iterations", &m_maxIterations, 1, 10);
    ImGui::SliderFloat("Branch Probability", &m_branchProbability, 0.1f, 1.0f);
    ImGui::SliderFloat("Connection Probability", &m_connectionProbability, 0.0f, 1.0f);

    if (ImGui::Button("Generate New Station")) {
        generateLSystem();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        resetLSystem();
    }

    ImGui::Separator();
    ImGui::Text("Generated System Info:");
    ImGui::Text("Modules: %zu", m_modules.size());
    ImGui::Text("Connections: %zu", m_connections.size());

    ImGui::Separator();

    // Visual representation
    drawLSystemVisualization();

    ImGui::Separator();

    // Text output
    ImGui::Text("L-System String Output:");
    ImGui::InputTextMultiline("##lsystem_output",
        const_cast<char*>(m_lSystemString.c_str()),
        m_lSystemString.length() + 1,
        ImVec2(-1, 200),
        ImGuiInputTextFlags_ReadOnly);

    ImGui::End();
}

void Station::drawLSystemVisualization() {
    ImGui::Text("2D Station Layout:");

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImVec2(350, 250);

    // Draw background
    draw_list->AddRectFilled(canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(50, 50, 50, 255));
    draw_list->AddRect(canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(255, 255, 255, 255));

    if (m_modules.empty()) {
        ImGui::Dummy(canvas_size);
        return;
    }

    // Calculate bounds
    float minX = m_gridSize, maxX = 0, minY = m_gridSize, maxY = 0;
    for (const auto& module : m_modules) {
        glm::vec2 endPos = module.position + module.direction * float(module.length);
        minX = std::min(minX, std::min(module.position.x, endPos.x));
        maxX = std::max(maxX, std::max(module.position.x, endPos.x));
        minY = std::min(minY, std::min(module.position.y, endPos.y));
        maxY = std::max(maxY, std::max(module.position.y, endPos.y));
    }

    float scale = std::min(canvas_size.x / (maxX - minX + 10), canvas_size.y / (maxY - minY + 10));
    glm::vec2 offset = glm::vec2(canvas_pos.x + canvas_size.x * 0.5f, canvas_pos.y + canvas_size.y * 0.5f) -
        glm::vec2((maxX + minX) * 0.5f * scale, (maxY + minY) * 0.5f * scale);

    // Draw connections first (so they appear behind modules)
    for (const auto& conn : m_connections) {
        glm::vec2 posA = offset + m_modules[conn.moduleA].position * scale;
        glm::vec2 posB = offset + m_modules[conn.moduleB].position * scale;
        draw_list->AddLine(ImVec2(posA.x, posA.y), ImVec2(posB.x, posB.y),
            IM_COL32(100, 255, 100, 255), 2.0f);
    }

    // Draw modules
    for (const auto& module : m_modules) {
        glm::vec2 startPos = offset + module.position * scale;
        glm::vec2 endPos = offset + (module.position + module.direction * float(module.length)) * scale;

        ImU32 color = module.isRoot ? IM_COL32(255, 100, 100, 255) : IM_COL32(100, 150, 255, 255);
        draw_list->AddLine(ImVec2(startPos.x, startPos.y), ImVec2(endPos.x, endPos.y), color, 4.0f);

        // Draw start point
        draw_list->AddCircleFilled(ImVec2(startPos.x, startPos.y), 3.0f, color);

        // Draw module ID
        std::string idStr = std::to_string(module.id);
        draw_list->AddText(ImVec2(startPos.x + 5, startPos.y - 10), IM_COL32(255, 255, 255, 255), idStr.c_str());
    }

    ImGui::Dummy(canvas_size);
}