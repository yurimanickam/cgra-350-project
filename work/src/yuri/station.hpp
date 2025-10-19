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

struct LSystemRule {
    char symbol;
    std::vector<std::string> productions;
    float probability;
};

struct LSystemParams {
    int iterations = 3;
    float baseLength = 10.0f;
    float baseAngle = 90.0f;
    float lengthDecay = 0.8f;
    float connectionProbability = 0.2f;
    float minLength = 2.0f;
    bool allowLoops = true;
    int seed = 1701;
};

struct Rendering3DParams {
    float nodeRadius = 1.0f;
    float tubeRadius = 1.5f;
    float gapMultiplier = 0.0f;
};

class Station {
public:
    Station();

    // Mesh generation
    cgra::gl_mesh createCylinderMesh(float radius, float height, int subdivisions, bool capped);
    cgra::gl_mesh createSphereMesh(float radius, int stacks, int slices);

    // L-System generation
    void initializeLSystem();
    void regenerate();

    // GUI rendering
    void renderGUI();

    // 3D rendering
    void render3DStation(const glm::mat4& view, const glm::mat4& proj, GLuint shader);

    // Accessors
    LSystemParams& getParams() { return m_params; }
    Rendering3DParams& getRenderParams() { return m_renderParams; }
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
    glm::vec2 m_previewPan = glm::vec2(0.0f);
    bool m_drawStation = true;

    // 3D rendering
    Rendering3DParams m_renderParams;
    cgra::gl_mesh m_cylinderMesh;
    cgra::gl_mesh m_nodeMesh;
    bool m_meshNeedsRebuild = true;

    // Turtle state
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
    void applyUIStyle();
    void renderControlsPanel();
    void renderPreviewPanel();
    void renderControlsGUI(); // Legacy
    void renderPreviewGUI(); // Legacy
    void drawVisualization();
    void calculateBounds(glm::vec2& minBounds, glm::vec2& maxBounds) const;

    // 3D rendering helpers
    void rebuildMeshes();
    glm::mat4 calculateConnectionTransform(const LSystemNode& from, const LSystemNode& to, float gapSize) const;
    glm::vec3 getModuleColor(int moduleType) const;
};