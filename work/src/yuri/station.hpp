#pragma once

#include <cgra/cgra_mesh.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <random>

// Represents a single module/node in the space station
struct LSystemNode {
    glm::vec2 position;
    float rotation;
    float length;
    int moduleType; // 0=corridor, 1=habitat, 2=docking, 3=power
    int generation;

    LSystemNode()
        : position(0.0f)
        , rotation(0.0f)
        , length(1.0f)
        , moduleType(0)
        , generation(0)
    {
    }
};

// Production rule for L-System
struct LSystemRule {
    char symbol;
    std::vector<std::string> productions;
    float probability;
};

// L-System generation parameters
struct LSystemParams {
    int iterations = 3;
    float baseLength = 10.0f;
    float baseAngle = 90.0f;
    float lengthDecay = 0.8f;
    float connectionProbability = 0.2f;
    float minLength = 2.0f;
    bool allowLoops = true;
    int seed = 12345;
};

class Station {
public:
    Station();

    // Mesh generation
    cgra::gl_mesh createCylinderMesh(float radius, float height, int subdivisions, bool capped);

    // L-System generation
    void initializeLSystem();
    void regenerate();

    // GUI rendering
    void renderGUI();

    // 3D rendering
    void render3DStation(const glm::mat4& view, const glm::mat4& proj, GLuint shader);

    // Accessors
    LSystemParams& getParams() { return m_params; }
    const std::vector<LSystemNode>& getNodes() const { return m_nodes; }
    const std::vector<std::pair<int, int>>& getConnections() const { return m_connections; }
    bool shouldDrawStation() const { return m_drawStation; }

private:
    // L-System data
    LSystemParams m_params;
    std::vector<LSystemNode> m_nodes;
    std::vector<std::pair<int, int>> m_connections;
    std::vector<LSystemRule> m_rules;
    std::string m_currentSequence;
    std::mt19937 m_rng;

    // Visualization
    float m_previewZoom = 1.0f;
    bool m_drawStation = true;

    // 3D rendering
    cgra::gl_mesh m_cylinderMesh;
    cgra::gl_mesh m_nodeMesh;
    float m_cylinderRadius = 2.0f;
    float m_nodeRadius = 3.0f;

    // Turtle state for interpretation
    struct TurtleState {
        glm::vec2 position;
        float angle;
        float length;
        int generation;
    };
    std::vector<TurtleState> m_stateStack;

    // L-System generation
    void setupRules();
    void generateSequence();
    std::string applyRules(const std::string& current);
    void interpretSequence(const std::string& sequence);

    // Node management
    void addConnection(int from, int to);
    bool isOverlapping(const glm::vec2& pos, float minDist) const;
    int findNearestNode(const glm::vec2& position, float maxDistance) const;

    // Random utilities
    float getRandomFloat(float min, float max);
    int getRandomInt(int min, int max);

    // GUI
    void renderControlsGUI();
    void renderPreviewGUI();
    void drawVisualization();
    void calculateBounds(glm::vec2& minBounds, glm::vec2& maxBounds) const;

    // 3D rendering helpers
    void initializeMeshes();
    glm::mat4 calculateConnectionTransform(const LSystemNode& from, const LSystemNode& to, float gapSize) const;
    glm::vec3 getModuleColor(int moduleType) const;
};