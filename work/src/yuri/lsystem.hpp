#pragma once

#include <glm/glm.hpp>
#include <random>
#include <string>
#include <vector>

/// @brief Represents a functional segment of the space station
struct StationModule {
    glm::vec2 startPos{ 0.0f };
    glm::vec2 endPos{ 0.0f };
    float rotation{ 0.0f };
    float length{ 10.0f };
    float verticalOffset{ 0.0f };
    int moduleType{ 0 };        // 0=corridor, 1=habitat, 2=docking, 3=power
    int generation{ 0 };
    bool isVertical{ false };
    bool hasSolarPanels{ false };
};

/// @brief Represents a connection point between modules
struct ModuleJunction {
    glm::vec2 position{ 0.0f };
    float verticalOffset{ 0.0f };
    int generation{ 0 };
};

/// @brief Production rule for L-system grammar
struct LSystemRule {
    char symbol;
    std::vector<std::string> productions;
    float probability;
};

/// @brief Configuration parameters for L-system generation
struct LSystemParams {
    int iterations{ 3 };
    int seed{ 1701 };
    float baseLength{ 10.0f };
    float baseAngle{ 90.0f };
    float lengthDecay{ 0.9f };
    float minLength{ 10.0f };
    float verticalProbability{ 0.35f };
    float solarPanelProbability{ 0.3f };
    bool allowVerticalModules{ true };
    bool enableSolarPanels{ false };
};

/// @brief L-system generator for procedural space station creation
class LSystem {
public:
    LSystem();
    ~LSystem() = default;

    // Prevent copying
    LSystem(const LSystem&) = delete;
    LSystem& operator=(const LSystem&) = delete;

    /// @brief Initialize the L-system with current parameters
    void initialize();

    /// @brief Regenerate the station structure
    void regenerate();

    // Accessors
    LSystemParams& getParams() { return m_params; }
    const LSystemParams& getParams() const { return m_params; }
    const std::vector<StationModule>& getModules() const { return m_modules; }
    const std::vector<ModuleJunction>& getJunctions() const { return m_junctions; }
    const std::string& getCurrentSequence() const { return m_currentSequence; }

private:
    /// @brief Turtle state for L-system interpretation
    struct TurtleState {
        glm::vec2 position{ 0.0f };
        float angle{ 0.0f };
        float length{ 10.0f };
        float verticalOffset{ 0.0f };
        int generation{ 0 };
    };

    // Member variables
    LSystemParams m_params;
    std::vector<StationModule> m_modules;
    std::vector<ModuleJunction> m_junctions;
    std::vector<LSystemRule> m_rules;
    std::vector<TurtleState> m_stateStack;
    std::string m_currentSequence;
    std::mt19937 m_rng;

    // L-System generation
    void setupRules();
    void generateSequence();
    std::string applyRules(const std::string& current);
    void interpretSequence(const std::string& sequence);

    // Module management
    void addModule(const glm::vec2& startPos, const glm::vec2& endPos,
        float rotation, float length, int moduleType, int generation);
    void addVerticalModule(const glm::vec2& basePos, float baseVerticalOffset,
        bool pointingUp, float length, int moduleType, int generation);
    void addJunction(const glm::vec2& position, int generation, float verticalOffset = 0.0f);

    // Utility functions
    float quantizeLength(float length) const;
    bool isOverlapping(const glm::vec2& pos, float minDist) const;
    float getRandomFloat(float min, float max);
    int getRandomInt(int min, int max);
};