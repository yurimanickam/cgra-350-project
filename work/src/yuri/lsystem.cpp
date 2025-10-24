#include "lsystem.hpp"

#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

// math/physics constants
constexpr float HALF_PI = glm::half_pi<float>();
constexpr float TWO_PI = glm::two_pi<float>();
constexpr float JUNCTION_RADIUS = 2.95f;
constexpr int NUM_MODULE_TYPES = 4;
constexpr float LENGTH_THRESHOLD_SMALL = 15.0f;
constexpr float LENGTH_THRESHOLD_MEDIUM = 25.0f;
constexpr float LENGTH_SMALL = 10.0f;
constexpr float LENGTH_MEDIUM = 20.0f;
constexpr float LENGTH_LARGE = 30.0f;
constexpr float JUNCTION_OVERLAP_THRESHOLD = 0.1f;

LSystem::LSystem() {
    initialize();
}

void LSystem::initialize() {
    m_rng.seed(m_params.seed);
    setupRules();
    regenerate();
}

void LSystem::setupRules() {
    m_rules.clear();
    m_rules.reserve(4);

    // sets up main l-system rules
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

void LSystem::regenerate() {
    m_rng.seed(m_params.seed);
    generateSequence();
    interpretSequence(m_currentSequence);
}

void LSystem::generateSequence() {
    m_currentSequence = "X";
    for (int i = 0; i < m_params.iterations; ++i) {
        m_currentSequence = applyRules(m_currentSequence);
    }
}

std::string LSystem::applyRules(const std::string& current) {
    std::string result;
    result.reserve(current.size() * 2);

    // applies grammar rules to each symbol
    for (char c : current) {
        bool ruleApplied = false;
        for (const auto& rule : m_rules) {
            if (rule.symbol == c) {
                if (getRandomFloat(0.0f, 1.0f) < rule.probability && !rule.productions.empty()) {
                    const int idx = getRandomInt(0, static_cast<int>(rule.productions.size()) - 1);
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

float LSystem::quantizeLength(float length) const {
    if (length < LENGTH_THRESHOLD_SMALL) {
        return LENGTH_SMALL;
    }
    else if (length < LENGTH_THRESHOLD_MEDIUM) {
        return LENGTH_MEDIUM;
    }
    return LENGTH_LARGE;
}

void LSystem::interpretSequence(const std::string& sequence) {
    m_modules.clear();
    m_junctions.clear();
    m_stateStack.clear();

    // does turtle graphics interpretation
    TurtleState state;
    state.position = glm::vec2(0.0f);
    state.angle = 0.0f;
    state.length = m_params.baseLength;
    state.generation = 0;
    state.verticalOffset = 0.0f;

    addJunction(state.position, state.generation);

    for (char command : sequence) {
        switch (command) {
        case 'F': {
            const float rawLength = std::max(m_params.minLength, state.length);
            const float actualLength = quantizeLength(rawLength);
            const glm::vec2 direction(std::cos(state.angle), std::sin(state.angle));
            const float totalDistance = actualLength + (2.0f * JUNCTION_RADIUS);
            const glm::vec2 newPos = state.position + direction * totalDistance;

            if (!isOverlapping(newPos, JUNCTION_RADIUS * 2.0f)) {
                const int moduleType = getRandomInt(0, NUM_MODULE_TYPES - 1);
                addModule(state.position, newPos, state.angle, actualLength, moduleType, state.generation);
                addJunction(newPos, state.generation);

                if (m_params.allowVerticalModules &&
                    getRandomFloat(0.0f, 1.0f) < m_params.verticalProbability) {
                    const bool pointingUp = getRandomFloat(0.0f, 1.0f) > 0.5f;
                    const float vertLength = quantizeLength(actualLength * 0.7f);
                    const int vertModuleType = getRandomInt(0, NUM_MODULE_TYPES - 1);
                    addVerticalModule(newPos, state.verticalOffset, pointingUp,
                        vertLength, vertModuleType, state.generation);
                }
                state.position = newPos;
                state.length *= m_params.lengthDecay;
            }
            break;
        }
        case '+':
            state.angle += HALF_PI;
            if (state.angle >= TWO_PI) {
                state.angle -= TWO_PI;
            }
            break;
        case '-':
            state.angle -= HALF_PI;
            if (state.angle < 0.0f) {
                state.angle += TWO_PI;
            }
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

void LSystem::addModule(const glm::vec2& startPos, const glm::vec2& endPos,
    float rotation, float length, int moduleType, int generation) {
    // adds a station module
    StationModule module;
    module.startPos = startPos;
    module.endPos = endPos;
    module.rotation = rotation;
    module.length = length;
    module.moduleType = moduleType;
    module.generation = generation;
    module.verticalOffset = 0.0f;
    module.isVertical = false;
    module.hasSolarPanels = m_params.enableSolarPanels &&
        (getRandomFloat(0.0f, 1.0f) < m_params.solarPanelProbability);
    m_modules.push_back(module);
}

void LSystem::addVerticalModule(const glm::vec2& basePos, float baseVerticalOffset,
    bool pointingUp, float length, int moduleType, int generation) {
    // adds a vertical module
    StationModule module;
    module.startPos = basePos;
    module.endPos = basePos;
    module.rotation = 0.0f;
    module.length = length;
    module.moduleType = moduleType;
    module.generation = generation;
    module.verticalOffset = baseVerticalOffset;
    module.isVertical = true;
    module.hasSolarPanels = m_params.enableSolarPanels &&
        (getRandomFloat(0.0f, 1.0f) < m_params.solarPanelProbability);

    const float totalVerticalDistance = length + (2.0f * JUNCTION_RADIUS);
    const float endVerticalOffset = pointingUp ?
        (baseVerticalOffset + totalVerticalDistance) :
        (baseVerticalOffset - totalVerticalDistance);

    addJunction(basePos, generation, endVerticalOffset);
    m_modules.push_back(module);
}

void LSystem::addJunction(const glm::vec2& position, int generation, float verticalOffset) {
    // adds a junction if not overlapping
    for (const auto& junction : m_junctions) {
        if (glm::length(junction.position - position) < JUNCTION_OVERLAP_THRESHOLD &&
            std::abs(junction.verticalOffset - verticalOffset) < JUNCTION_OVERLAP_THRESHOLD) {
            return;
        }
    }
    ModuleJunction junction;
    junction.position = position;
    junction.generation = generation;
    junction.verticalOffset = verticalOffset;
    m_junctions.push_back(junction);
}

bool LSystem::isOverlapping(const glm::vec2& pos, float minDist) const {
    for (const auto& junction : m_junctions) {
        if (glm::length(junction.position - pos) < minDist) {
            return true;
        }
    }
    return false;
}

float LSystem::getRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(m_rng);
}

int LSystem::getRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(m_rng);
}