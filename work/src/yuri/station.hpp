
#pragma once

#include <cgra/cgra_mesh.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <random>

// A Module is a functional segment of the station (rendered as a tube/cylinder)
struct StationModule {
    glm::vec2 startPos;      // Start position in 2D layout
    glm::vec2 endPos;        // End position in 2D layout
    float rotation;          // Direction angle
    float length;            // Length of the module
    int moduleType;          // 0=corridor, 1=habitat, 2=docking, 3=power
    int generation;          // L-System generation level
    float verticalOffset;    // Y-axis offset for vertical modules
    bool isVertical;         // True if module points up/down

    StationModule()
        : startPos(0.0f)
        , endPos(0.0f)
        , rotation(0.0f)
        , length(1.0f)
        , moduleType(0)
        , generation(0)
        , verticalOffset(0.0f)
        , isVertical(false)
    {
    }
};

// A Junction is a connection point between modules (rendered as a sphere)
struct ModuleJunction {
    glm::vec2 position;
    int generation;
    float verticalOffset;    // Y-axis offset for vertical junctions

    ModuleJunction()
        : position(0.0f)
        , generation(0)
        , verticalOffset(0.0f)
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
    bool allowVerticalModules = true;  // New parameter
    float verticalProbability = 0.15f; // New parameter
    int seed = 1701;
};

struct Rendering3DParams {
    float junctionRadius = 1.0f;    // Size of junction spheres
    float moduleRadius = 1.5f;       // Radius/thickness of module tubes
    float gapMultiplier = 0.0f;      // Gap between modules and junctions
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
    const std::vector<StationModule>& getModules() const { return m_modules; }
    const std::vector<ModuleJunction>& getJunctions() const { return m_junctions; }
    bool shouldDrawStation() const { return m_drawStation; }

private:
    // L-System data
    LSystemParams m_params;
    std::vector<StationModule> m_modules;        // The functional modules (tubes)
    std::vector<ModuleJunction> m_junctions;     // Connection points (spheres)
    std::vector<LSystemRule> m_rules;
    std::string m_currentSequence;
    std::mt19937 m_rng;

    // Visualization
    float m_previewZoom = 1.0f;
    glm::vec2 m_previewPan = glm::vec2(0.0f);
    bool m_drawStation = false;

    // 3D rendering
    Rendering3DParams m_renderParams;
    cgra::gl_mesh m_moduleMesh;      // Cylinder mesh for modules
    cgra::gl_mesh m_junctionMesh;    // Sphere mesh for junctions
    bool m_meshNeedsRebuild = true;

    // Turtle state
    struct TurtleState {
        glm::vec2 position;
        float angle;
        float length;
        int generation;
        float verticalOffset;        // New field
        bool hasVerticalChild;       // New field - tracks if this branch already spawned a vertical module
    };
    std::vector<TurtleState> m_stateStack;

    // L-System generation
    void setupRules();
    void generateSequence();
    std::string applyRules(const std::string& current);
    void interpretSequence(const std::string& sequence);

    // Module management
    void addModule(const glm::vec2& startPos, const glm::vec2& endPos, float rotation, float length, int moduleType, int generation);
    void addJunction(const glm::vec2& position, int generation);
    void addVerticalModule(const glm::vec2& basePos, float baseVerticalOffset, bool pointingUp, float length, int moduleType, int generation); // New method
    void addVerticalJunction(const glm::vec2& position, float verticalOffset, int generation); // New method
    void connectNearbyJunctions(const glm::vec2& newJunctionPos, int generation);
    bool isOverlapping(const glm::vec2& pos, float minDist) const;
    int findNearestJunction(const glm::vec2& position, float maxDistance) const;

    // Random utilities
    float getRandomFloat(float min, float max);
    int getRandomInt(int min, int max);

    // GUI
    void applyUIStyle();
    void renderControlsPanel();
    void renderPreviewPanel();
    void drawVisualization();
    void calculateBounds(glm::vec2& minBounds, glm::vec2& maxBounds) const;

    // 3D rendering helpers
    void rebuildMeshes();
    glm::mat4 calculateModuleTransform(const StationModule& module, float gapSize) const;
    glm::vec3 getModuleColor(int moduleType) const;
};
