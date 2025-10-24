#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <random>

// A Module is a functional segment of the station (rendered as one of three model sizes)
struct StationModule {
    glm::vec2 startPos;      // Start position in 2D layout
    glm::vec2 endPos;        // End position in 2D layout
    float rotation;          // Direction angle
    float length;            // Length of the module (10, 20, or 30 units)
    int moduleType;          // 0=corridor, 1=habitat, 2=docking, 3=power
    int generation;          // L-System generation level
    float verticalOffset;    // Y-axis offset for vertical modules
    bool isVertical;         // True if module points up/down

    StationModule()
        : startPos(0.0f)
        , endPos(0.0f)
        , rotation(0.0f)
        , length(10.0f)
        , moduleType(0)
        , generation(0)
        , verticalOffset(0.0f)
        , isVertical(false)
    {
    }
};

// A Junction is a connection point between modules (rendered as junction.obj)
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

// *** REMOVED LOOP CONNECTION parameters ***
struct LSystemParams {
    int iterations = 3;
    float baseLength = 10.0f;      // Base module length (10, 20, or 30)
    float baseAngle = 90.0f;
    float lengthDecay = 0.8f;
    // float connectionProbability = 0.2f; // REMOVED
    float minLength = 10.0f;       // Minimum module length
    // bool allowLoops = true;       // REMOVED
    bool allowVerticalModules = true;
    float verticalProbability = 0.35f; // *** Increased from 0.15f for more vertical modules ***
    int seed = 1701;
};

class LSystem {
public:
    LSystem();
    ~LSystem();

    // Initialize and regenerate
    void initialize();
    void regenerate();

    // Accessors
    LSystemParams& getParams() { return m_params; }
    const std::vector<StationModule>& getModules() const { return m_modules; }
    const std::vector<ModuleJunction>& getJunctions() const { return m_junctions; }
    const std::string& getCurrentSequence() const { return m_currentSequence; }

private:
    // L-System data
    LSystemParams m_params;
    std::vector<StationModule> m_modules;
    std::vector<ModuleJunction> m_junctions;
    std::vector<LSystemRule> m_rules;
    std::string m_currentSequence;
    std::mt19937 m_rng;

    // Turtle state
    struct TurtleState {
        glm::vec2 position;
        float angle;
        float length;
        int generation;
        float verticalOffset;
        bool hasVerticalChild;
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
    void addVerticalModule(const glm::vec2& basePos, float baseVerticalOffset, bool pointingUp, float length, int moduleType, int generation);
    void addVerticalJunction(const glm::vec2& position, float verticalOffset, int generation);
    // void connectNearbyJunctions(const glm::vec2& newJunctionPos, int generation); // REMOVED
    bool isOverlapping(const glm::vec2& pos, float minDist) const;
    int findNearestJunction(const glm::vec2& position, float maxDistance) const;

    // Helper to quantize length to 10, 20, or 30 units
    float quantizeLength(float length) const;

    // Random utilities
    float getRandomFloat(float min, float max);
    int getRandomInt(int min, int max);
};