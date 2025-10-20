#include "objloader.hpp"

// std
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>

// glm
#include <glm/gtc/matrix_transform.hpp>

namespace cgra {

    OBJLoader::OBJLoader() : m_originalVertexCount(0), m_optimizedVertexCount(0) {
        // Reserve some initial capacity to reduce allocations
        m_positions.reserve(10000);
        m_normals.reserve(10000);
        m_texCoords.reserve(10000);
        m_vertices.reserve(10000);
    }

    OBJLoader::~OBJLoader() {
        clear();
    }

    bool OBJLoader::loadFromFile(const std::string& filename) {
        auto start = std::chrono::high_resolution_clock::now();

        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open OBJ file: " << filename << std::endl;
            return false;
        }

        std::cout << "Loading OBJ file: " << filename << std::endl;

        clear(); // Clear any existing data

        // Extract path for loading related files like MTL
        size_t last_slash = filename.find_last_of("/\\");
        m_basePath = (last_slash == std::string::npos) ? "" : filename.substr(0, last_slash + 1);


        std::string line;
        size_t lineNumber = 0;

        m_faceGroups.push_back({ "default", {} }); // Start with a default material group

        while (std::getline(file, line)) {
            lineNumber++;

            // Skip empty lines and comments
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // Trim whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);

            if (line.empty()) continue;

            try {
                // Parse based on the first token
                if (line.substr(0, 7) == "mtllib ") {
                    parseMtllib(line);
                }
                else if (line.substr(0, 7) == "usemtl ") {
                    parseUsemtl(line);
                }
                else if (line.substr(0, 2) == "v ") {
                    parseVertexPosition(line);
                }
                else if (line.substr(0, 3) == "vn ") {
                    parseVertexNormal(line);
                }
                else if (line.substr(0, 3) == "vt ") {
                    parseTextureCoord(line);
                }
                else if (line.substr(0, 2) == "f ") {
                    parseFace(line);
                }
                // Ignore other tokens (g, o, s, etc.)
            }
            catch (const std::exception& e) {
                std::cerr << "Error parsing line " << lineNumber << ": " << e.what() << std::endl;
                std::cerr << "Line content: " << line << std::endl;
                // Continue parsing instead of failing completely
            }
        }

        file.close();

        // Post-processing
        if (m_normals.empty()) {
            std::cout << "No normals found, generating them..." << std::endl;
            generateNormalsIfMissing();
        }

        if (m_texCoords.empty()) {
            std::cout << "No texture coordinates found, generating default ones..." << std::endl;
            generateDefaultTexCoords();
        }

        optimizeMesh();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "OBJ loading completed in " << duration.count() << "ms" << std::endl;
        printStatistics();

        return true;
    }

    void OBJLoader::parseVertexPosition(const std::string& line) {
        std::istringstream iss(line.substr(2)); // Skip "v "
        glm::vec3 pos;

        if (!(iss >> pos.x >> pos.y >> pos.z)) {
            throw std::runtime_error("Invalid vertex position format");
        }

        m_positions.push_back(pos);
    }

    void OBJLoader::parseVertexNormal(const std::string& line) {
        std::istringstream iss(line.substr(3)); // Skip "vn "
        glm::vec3 normal;

        if (!(iss >> normal.x >> normal.y >> normal.z)) {
            throw std::runtime_error("Invalid vertex normal format");
        }

        // Normalize the normal vector
        float length = glm::length(normal);
        if (length > 0.0f) {
            normal /= length;
        }

        m_normals.push_back(normal);
    }

    void OBJLoader::parseTextureCoord(const std::string& line) {
        std::istringstream iss(line.substr(3)); // Skip "vt "
        glm::vec2 texCoord;

        if (!(iss >> texCoord.x >> texCoord.y)) {
            throw std::runtime_error("Invalid texture coordinate format");
        }

        m_texCoords.push_back(texCoord);
    }

    void OBJLoader::parseMtllib(const std::string& line) {
        std::istringstream iss(line.substr(7));
        std::string mtl_filename;
        iss >> mtl_filename;
        processMtlFile(m_basePath + mtl_filename);
    }

    void OBJLoader::parseUsemtl(const std::string& line) {
        std::istringstream iss(line.substr(7));
        std::string material_name;
        iss >> material_name;

        if (m_currentMaterial != material_name) {
            m_currentMaterial = material_name;

            // Check if this material group already exists
            bool found = false;
            for (const auto& group : m_faceGroups) {
                if (group.material_name == material_name) {
                    found = true;
                    break;
                }
            }
            // If not, create a new group for it
            if (!found) {
                m_faceGroups.push_back({ material_name, {} });
            }
        }
    }

    void OBJLoader::processMtlFile(const std::string& mtl_filename) {
        std::ifstream file(mtl_filename);
        if (!file.is_open()) {
            std::cerr << "Error: Could not open MTL file: " << mtl_filename << std::endl;
            return;
        }
        std::cout << "Loading MTL file: " << mtl_filename << std::endl;

        std::string line;
        while (std::getline(file, line)) {
            // Trim and skip comments/empty lines
            if (line.empty() || line[0] == '#') continue;
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);
            if (line.empty()) continue;

            std::istringstream iss(line);
            std::string token;
            iss >> token;

            if (token == "newmtl") {
                std::string material_name;
                iss >> material_name;
                if (std::find(m_materialNames.begin(), m_materialNames.end(), material_name) == m_materialNames.end()) {
                    m_materialNames.push_back(material_name);
                }
            }
        }
    }


    void OBJLoader::parseFace(const std::string& line) {
        std::istringstream iss(line.substr(2)); // Skip "f "
        std::vector<unsigned int> faceIndices;
        std::string vertexData;

        while (iss >> vertexData) {
            std::istringstream vertexStream(vertexData);
            std::string indexStr;
            std::vector<int> indices;

            // Parse vertex/texture/normal format (e.g., "1/2/3" or "1//3" or "1")
            while (std::getline(vertexStream, indexStr, '/')) {
                if (indexStr.empty()) {
                    indices.push_back(-1); // Missing index
                }
                else {
                    indices.push_back(std::stoi(indexStr));
                }
            }

            // Ensure we have at least position index
            if (indices.empty()) {
                throw std::runtime_error("Invalid face vertex format");
            }

            // Convert to 0-based indexing and handle negative indices
            int posIndex = indices[0];
            if (posIndex < 0) posIndex = static_cast<int>(m_positions.size()) + posIndex + 1;
            else posIndex -= 1;

            int texIndex = -1;
            if (indices.size() > 1 && indices[1] != -1) {
                texIndex = indices[1];
                if (texIndex < 0) texIndex = static_cast<int>(m_texCoords.size()) + texIndex + 1;
                else texIndex -= 1;
            }

            int normalIndex = -1;
            if (indices.size() > 2 && indices[2] != -1) {
                normalIndex = indices[2];
                if (normalIndex < 0) normalIndex = static_cast<int>(m_normals.size()) + normalIndex + 1;
                else normalIndex -= 1;
            }

            // Validate indices
            if (posIndex < 0 || posIndex >= static_cast<int>(m_positions.size())) {
                throw std::runtime_error("Position index out of range");
            }

            // Create vertex
            ParsedVertex vertex;
            vertex.position = m_positions[posIndex];

            if (normalIndex >= 0 && normalIndex < static_cast<int>(m_normals.size())) {
                vertex.normal = m_normals[normalIndex];
            }
            else {
                vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f); // Default normal
            }

            if (texIndex >= 0 && texIndex < static_cast<int>(m_texCoords.size())) {
                vertex.texCoord = m_texCoords[texIndex];
            }
            else {
                vertex.texCoord = glm::vec2(0.0f, 0.0f); // Default tex coord
            }

            unsigned int vertexIndex = addVertex(vertex);
            faceIndices.push_back(vertexIndex);
        }

        // Triangulate the face (assumes convex faces) and add to current material group
        if (faceIndices.size() >= 3) {
            // Find the current group
            FaceGroup* currentGroup = nullptr;
            for (auto& group : m_faceGroups) {
                if (group.material_name == m_currentMaterial) {
                    currentGroup = &group;
                    break;
                }
            }
            if (!currentGroup) { // Should not happen if usemtl is handled correctly
                m_faceGroups.push_back({ m_currentMaterial, {} });
                currentGroup = &m_faceGroups.back();
            }

            for (size_t i = 1; i < faceIndices.size() - 1; i++) {
                currentGroup->indices.push_back(faceIndices[0]);
                currentGroup->indices.push_back(faceIndices[i]);
                currentGroup->indices.push_back(faceIndices[i + 1]);
            }
        }
    }

    unsigned int OBJLoader::addVertex(const ParsedVertex& vertex) {
        auto it = m_vertexMap.find(vertex);
        if (it != m_vertexMap.end()) {
            return it->second; // Return existing vertex index
        }

        // Add new vertex
        unsigned int index = static_cast<unsigned int>(m_vertices.size());
        m_vertices.push_back({ vertex.position, vertex.normal, vertex.texCoord });
        m_vertexMap[vertex] = index;

        return index;
    }

    void OBJLoader::generateNormalsIfMissing() {
        if (!m_normals.empty()) return;

        // Create normals array with same size as positions
        m_normals.resize(m_positions.size(), glm::vec3(0.0f));

        // Calculate face normals and accumulate to vertex normals
        for (const auto& group : m_faceGroups) {
            for (size_t i = 0; i < group.indices.size(); i += 3) {
                unsigned int i0 = group.indices[i];
                unsigned int i1 = group.indices[i + 1];
                unsigned int i2 = group.indices[i + 2];

                if (i0 >= m_vertices.size() || i1 >= m_vertices.size() || i2 >= m_vertices.size()) {
                    continue; // Skip invalid triangles
                }

                glm::vec3 v0 = m_vertices[i0].pos;
                glm::vec3 v1 = m_vertices[i1].pos;
                glm::vec3 v2 = m_vertices[i2].pos;

                glm::vec3 faceNormal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

                // Accumulate face normal to vertex normals
                m_vertices[i0].norm += faceNormal;
                m_vertices[i1].norm += faceNormal;
                m_vertices[i2].norm += faceNormal;
            }
        }

        // Normalize all vertex normals
        for (auto& vertex : m_vertices) {
            if (glm::dot(vertex.norm, vertex.norm) > 0.0f) {
                vertex.norm = glm::normalize(vertex.norm);
            }
            else {
                vertex.norm = glm::vec3(0.0f, 1.0f, 0.0f); // Default up vector
            }
        }
    }

    void OBJLoader::generateDefaultTexCoords() {
        if (!m_texCoords.empty()) return;

        // Generate simple spherical texture coordinates
        for (const auto& pos : m_positions) {
            float u = 0.5f + (std::atan2(pos.z, pos.x) / (2.0f * glm::pi<float>()));
            float v = 0.5f - (std::asin(pos.y / glm::length(pos)) / glm::pi<float>());
            m_texCoords.push_back(glm::vec2(u, v));
        }
    }

    void OBJLoader::optimizeMesh() {
        m_originalVertexCount = m_vertices.size();
        // Vertex deduplication is already handled in addVertex()
        m_optimizedVertexCount = m_vertices.size();
    }

    void OBJLoader::printStatistics() const {
        std::cout << "=== OBJ Loading Statistics ===" << std::endl;
        std::cout << "Positions: " << m_positions.size() << std::endl;
        std::cout << "Normals: " << m_normals.size() << std::endl;
        std::cout << "Texture Coordinates: " << m_texCoords.size() << std::endl;
        std::cout << "Triangles: " << getTriangleCount() << std::endl;
        std::cout << "Final Vertices: " << getVertexCount() << std::endl;
        std::cout << "Materials Found: " << m_materialNames.size() << std::endl;
        std::cout << "Vertex Deduplication Ratio: " <<
            (m_originalVertexCount > 0 ?
                (1.0f - float(m_optimizedVertexCount) / float(m_originalVertexCount)) * 100.0f : 0.0f)
            << "%" << std::endl;
        std::cout << "===============================" << std::endl;
    }

    size_t OBJLoader::getTriangleCount() const {
        size_t count = 0;
        for (const auto& group : m_faceGroups) {
            count += group.indices.size();
        }
        return count / 3;
    }

    multi_mesh_model OBJLoader::buildMultiMeshModel() const {
        multi_mesh_model model;
        model.material_names = m_materialNames;

        mesh_builder shared_vertex_builder;
        for (const auto& vertex : m_vertices) {
            shared_vertex_builder.push_vertex(vertex);
        }

        for (const auto& group : m_faceGroups) {
            if (group.indices.empty()) {
                continue;
            }

            mesh_builder group_builder = shared_vertex_builder;
            for (unsigned int index : group.indices) {
                group_builder.push_index(index);
            }

            mesh_group mg;
            mg.material_name = group.material_name;
            mg.mesh = group_builder.build();
            model.mesh_groups.push_back(mg);
        }

        return model;
    }


    void OBJLoader::clear() {
        m_positions.clear();
        m_normals.clear();
        m_texCoords.clear();
        m_vertices.clear();
        m_vertexMap.clear();
        m_faceGroups.clear();
        m_materialNames.clear();
        m_currentMaterial = "";
        m_basePath = "";
        m_originalVertexCount = 0;
        m_optimizedVertexCount = 0;
    }

    // Convenience function
    multi_mesh_model load_multi_mesh_model(const std::string& filename) {
        OBJLoader loader;
        if (loader.loadFromFile(filename)) {
            return loader.buildMultiMeshModel();
        }
        else {
            std::cerr << "Failed to load OBJ file: " << filename << std::endl;
            return multi_mesh_model(); // Return empty model
        }
    }
}