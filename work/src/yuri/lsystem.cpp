#include "lsystem.hpp"
#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <iostream>

using namespace glm;

namespace {
    constexpr float HALF_PI = glm::half_pi<float>();
    constexpr float TWO_PI = glm::two_pi<float>();
    constexpr float PI = glm::pi<float>();
    constexpr int NUM_MODULE_TYPES = 4;
    constexpr float MODEL_LENGTH = 10.0f; // One model unit
    constexpr float JUNCTION_RADIUS = 2.95f; // Half of 5.9 (junction size)
}

LSystem::LSystem() {
    initialize();
}

LSystem::~LSystem() {
}

// Initialize L-system
void LSystem::initialize() {
    m_rng.seed(m_params.seed);
    setupRules();
    regenerate();
}

// Set up L-system rules
void LSystem::setupRules() {
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

// Generate new sequence
void LSystem::regenerate() {
    m_rng.seed(m_params.seed);
    generateSequence();
    interpretSequence(m_currentSequence);
}

// Generate sequence
void LSystem::generateSequence() {
    m_currentSequence = "X";
    for (int i = 0; i < m_params.iterations; ++i) {
        m_currentSequence = applyRules(m_currentSequence);
    }
}

// Apply rules to sequence
std::string LSystem::applyRules(const std::string& current) {
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

// Quantize length to EXACTLY 10, 20, or 30 units only
float LSystem::quantizeLength(float length) const {
    if (length < 15.0f) return 10.0f;      // 10 units
    else if (length < 25.0f) return 20.0f; // 20 units
    else return 30.0f;                      // 30 units
}

// Interpret sequence
void LSystem::interpretSequence(const std::string& sequence) {
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
            float rawLength = std::max(m_params.minLength, state.length);
            float actualLength = quantizeLength(rawLength);

            vec2 direction(std::cos(state.angle), std::sin(state.angle));

            // Calculate the total distance to move, including junction radii on both ends
            float totalDistance = actualLength + (2.0f * JUNCTION_RADIUS);
            vec2 newPos = state.position + direction * totalDistance;

            if (!isOverlapping(newPos, JUNCTION_RADIUS * 2.0f)) {
                int moduleType = getRandomInt(0, NUM_MODULE_TYPES - 1);
                addModule(state.position, newPos, state.angle, actualLength, moduleType, state.generation);
                addJunction(newPos, state.generation);

                // INCREASE vertical module probability
                if (m_params.allowVerticalModules
                    && !state.hasVerticalChild
                    && getRandomFloat(0.0f, 1.0f) < m_params.verticalProbability) {

                    bool pointingUp = getRandomFloat(0.0f, 1.0f) > 0.5f;
                    float vertLength = quantizeLength(actualLength * 0.7f);
                    int vertModuleType = getRandomInt(0, NUM_MODULE_TYPES - 1);
                    addVerticalModule(newPos, state.verticalOffset, pointingUp, vertLength, vertModuleType, state.generation);
                    state.hasVerticalChild = true;
                }

                // *** REMOVED LOOP CONNECTIONS ***
                // if (m_params.allowLoops
                //     && getRandomFloat(0.0f, 1.0f) < m_params.connectionProbability) {
                //     connectNearbyJunctions(newPos, state.generation);
                // }

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

// Add module
void LSystem::addModule(const glm::vec2& startPos, const glm::vec2& endPos, float rotation, float length, int moduleType, int generation) {
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

// Add vertical module
void LSystem::addVerticalModule(const glm::vec2& basePos, float baseVerticalOffset, bool pointingUp, float length, int moduleType, int generation) {
    StationModule module;
    module.startPos = basePos;
    module.endPos = basePos;
    module.rotation = 0.0f;
    module.length = length;
    module.moduleType = moduleType;
    module.generation = generation;
    module.verticalOffset = baseVerticalOffset;
    module.isVertical = true;

    // Account for junction radius in vertical direction
    float totalVerticalDistance = length + (2.0f * JUNCTION_RADIUS);
    float endVerticalOffset = pointingUp ? (baseVerticalOffset + totalVerticalDistance) : (baseVerticalOffset - totalVerticalDistance);
    addVerticalJunction(basePos, endVerticalOffset, generation);

    m_modules.push_back(module);
}

// Add junction
void LSystem::addJunction(const glm::vec2& position, int generation) {
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

// Add vertical junction
void LSystem::addVerticalJunction(const glm::vec2& position, float verticalOffset, int generation) {
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

// *** REMOVED LOOP CONNECTIONS ***
// void LSystem::connectNearbyJunctions(const glm::vec2& newJunctionPos, int generation) {
//     int nearJunction = findNearestJunction(newJunctionPos, m_params.baseLength * 2.0f);
//     if (nearJunction >= 0) {
//         const glm::vec2& targetPos = m_junctions[nearJunction].position;
//         float dist = length(targetPos - newJunctionPos);
//         // Minimum distance should account for two junction radii
//         float minConnectionDist = JUNCTION_RADIUS * 4.0f;
//         if (dist > minConnectionDist) {
//             vec2 dir = normalize(targetPos - newJunctionPos);
//             // The actual module length is the distance minus the two junction radii
//             float actualModuleLength = dist - (2.0f * JUNCTION_RADIUS);
//             float quantizedLength = quantizeLength(actualModuleLength);

//             // Recalculate end position based on quantized length
//             float totalDistance = quantizedLength + (2.0f * JUNCTION_RADIUS);
//             vec2 adjustedEndPos = newJunctionPos + dir * totalDistance;

//             float angle = std::atan2(dir.y, dir.x);
//             int moduleType = 0;
//             addModule(newJunctionPos, adjustedEndPos, angle, quantizedLength, moduleType, generation);
//         }
//     }
// }

// Check overlap
bool LSystem::isOverlapping(const vec2& pos, float minDist) const {
    for (const auto& junction : m_junctions) {
        if (length(junction.position - pos) < minDist)
            return true;
    }
    return false;
}

// Find nearest junction
int LSystem::findNearestJunction(const glm::vec2& position, float maxDistance) const {
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

// Random float
float LSystem::getRandomFloat(float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(m_rng);
}

// Random int
int LSystem::getRandomInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(m_rng);
}