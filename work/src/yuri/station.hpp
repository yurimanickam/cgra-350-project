#pragma once

#include "cgra/cgra_mesh.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <random>

struct Module {
    glm::vec2 position;
    glm::vec2 direction; // normalized direction vector
    int length;
    int id;
    bool isRoot;
    int parentId;
};

struct Connection {
    int moduleA;
    int moduleB;
    glm::vec2 connectionPoint;
};

class Station {
public:
    // Original cylinder mesh creation
    cgra::gl_mesh createCylinderMesh(float radius, float height, int subdivisions, bool capped = true);

    // L-System Space Station Generator
    void generateLSystem();
    void renderLSystemGUI();
    void resetLSystem();

    // Getters for the generated system
    const std::vector<Module>& getModules() const { return m_modules; }
    const std::vector<Connection>& getConnections() const { return m_connections; }
    const std::string& getLSystemString() const { return m_lSystemString; }

private:
    // L-System parameters
    int m_numRoots = 4;
    int m_minBranchLength = 1;
    int m_maxBranchLength = 3;
    int m_minSpacing = 2;
    int m_maxIterations = 5;
    float m_branchProbability = 0.7f;
    float m_connectionProbability = 0.3f;
    int m_gridSize = 50;

    // Generation state
    std::vector<Module> m_modules;
    std::vector<Connection> m_connections;
    std::vector<std::vector<int>> m_occupancyGrid; // moduleId at each grid position, -1 = empty
    std::string m_lSystemString;
    std::mt19937 m_rng;
    int m_nextModuleId = 0;

    // Internal methods
    void initializeRoots();
    void growBranches();
    void findConnections();
    void updateLSystemString();
    glm::vec2 gridToWorld(int x, int y) const;
    glm::ivec2 worldToGrid(glm::vec2 pos) const;
    bool isValidPosition(glm::ivec2 gridPos, int length, glm::vec2 direction) const;
    bool hasMinimumSpacing(glm::ivec2 gridPos, glm::vec2 direction, int length) const;
    void occupyGridCells(const Module& module);
    std::vector<glm::vec2> getValidDirections() const;
    void drawLSystemVisualization();
};