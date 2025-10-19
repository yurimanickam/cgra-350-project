#pragma once

#include <cgra/cgra_mesh.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <random>

struct LSystemNode {
    glm::vec2 position;
    float rotation;
    float length;
    int moduleType; // 0=corridor, 1=habitat, 2=docking, 3=power, etc.
    int generation;
    bool isActive;

    LSystemNode() : position(0.0f), rotation(0.0f), length(1.0f), moduleType(0), generation(0), isActive(true) {}
};

struct LSystemRule {
    char symbol;
    std::vector<std::string> productions;
    float probability;
    float lengthMultiplier;
    float angleVariation;
};

class Station {
public:
    // Existing cylinder mesh function
    cgra::gl_mesh createCylinderMesh(float radius, float height, int subdivisions, bool capped);

    // L-System functions
    void initializeLSystem();
    void generateLSystem();
    void interpretLSystem(const std::string& sequence);
    void renderLSystemGUI();
    void drawLSystemVisualization();

    // L-System parameters
    struct LSystemParams {
        int iterations = 3;
        float baseLength = 10.0f;
        float lengthVariation = 0.3f;
        float baseAngle = 90.0f;
        float angleVariation = 15.0f;
        float branchProbability = 0.7f;
        float connectionProbability = 0.2f;
        float lengthDecay = 0.8f;
        int maxConnections = 3;
        float minLength = 2.0f;
        bool allowLoops = true;
        int seed = 12345;
    } m_lsystemParams;

    void setPreviewZoom(float zoom) { m_previewZoom = glm::clamp(zoom, 0.2f, 8.0f); }
    float getPreviewZoom() const { return m_previewZoom; }

private:
    std::vector<LSystemNode> m_nodes;
    std::vector<std::pair<int, int>> m_connections;
    std::vector<LSystemRule> m_rules;
    std::string m_currentSequence;
    std::string m_axiom;
    std::mt19937 m_rng;

    float m_previewZoom = 1.0f;
    // Returns true if position is too close to any existing node (overlap)
    bool isOverlapping(const glm::vec2& pos, float minDist);


    // Interpretation state
    struct TurtleState {
        glm::vec2 position;
        float angle;
        float length;
        int generation;
    };
    std::vector<TurtleState> m_stateStack;

    void setupRules();
    void addConnection(int from, int to);
    bool canConnect(const glm::vec2& pos1, const glm::vec2& pos2);
    int findNearestNode(const glm::vec2& position, float maxDistance);
    std::string applyRules(const std::string& current);
    float getRandomFloat(float min, float max);
    int getRandomInt(int min, int max);
};