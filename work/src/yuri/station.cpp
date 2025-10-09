#include "station.hpp"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <random>
#include <ctime>
#include <stack>
#include <cmath>

// Create a cylinder mesh for space station modules
void createCylinderMesh(StationModule& module, float length, float radius, int segments) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    const float halfLength = length / 2.0f;

    // Generate side vertices (cylinder body)
    for (int ring = 0; ring <= 1; ++ring) {
        float z = ring == 0 ? -halfLength : halfLength;

        for (int i = 0; i <= segments; ++i) {
            float theta = (float)i / (float)segments * 2.0f * glm::pi<float>();
            float x = radius * cos(theta);
            float y = radius * sin(theta);

            // Position
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // Normal (pointing outward)
            glm::vec3 normal = glm::normalize(glm::vec3(x, y, 0.0f));
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);

            // UV
            vertices.push_back((float)i / (float)segments);
            vertices.push_back((float)ring);
        }
    }

    // Generate indices for cylinder sides
    for (int i = 0; i < segments; ++i) {
        int bottomLeft = i;
        int bottomRight = i + 1;
        int topLeft = i + (segments + 1);
        int topRight = i + 1 + (segments + 1);

        // First triangle
        indices.push_back(bottomLeft);
        indices.push_back(topLeft);
        indices.push_back(bottomRight);

        // Second triangle
        indices.push_back(bottomRight);
        indices.push_back(topLeft);
        indices.push_back(topRight);
    }

    // Add front cap center vertex
    int frontCenterIndex = vertices.size() / 8; // 8 floats per vertex
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(-halfLength);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(-1.0f);
    vertices.push_back(0.5f);
    vertices.push_back(0.5f);

    // Add front cap edge vertices
    int frontCapStart = vertices.size() / 8;
    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / (float)segments * 2.0f * glm::pi<float>();
        float x = radius * cos(theta);
        float y = radius * sin(theta);

        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(-halfLength);
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
        vertices.push_back(-1.0f);
        vertices.push_back((float)i / (float)segments);
        vertices.push_back(0.0f);
    }

    // Generate front cap indices
    for (int i = 0; i < segments; ++i) {
        indices.push_back(frontCenterIndex);
        indices.push_back(frontCapStart + i);
        indices.push_back(frontCapStart + i + 1);
    }

    // Add back cap center vertex
    int backCenterIndex = vertices.size() / 8;
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(halfLength);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(1.0f);
    vertices.push_back(0.5f);
    vertices.push_back(0.5f);

    // Add back cap edge vertices
    int backCapStart = vertices.size() / 8;
    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / (float)segments * 2.0f * glm::pi<float>();
        float x = radius * cos(theta);
        float y = radius * sin(theta);

        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(halfLength);
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);
        vertices.push_back((float)i / (float)segments);
        vertices.push_back(1.0f);
    }

    // Generate back cap indices
    for (int i = 0; i < segments; ++i) {
        indices.push_back(backCenterIndex);
        indices.push_back(backCapStart + i + 1);
        indices.push_back(backCapStart + i);
    }

    // Clean up old buffers
    if (module.vao != 0) {
        glDeleteVertexArrays(1, &module.vao);
        glDeleteBuffers(1, &module.vbo);
        if (module.ebo != 0) {
            glDeleteBuffers(1, &module.ebo);
        }
    }

    // Create new buffers
    glGenVertexArrays(1, &module.vao);
    glGenBuffers(1, &module.vbo);
    glGenBuffers(1, &module.ebo);

    glBindVertexArray(module.vao);

    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, module.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, module.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Set up vertex attributes
    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    // Normal attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    // UV attribute
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    glBindVertexArray(0);

    module.length = length;
    module.radius = radius;
    module.indexCount = indices.size(); // Store index count for rendering
}

// L-System string generation
std::string generateLSystemString(const LSystemParams& params) {
    std::string current = params.axiom;
    std::mt19937 rng(params.randomSeed);

    for (int iter = 0; iter < params.iterations; ++iter) {
        std::string next = "";

        for (char symbol : current) {
            bool replaced = false;

            // Find matching rule
            for (const auto& rule : params.rules) {
                if (rule.symbol == symbol) {
                    // Check probability for stochastic L-systems
                    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
                    if (dist(rng) <= rule.probability) {
                        next += rule.replacement;
                        replaced = true;
                        break;
                    }
                }
            }

            // If no rule applied, keep the symbol
            if (!replaced) {
                next += symbol;
            }
        }

        current = next;
    }

    return current;
}

// Interpret L-System string to generate station modules
std::vector<StationModule> interpretLSystemToStation(
    const std::string& lSystemString, 
    const LSystemParams& params) 
{
    std::vector<StationModule> modules;
    std::stack<TurtleState> stateStack;
    
    TurtleState turtle;
    turtle.currentLength = 6.0f;
    turtle.currentRadius = 1.0f;
    turtle.position = glm::vec3(0.0f);
    turtle.direction = glm::vec3(1.0f, 0.0f, 0.0f);
    turtle.up = glm::vec3(0.0f, 1.0f, 0.0f);
    turtle.left = glm::vec3(0.0f, 0.0f, 1.0f);
    
    for (char symbol : lSystemString) {
        switch (symbol) {
            case 'F': // Forward: draw main corridor
            case 'A': { // Main module
                StationModule module;
                module.type = StationModule::MAIN_CORRIDOR;
                module.generation = turtle.generation;
                
                // Create cylinder mesh
                createCylinderMesh(module, turtle.currentLength, turtle.currentRadius, 32);
                
                // Create transformation matrix
                glm::vec3 center = turtle.position + turtle.direction * (turtle.currentLength * 0.5f);
                
                // Calculate rotation to align cylinder (default along Z) with turtle direction
                glm::vec3 defaultDir(0.0f, 0.0f, 1.0f);
                glm::vec3 rotAxis = glm::cross(defaultDir, turtle.direction);
                float rotAngle = acos(glm::dot(defaultDir, glm::normalize(turtle.direction)));
                
                module.model = glm::translate(glm::mat4(1.0f), center);
                
                if (glm::length(rotAxis) > 0.001f) {
                    module.model = glm::rotate(module.model, rotAngle, glm::normalize(rotAxis));
                }
                
                modules.push_back(module);
                
                // Move turtle forward
                turtle.position += turtle.direction * turtle.currentLength;
                break;
            }
            
            case '+': // Turn up (pitch up)
                turtle.direction = glm::rotate(glm::mat4(1.0f), glm::radians(params.branchAngle), turtle.left) * glm::vec4(turtle.direction, 1.0f);
                turtle.up = glm::rotate(glm::mat4(1.0f), glm::radians(params.branchAngle), turtle.left) * glm::vec4(turtle.up, 1.0f);
                break;
                
            case '-': // Turn down (pitch down)
                turtle.direction = glm::rotate(glm::mat4(1.0f), glm::radians(-params.branchAngle), turtle.left) * glm::vec4(turtle.direction, 1.0f);
                turtle.up = glm::rotate(glm::mat4(1.0f), glm::radians(-params.branchAngle), turtle.left) * glm::vec4(turtle.up, 1.0f);
                break;
                
            case '&': // Turn left (yaw left)
                turtle.direction = glm::rotate(glm::mat4(1.0f), glm::radians(params.branchAngle), turtle.up) * glm::vec4(turtle.direction, 1.0f);
                turtle.left = glm::rotate(glm::mat4(1.0f), glm::radians(params.branchAngle), turtle.up) * glm::vec4(turtle.left, 1.0f);
                break;
                
            case '^': // Turn right (yaw right)
                turtle.direction = glm::rotate(glm::mat4(1.0f), glm::radians(-params.branchAngle), turtle.up) * glm::vec4(turtle.direction, 1.0f);
                turtle.left = glm::rotate(glm::mat4(1.0f), glm::radians(-params.branchAngle), turtle.up) * glm::vec4(turtle.left, 1.0f);
                break;
                
            case '\\': // Roll left
                turtle.up = glm::rotate(glm::mat4(1.0f), glm::radians(params.branchAngle), turtle.direction) * glm::vec4(turtle.up, 1.0f);
                turtle.left = glm::rotate(glm::mat4(1.0f), glm::radians(params.branchAngle), turtle.direction) * glm::vec4(turtle.left, 1.0f);
                break;
                
            case '/': // Roll right
                turtle.up = glm::rotate(glm::mat4(1.0f), glm::radians(-params.branchAngle), turtle.direction) * glm::vec4(turtle.up, 1.0f);
                turtle.left = glm::rotate(glm::mat4(1.0f), glm::radians(-params.branchAngle), turtle.direction) * glm::vec4(turtle.left, 1.0f);
                break;
                
            case '[': // Push state (save current position/orientation)
                stateStack.push(turtle);
                turtle.generation++;
                turtle.currentLength *= params.lengthScale;
                turtle.currentRadius *= params.radiusScale;
                break;
                
            case ']': // Pop state (restore position/orientation)
                if (!stateStack.empty()) {
                    turtle = stateStack.top();
                    stateStack.pop();
                }
                break;
                
            default:
                // Ignore unknown symbols
                break;
        }
    }
    
    return modules;
}

// Create preset L-system parameters
LSystemParams createStandardStationParams() {
    LSystemParams params;
    params.axiom = "A";
    params.iterations = 3;
    params.lengthScale = 0.7f;
    params.radiusScale = 0.75f;
    params.branchAngle = 90.0f;
    params.randomSeed = static_cast<unsigned>(std::time(nullptr));

    params.rules.push_back({ 'A', "F[+A][-A][&A][^A]A", 1.0f });
    params.rules.push_back({ 'F', "FF", 0.8f });

    return params;
}

LSystemParams createComplexStationParams() {
    LSystemParams params;
    params.axiom = "A";
    params.iterations = 4;
    params.lengthScale = 0.65f;
    params.radiusScale = 0.7f;
    params.branchAngle = 90.0f;
    params.randomSeed = static_cast<unsigned>(std::time(nullptr));

    params.rules.push_back({ 'A', "F[+A][-A][&A][^A][\\A][/A]F", 0.6f });
    params.rules.push_back({ 'A', "F[+A][-A][&A][^A]A", 0.4f });
    params.rules.push_back({ 'F', "FA", 1.0f });

    return params;
}

LSystemParams createMinimalStationParams() {
    LSystemParams params;
    params.axiom = "A";
    params.iterations = 2;
    params.lengthScale = 0.75f;
    params.radiusScale = 0.8f;
    params.branchAngle = 90.0f;
    params.randomSeed = static_cast<unsigned>(std::time(nullptr));

    params.rules.push_back({ 'A', "F[+A][-A]", 1.0f });
    params.rules.push_back({ 'F', "F", 1.0f });

    return params;
}

// Create custom L-system parameters
LSystemParams createCustomStationParams(
    int iterations,
    float lengthScale,
    float radiusScale,
    float branchAngle,
    unsigned int randomSeed)
{
    LSystemParams params;
    params.axiom = "A";
    params.iterations = iterations;
    params.lengthScale = lengthScale;
    params.radiusScale = radiusScale;
    params.branchAngle = branchAngle;
    params.randomSeed = randomSeed;

    // Use standard rules but with custom parameters
    if (iterations <= 2) {
        // Minimal
        params.rules.push_back({ 'A', "F[+A][-A]", 1.0f });
        params.rules.push_back({ 'F', "F", 1.0f });
    }
    else if (iterations == 3) {
        // Standard
        params.rules.push_back({ 'A', "F[+A][-A][&A][^A]A", 1.0f });
        params.rules.push_back({ 'F', "FF", 0.8f });
    }
    else {
        // Complex
        params.rules.push_back({ 'A', "F[+A][-A][&A][^A][\\A][/A]F", 0.6f });
        params.rules.push_back({ 'A', "F[+A][-A][&A][^A]A", 0.4f });
        params.rules.push_back({ 'F', "FA", 1.0f });
    }

    return params;
}

// Generate a complete procedural space station
std::vector<StationModule> generateProceduralStation(
    const LSystemParams& params,
    float mainCylinderLength,
    float mainCylinderRadius)
{
    std::string lSystemString = generateLSystemString(params);
    std::cout << "Generated L-System: " << lSystemString << std::endl;

    auto modules = interpretLSystemToStation(lSystemString, params);
    std::cout << "Generated " << modules.size() << " station modules" << std::endl;

    return modules;
}

// Render station modules with PBR
void renderStationModulesPBR(
    const std::vector<StationModule>& modules,
    const glm::mat4& view,
    const glm::mat4& proj,
    unsigned int pbrShader)
{
    glUseProgram(pbrShader);

    GLint locProj = glGetUniformLocation(pbrShader, "projection");
    GLint locView = glGetUniformLocation(pbrShader, "view");
    if (locProj != -1) glUniformMatrix4fv(locProj, 1, GL_FALSE, glm::value_ptr(proj));
    if (locView != -1) glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(view));

    for (const StationModule& module : modules) {
        glm::mat4 model = module.model;
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

        GLint locModel = glGetUniformLocation(pbrShader, "model");
        GLint locNormal = glGetUniformLocation(pbrShader, "normalMatrix");
        if (locModel != -1) glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
        if (locNormal != -1) glUniformMatrix3fv(locNormal, 1, GL_FALSE, glm::value_ptr(normalMatrix));

        glBindVertexArray(module.vao);

        // Draw using indices
        glDrawElements(GL_TRIANGLES, module.indexCount, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

// Legacy functions below

void createCuboidMesh(BoundCube& cube, float length, float width, float height) {
    // Centered at origin. Vertex order: each face, 2 triangles per face.
    // Attribute layout: position (3), normal (3), uv (2)
    float x = length / 2.0f, y = height / 2.0f, z = width / 2.0f;

    float vertices[] = {
        // back face (0,0,-1)
        -x, -y, -z,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         x,  y, -z,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
         x, -y, -z,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         x,  y, -z,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        -x, -y, -z,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
        -x,  y, -z,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,

        // front face (0,0,1)
        -x, -y,  z,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         x, -y,  z,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         x,  y,  z,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
         x,  y,  z,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -x,  y,  z,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        -x, -y,  z,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,

        // left face (-1,0,0)
        -x,  y,  z, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -x,  y, -z, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        -x, -y, -z, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -x, -y, -z, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -x, -y,  z, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -x,  y,  z, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

        // right face (1,0,0)
         x,  y,  z,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         x, -y, -z,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         x,  y, -z,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         x, -y, -z,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         x,  y,  z,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         x, -y,  z,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,

         // bottom face (0,-1,0)
         -x, -y, -z,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
          x, -y, -z,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
          x, -y,  z,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
          x, -y,  z,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
         -x, -y,  z,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
         -x, -y, -z,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

         // top face (0,1,0)
         -x,  y, -z,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
          x,  y,  z,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
          x,  y, -z,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
          x,  y,  z,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
         -x,  y, -z,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
         -x,  y,  z,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f
    };

    if (cube.vao != 0) {
        glDeleteVertexArrays(1, &cube.vao);
        glDeleteBuffers(1, &cube.vbo);
    }
    glGenVertexArrays(1, &cube.vao);
    glGenBuffers(1, &cube.vbo);

    glBindVertexArray(cube.vao);
    glBindBuffer(GL_ARRAY_BUFFER, cube.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // pos (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    // normal (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    // uv (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    glBindVertexArray(0);
}

std::vector<BoundCube> scatterBoundCubes(
    int count,
    float sphereRadius,
    float length, float width, float height)
{
    std::vector<BoundCube> cubes;
    std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));

    // Calculate the half-diagonal of the cube to determine if it fits
    float cubeHalfDiagonal = glm::sqrt(length * length + width * width + height * height) / 2.0f;

    // Maximum attempts to place a cube
    const int maxAttempts = 100;

    for (int i = 0; i < count; ++i) {
        bool placed = false;

        for (int attempt = 0; attempt < maxAttempts && !placed; ++attempt) {
            // Generate random position
            std::uniform_real_distribution<float> dist(-sphereRadius, sphereRadius);
            glm::vec3 pos(dist(rng), dist(rng), dist(rng));

            // Check if the cube center is within the sphere
            float distanceFromCenter = glm::length(pos);

            // Check if the entire cube (including corners) fits within the sphere
            if (distanceFromCenter + cubeHalfDiagonal <= sphereRadius) {
                BoundCube cube;
                createCuboidMesh(cube, length, width, height);

                // Set position without rotation
                cube.model = glm::translate(glm::mat4(1.0f), pos);

                // Optional: varied color based on position
                cube.color = glm::vec3(0.2f + 0.6f * float(i) / count, 0.7f, 1.0f - 0.5f * float(i) / count);

                cubes.push_back(cube);
                placed = true;
            }
        }

        // If we couldn't place the cube after max attempts, skip it
        if (!placed) {
            std::cout << "Warning: Could not place cube " << i << " within sphere bounds" << std::endl;
        }
    }

    return cubes;
}

void renderBoundCubesPBR(const std::vector<BoundCube>& cubes, const glm::mat4& view, const glm::mat4& proj, unsigned int pbrShader) {
    glUseProgram(pbrShader);

    // Set once (caller may already set these, but it's safe to set again)
    GLint locProj = glGetUniformLocation(pbrShader, "projection");
    GLint locView = glGetUniformLocation(pbrShader, "view");
    if (locProj != -1) glUniformMatrix4fv(locProj, 1, GL_FALSE, glm::value_ptr(proj));
    if (locView != -1) glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(view));

    // Draw each cube with its own model/normalMatrix
    for (const BoundCube& cube : cubes) {
        glm::mat4 model = cube.model;
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

        GLint locModel = glGetUniformLocation(pbrShader, "model");
        GLint locNormal = glGetUniformLocation(pbrShader, "normalMatrix");
        if (locModel != -1) glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
        if (locNormal != -1) glUniformMatrix3fv(locNormal, 1, GL_FALSE, glm::value_ptr(normalMatrix));

        glBindVertexArray(cube.vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    glBindVertexArray(0);
    glUseProgram(0);
}