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

// Create a small cube greeble mesh
void createGreebleCubeMesh(Greeble& greeble, float size) {
    float h = size / 2.0f;

    std::vector<float> vertices = {
        // back face
        -h, -h, -h,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
         h,  h, -h,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
         h, -h, -h,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f,
         h,  h, -h,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f,
        -h, -h, -h,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f,
        -h,  h, -h,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f,
        // front face
        -h, -h,  h,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
         h, -h,  h,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f,
         h,  h,  h,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
         h,  h,  h,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f,
        -h,  h,  h,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f,
        -h, -h,  h,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f,
        // left face
        -h,  h,  h, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
        -h,  h, -h, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
        -h, -h, -h, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
        -h, -h, -h, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
        -h, -h,  h, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
        -h,  h,  h, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
        // right face
         h,  h,  h,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
         h, -h, -h,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
         h,  h, -h,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f,
         h, -h, -h,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f,
         h,  h,  h,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f,
         h, -h,  h,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f,
         // bottom face
         -h, -h, -h,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
          h, -h, -h,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f,
          h, -h,  h,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
          h, -h,  h,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f,
         -h, -h,  h,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f,
         -h, -h, -h,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f,
         // top face
         -h,  h, -h,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
          h,  h,  h,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
          h,  h, -h,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f,
          h,  h,  h,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f,
         -h,  h, -h,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f,
         -h,  h,  h,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f
    };

    if (greeble.vao != 0) {
        glDeleteVertexArrays(1, &greeble.vao);
        glDeleteBuffers(1, &greeble.vbo);
    }

    glGenVertexArrays(1, &greeble.vao);
    glGenBuffers(1, &greeble.vbo);

    glBindVertexArray(greeble.vao);
    glBindBuffer(GL_ARRAY_BUFFER, greeble.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    glBindVertexArray(0);

    greeble.indexCount = 36; // 6 faces * 2 triangles * 3 vertices
    greeble.type = Greeble::SMALL_CUBE;
}

// Create a flat cylinder greeble mesh
void createGreebleFlatCylinderMesh(Greeble& greeble, float radius, float height, int segments) {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    const float halfHeight = height / 2.0f;

    // Side vertices
    for (int ring = 0; ring <= 1; ++ring) {
        float z = ring == 0 ? -halfHeight : halfHeight;

        for (int i = 0; i <= segments; ++i) {
            float theta = (float)i / (float)segments * 2.0f * glm::pi<float>();
            float x = radius * cos(theta);
            float y = radius * sin(theta);

            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            glm::vec3 normal = glm::normalize(glm::vec3(x, y, 0.0f));
            vertices.push_back(normal.x);
            vertices.push_back(normal.y);
            vertices.push_back(normal.z);

            vertices.push_back((float)i / (float)segments);
            vertices.push_back((float)ring);
        }
    }

    // Side indices
    for (int i = 0; i < segments; ++i) {
        int bottomLeft = i;
        int bottomRight = i + 1;
        int topLeft = i + (segments + 1);
        int topRight = i + 1 + (segments + 1);

        indices.push_back(bottomLeft);
        indices.push_back(topLeft);
        indices.push_back(bottomRight);

        indices.push_back(bottomRight);
        indices.push_back(topLeft);
        indices.push_back(topRight);
    }

    // Front cap
    int frontCenterIndex = vertices.size() / 8;
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(-halfHeight);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(-1.0f);
    vertices.push_back(0.5f);
    vertices.push_back(0.5f);

    int frontCapStart = vertices.size() / 8;
    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / (float)segments * 2.0f * glm::pi<float>();
        float x = radius * cos(theta);
        float y = radius * sin(theta);

        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(-halfHeight);
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
        vertices.push_back(-1.0f);
        vertices.push_back((float)i / (float)segments);
        vertices.push_back(0.0f);
    }

    for (int i = 0; i < segments; ++i) {
        indices.push_back(frontCenterIndex);
        indices.push_back(frontCapStart + i);
        indices.push_back(frontCapStart + i + 1);
    }

    // Back cap
    int backCenterIndex = vertices.size() / 8;
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(halfHeight);
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    vertices.push_back(1.0f);
    vertices.push_back(0.5f);
    vertices.push_back(0.5f);

    int backCapStart = vertices.size() / 8;
    for (int i = 0; i <= segments; ++i) {
        float theta = (float)i / (float)segments * 2.0f * glm::pi<float>();
        float x = radius * cos(theta);
        float y = radius * sin(theta);

        vertices.push_back(x);
        vertices.push_back(y);
        vertices.push_back(halfHeight);
        vertices.push_back(0.0f);
        vertices.push_back(0.0f);
        vertices.push_back(1.0f);
        vertices.push_back((float)i / (float)segments);
        vertices.push_back(1.0f);
    }

    for (int i = 0; i < segments; ++i) {
        indices.push_back(backCenterIndex);
        indices.push_back(backCapStart + i + 1);
        indices.push_back(backCapStart + i);
    }

    if (greeble.vao != 0) {
        glDeleteVertexArrays(1, &greeble.vao);
        glDeleteBuffers(1, &greeble.vbo);
        if (greeble.ebo != 0) {
            glDeleteBuffers(1, &greeble.ebo);
        }
    }

    glGenVertexArrays(1, &greeble.vao);
    glGenBuffers(1, &greeble.vbo);
    glGenBuffers(1, &greeble.ebo);

    glBindVertexArray(greeble.vao);

    glBindBuffer(GL_ARRAY_BUFFER, greeble.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, greeble.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    glBindVertexArray(0);

    greeble.indexCount = indices.size();
    greeble.type = Greeble::FLAT_CYLINDER;
}

// Generate greebles scattered on the surface of a module
// Generate greebles scattered on the surface of a module
std::vector<Greeble> generateGreeblesForModule(
    const StationModule& module,
    int greebleCount,
    unsigned int randomSeed,
    float scaleFactor,
    float scaleProportion)
{
    std::vector<Greeble> greebles;
    std::mt19937 rng(randomSeed);
    std::uniform_real_distribution<float> dist01(0.0f, 1.0f);
    std::uniform_real_distribution<float> distAngle(0.0f, 2.0f * glm::pi<float>());
    std::uniform_real_distribution<float> distLength(0.0f, module.length);

    // Extract the module's transformation to position greebles in world space
    glm::mat4 moduleTransform = module.model;

    for (int i = 0; i < greebleCount; ++i) {
        Greeble greeble;

        // Randomly choose greeble type
        bool isCube = dist01(rng) < 0.5f;

        // Random size variations
        float size = 0.05f + dist01(rng) * 0.15f; // Size between 0.05 and 0.2

        if (isCube) {
            createGreebleCubeMesh(greeble, size);
        }
        else {
            float cylRadius = 0.03f + dist01(rng) * 0.1f;
            float cylHeight = 0.02f + dist01(rng) * 0.05f; // Flat cylinder
            createGreebleFlatCylinderMesh(greeble, cylRadius, cylHeight, 12);
        }

        // Determine if this greeble should be scaled
        float greebleScale = 1.0f;
        if (dist01(rng) < scaleProportion) {
            greebleScale = scaleFactor;
        }
        greeble.scale = greebleScale;

        // Random color variations (orange to red accent colors)
        greeble.color = glm::vec3(
            0.7f + dist01(rng) * 0.3f,  // Red: 0.7-1.0
            0.2f + dist01(rng) * 0.2f,  // Green: 0.2-0.4
            0.05f + dist01(rng) * 0.1f  // Blue: 0.05-0.15
        );

        // Position on cylinder surface
        // Cylinder is centered at origin in local space, aligned along Z-axis
        float theta = distAngle(rng);  // Random angle around cylinder
        float zLocal = distLength(rng) - module.length / 2.0f; // Random position along length

        // Position on surface (slightly above to avoid z-fighting)
        float surfaceOffset = module.radius + size * 0.5f * greebleScale; // Adjust for scale
        glm::vec3 localPos(
            surfaceOffset * cos(theta),
            surfaceOffset * sin(theta),
            zLocal
        );

        // Create local transformation matrix
        glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), localPos);

        // Apply scale
        localTransform = glm::scale(localTransform, glm::vec3(greebleScale));

        // Calculate surface normal at this position (points outward from cylinder)
        glm::vec3 surfaceNormal = glm::normalize(glm::vec3(cos(theta), sin(theta), 0.0f));

        // Align greeble with surface normal
        glm::vec3 up(0.0f, 0.0f, 1.0f);
        glm::vec3 rotAxis = glm::cross(up, surfaceNormal);
        if (glm::length(rotAxis) > 0.001f) {
            float rotAngle = acos(glm::dot(up, surfaceNormal));
            localTransform = glm::rotate(localTransform, rotAngle, glm::normalize(rotAxis));
        }

        // Add random rotation around surface normal for variety
        float randomRot = distAngle(rng);
        localTransform = glm::rotate(localTransform, randomRot, surfaceNormal);

        // Combine with module's world transform
        greeble.model = moduleTransform * localTransform;

        greebles.push_back(greeble);
    }

    return greebles;
}

// Render greebles using PBR shader
void renderGreeblesPBR(
    const std::vector<Greeble>& greebles,
    const glm::mat4& view,
    const glm::mat4& proj,
    unsigned int pbrShader)
{
    glUseProgram(pbrShader);

    GLint locProj = glGetUniformLocation(pbrShader, "projection");
    GLint locView = glGetUniformLocation(pbrShader, "view");
    if (locProj != -1) glUniformMatrix4fv(locProj, 1, GL_FALSE, glm::value_ptr(proj));
    if (locView != -1) glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(view));

    for (const Greeble& greeble : greebles) {
        glm::mat4 model = greeble.model;
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));

        GLint locModel = glGetUniformLocation(pbrShader, "model");
        GLint locNormal = glGetUniformLocation(pbrShader, "normalMatrix");
        if (locModel != -1) glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(model));
        if (locNormal != -1) glUniformMatrix3fv(locNormal, 1, GL_FALSE, glm::value_ptr(normalMatrix));

        glBindVertexArray(greeble.vao);

        if (greeble.type == Greeble::SMALL_CUBE) {
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
        else {
            glDrawElements(GL_TRIANGLES, greeble.indexCount, GL_UNSIGNED_INT, 0);
        }
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

// L-System string generation with branch probability
std::string generateLSystemString(const LSystemParams& params) {
    std::string current = params.axiom;
    std::mt19937 rng(params.randomSeed);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    for (int iter = 0; iter < params.iterations; ++iter) {
        std::string next = "";

        // Track depth for asymmetric branching
        int bracketDepth = 0;

        for (size_t i = 0; i < current.size(); ++i) {
            char symbol = current[i];

            // Track bracket depth to know which generation we're in
            if (symbol == '[') bracketDepth++;
            else if (symbol == ']') bracketDepth--;

            bool replaced = false;

            // Find matching rule
            for (const auto& rule : params.rules) {
                if (rule.symbol == symbol) {
                    // For symbols in branches (depth > 0), apply branch probability
                    float effectiveProbability = rule.probability;

                    // Reduce probability for deeper branches
                    if (bracketDepth > 0 && symbol == 'A') {
                        // Each level of depth reduces the chance of branching
                        effectiveProbability *= std::pow(params.branchProbability, bracketDepth);
                    }

                    if (dist(rng) <= effectiveProbability) {
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

LSystemParams createStandardStationParams() {
    LSystemParams params;
    params.axiom = "A";
    params.iterations = 3;
    params.lengthScale = 0.7f;
    params.radiusScale = 0.75f;
    params.branchAngle = 90.0f;
    params.branchProbability = 0.8f;
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
    params.branchProbability = 0.6f;
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
    params.branchProbability = 1.0f;
    params.randomSeed = static_cast<unsigned>(std::time(nullptr));

    params.rules.push_back({ 'A', "F[+A][-A]", 1.0f });
    params.rules.push_back({ 'F', "F", 1.0f });

    return params;
}

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
    params.branchProbability = 0.7f;
    params.randomSeed = randomSeed;

    if (iterations <= 2) {
        params.rules.push_back({ 'A', "F[+A][-A]", 1.0f });
        params.rules.push_back({ 'F', "F", 1.0f });
    }
    else if (iterations == 3) {
        params.rules.push_back({ 'A', "F[+A][-A][&A][^A]A", 1.0f });
        params.rules.push_back({ 'F', "FF", 0.8f });
    }
    else {
        params.rules.push_back({ 'A', "F[+A][-A][&A][^A][\\A][/A]F", 0.6f });
        params.rules.push_back({ 'A', "F[+A][-A][&A][^A]A", 0.4f });
        params.rules.push_back({ 'F', "FA", 1.0f });
    }

    return params;
}

LSystemParams createCustomStationParams(
    int iterations,
    float lengthScale,
    float radiusScale,
    float branchAngle,
    float branchProbability,
    unsigned int randomSeed)
{
    LSystemParams params;
    params.axiom = "A";
    params.iterations = iterations;
    params.lengthScale = lengthScale;
    params.radiusScale = radiusScale;
    params.branchAngle = branchAngle;
    params.branchProbability = branchProbability;
    params.randomSeed = randomSeed;

    if (iterations <= 2) {
        params.rules.push_back({ 'A', "F[+A][-A]", 1.0f });
        params.rules.push_back({ 'F', "F", 1.0f });
    }
    else if (iterations == 3) {
        params.rules.push_back({ 'A', "F[+A][-A][&A][^A]A", 1.0f });
        params.rules.push_back({ 'F', "FF", 0.8f });
    }
    else {
        params.rules.push_back({ 'A', "F[+A][-A][&A][^A][\\A][/A]F", 0.6f });
        params.rules.push_back({ 'A', "F[+A][-A][&A][^A]A", 0.4f });
        params.rules.push_back({ 'F', "FA", 1.0f });
    }

    return params;
}

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
        glDrawElements(GL_TRIANGLES, module.indexCount, GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glUseProgram(0);
}

// Legacy functions below

void createCuboidMesh(BoundCube& cube, float length, float width, float height) {
    float x = length / 2.0f, y = height / 2.0f, z = width / 2.0f;

    float vertices[] = {
        -x, -y, -z,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
         x,  y, -z,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
         x, -y, -z,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
         x,  y, -z,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
        -x, -y, -z,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
        -x,  y, -z,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,

        -x, -y,  z,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
         x, -y,  z,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
         x,  y,  z,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
         x,  y,  z,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
        -x,  y,  z,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
        -x, -y,  z,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,

        -x,  y,  z, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
        -x,  y, -z, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
        -x, -y, -z, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -x, -y, -z, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
        -x, -y,  z, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
        -x,  y,  z, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

         x,  y,  z,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         x, -y, -z,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         x,  y, -z,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
         x, -y, -z,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
         x,  y,  z,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
         x, -y,  z,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,

         -x, -y, -z,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
          x, -y, -z,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
          x, -y,  z,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
          x, -y,  z,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
         -x, -y,  z,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
         -x, -y, -z,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

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

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
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

    float cubeHalfDiagonal = glm::sqrt(length * length + width * width + height * height) / 2.0f;
    const int maxAttempts = 100;

    for (int i = 0; i < count; ++i) {
        bool placed = false;

        for (int attempt = 0; attempt < maxAttempts && !placed; ++attempt) {
            std::uniform_real_distribution<float> dist(-sphereRadius, sphereRadius);
            glm::vec3 pos(dist(rng), dist(rng), dist(rng));

            float distanceFromCenter = glm::length(pos);

            if (distanceFromCenter + cubeHalfDiagonal <= sphereRadius) {
                BoundCube cube;
                createCuboidMesh(cube, length, width, height);
                cube.model = glm::translate(glm::mat4(1.0f), pos);
                cube.color = glm::vec3(0.2f + 0.6f * float(i) / count, 0.7f, 1.0f - 0.5f * float(i) / count);
                cubes.push_back(cube);
                placed = true;
            }
        }

        if (!placed) {
            std::cout << "Warning: Could not place cube " << i << " within sphere bounds" << std::endl;
        }
    }

    return cubes;
}

void renderBoundCubesPBR(const std::vector<BoundCube>& cubes, const glm::mat4& view, const glm::mat4& proj, unsigned int pbrShader) {
    glUseProgram(pbrShader);

    GLint locProj = glGetUniformLocation(pbrShader, "projection");
    GLint locView = glGetUniformLocation(pbrShader, "view");
    if (locProj != -1) glUniformMatrix4fv(locProj, 1, GL_FALSE, glm::value_ptr(proj));
    if (locView != -1) glUniformMatrix4fv(locView, 1, GL_FALSE, glm::value_ptr(view));

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