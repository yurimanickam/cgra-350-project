// lava_lamp.cpp
#define GLM_ENABLE_EXPERIMENTAL
#include "lava_lamp.hpp"
#include <glm/gtc/noise.hpp>
#include <glm/gtc/random.hpp>
#include <algorithm>
#include <cmath>
#include <glm/gtx/extended_min_max.hpp>
#include <iostream>

using namespace glm;

// small helper
float distance_squared(const glm::vec3& a, const glm::vec3& b) {
	glm::vec3 diff = a - b;
	return glm::dot(diff, diff);
}

// (marching cubes tables omitted; not used for shader-based rendering here)
static const int edgeTable[256] = { 0 };
static const int triTable[256][16] = { { -1 } };

LavaBlob::LavaBlob(glm::vec3 pos, float r)
	: position(pos)
	, velocity(0.0f)
	, radius(r)
	, temperature(25.0f)
	, blobbiness(-0.5f)
	, color(1.0f, 0.3f, 0.1f) // Orange-red
{
}

LavaLamp::LavaLamp()
	: m_rng(std::random_device{}())
	, m_randomDist(-1.0f, 1.0f)
	, m_threshold(0.2f)
{
	// Defaults tuned to match mesh geometry (important!)
	m_radius = 1.8f;
	m_height = 10.0f;
	m_baseHeight = 1.7f;
	m_ambientTemp = 20.0f;
	m_heaterTemp = 80.0f;
}

LavaLamp::~LavaLamp() {}

void LavaLamp::initialize(int numBlobs) {
	m_blobs.clear();

	for (int i = 0; i < numBlobs; ++i) {
		glm::vec3 pos;
		// Start blobs somewhat inside the lamp interior
		pos.x = m_randomDist(m_rng) * m_radius * 0.6f;
		pos.y = m_baseHeight + 0.4f + i * 1.0f; // start above glass bottom
		pos.z = m_randomDist(m_rng) * m_radius * 0.6f;

		float radius = 0.6f + (m_randomDist(m_rng) + 1.0f) * 0.2f; // around 0.6..1.0

		LavaBlob blob(pos, radius);
		blob.temperature = m_ambientTemp + (m_randomDist(m_rng) * 5.0f);
		blob.blobbiness = -0.1f - (m_randomDist(m_rng) * 0.2f);
		blob.color = glm::vec3(
			glm::clamp(0.9f + m_randomDist(m_rng) * 0.1f, 0.0f, 1.0f),
			glm::clamp(0.3f + m_randomDist(m_rng) * 0.2f, 0.0f, 1.0f),
			glm::clamp(0.0f + m_randomDist(m_rng) * 0.05f, 0.0f, 1.0f)
		);

		m_blobs.push_back(blob);
	}
}

void LavaLamp::update(float deltaTime) {
	if (deltaTime <= 0.0f) return;

	// Update each blob physics
	for (auto& blob : m_blobs) {
		updateBlobPhysics(blob, deltaTime);
	}

	// interactions & topological operations
	handleBlobInteractions();
	mergeBlobsIfClose();
	splitLargeBlobs();
}

void LavaLamp::updateBlobPhysics(LavaBlob& blob, float dt) {
	// Heat diffusion - GRADIENT based (not step function)
	// Blobs closer to bottom heat faster
	float distFromHeater = glm::max(0.0f, blob.position.y - m_baseHeight);
	float heatZone = 2.0f; // heating influence extends 2 units up
	float heatFactor = glm::clamp(1.0f - (distFromHeater / heatZone), 0.0f, 1.0f);

	float targetTemp = m_ambientTemp + heatFactor * (m_heaterTemp - m_ambientTemp);
	blob.temperature += (targetTemp - blob.temperature) * 0.8f * dt; // fast diffusion

	// CORRECTED BUOYANCY: Archimedes principle
	// Hot blobs are less dense -> float upward
	float tempFactor = glm::clamp((blob.temperature - m_ambientTemp) / (m_heaterTemp - m_ambientTemp), 0.0f, 1.0f);

	// Density decreases with temperature (thermal expansion)
	float densityRatio = 1.0f - 0.15f * tempFactor; // hot blobs are 15% less dense

	// Buoyant force = (ambient_density - blob_density) * volume * gravity
	// Simplified: use density ratio directly
	float buoyancy = (1.0f - densityRatio) * (-m_gravity) * 12.0f; // strong effect

	// Basic forces
	glm::vec3 acceleration(0.0f);
	acceleration.y += m_gravity * densityRatio; // gravity scales with density
	acceleration.y += buoyancy;

	// DRAG FORCE (not viscosity damping)
	// Stokes drag: F = 6π * viscosity * radius * velocity
	// Simplified: drag proportional to velocity and blob size
	float fluidViscosity = 0.3f * (1.0f + 0.5f * tempFactor); // less viscous when hot
	glm::vec3 drag = -blob.velocity * (fluidViscosity * blob.radius * 2.0f);
	acceleration += drag;

	// Turbulence (curl noise would be better, but Perlin is fine)
	float noiseScale = 0.3f;
	float time = glfwGetTime() * 0.5f; // use global time for animation
	glm::vec3 noiseVec(
		glm::perlin(blob.position * noiseScale + glm::vec3(time, 0.0f, 0.0f)),
		glm::perlin(blob.position * noiseScale + glm::vec3(0.0f, time, 100.0f)),
		glm::perlin(blob.position * noiseScale + glm::vec3(200.0f, 0.0f, time))
	);
	acceleration += noiseVec * 1.2f;

	// Integrate (semi-implicit Euler for stability)
	blob.velocity += acceleration * dt;
	blob.velocity *= 0.98f; // gentle global damping for stability
	blob.position += blob.velocity * dt;

	// Boundary handling
	applyBoundaryConditions(blob);
}

void LavaLamp::applyBoundaryConditions(LavaBlob& blob) {
	// Cylindrical boundary (x,z) and caps (y)
	float distFromCenter = sqrt(blob.position.x * blob.position.x + blob.position.z * blob.position.z);

	if (distFromCenter + blob.radius > m_radius) {
		// push back inside
		if (distFromCenter > 1e-6f) {
			glm::vec2 xz(blob.position.x, blob.position.z);
			xz = glm::normalize(xz) * (m_radius - blob.radius - 1e-3f);
			blob.position.x = xz.x;
			blob.position.z = xz.y;
		}
		else {
			// if exactly center, nudge randomly
			blob.position.x = (m_randomDist(m_rng)) * (m_radius - blob.radius - 1e-3f);
			blob.position.z = (m_randomDist(m_rng)) * (m_radius - blob.radius - 1e-3f);
		}
		blob.velocity *= 0.6f;
	}

	// bottom: clamp to m_baseHeight (glass bottom in mesh)
	if (blob.position.y - blob.radius < m_baseHeight) {
		blob.position.y = m_baseHeight + blob.radius;
		blob.velocity.y = fabs(blob.velocity.y) * 0.4f;
	}

	// top
	if (blob.position.y + blob.radius > m_height) {
		blob.position.y = m_height - blob.radius;
		blob.velocity.y = -fabs(blob.velocity.y) * 0.4f;
	}
}

void LavaLamp::handleBlobInteractions() {
	// soft collisions / repulsion
	for (size_t i = 0; i < m_blobs.size(); ++i) {
		for (size_t j = i + 1; j < m_blobs.size(); ++j) {
			glm::vec3 diff = m_blobs[i].position - m_blobs[j].position;
			float d = glm::length(diff);
			float minDist = (m_blobs[i].radius + m_blobs[j].radius) * 0.9f;

			if (d > 1e-5f && d < minDist) {
				glm::vec3 dir = diff / d;
				float overlap = minDist - d;
				glm::vec3 force = dir * (overlap * 0.05f);
				m_blobs[i].velocity += force * 0.5f;
				m_blobs[j].velocity -= force * 0.5f;
			}
		}
	}
}

float LavaLamp::computeDensityField(const glm::vec3& point) const {
	float density = 0.0f;
	for (const auto& blob : m_blobs) {
		float r2 = distance_squared(point, blob.position);
		float R2 = blob.radius * blob.radius;
		if (R2 <= 0.0f) continue;

		float a = -blob.blobbiness / R2;
		// clamp to avoid blowups
		a = glm::max(a, 0.0001f);
		float contribution = exp(-a * r2 + blob.blobbiness);
		density += contribution;
	}
	return density;
}

glm::vec3 LavaLamp::computeDensityGradient(const glm::vec3& point) const {
	glm::vec3 gradient(0.0f);
	for (const auto& blob : m_blobs) {
		glm::vec3 diff = point - blob.position;
		float r2 = glm::dot(diff, diff);
		float R2 = blob.radius * blob.radius;
		if (R2 <= 0.0f) continue;

		float a = -blob.blobbiness / R2;
		a = glm::max(a, 0.0001f);
		float contribution = exp(-a * r2 + blob.blobbiness);
		gradient += -2.0f * a * diff * contribution;
	}
	return gradient;
}

void LavaLamp::mergeBlobsIfClose() {
	const float baseMergeDist = 0.4f;
	for (size_t i = 0; i < m_blobs.size(); ++i) {
		// Calculate blobbiness factor for blob i
		float blobbinessFactor = glm::clamp(-m_blobs[i].blobbiness / 0.5f, 0.5f, 2.0f);
		float mergeDist = baseMergeDist * blobbinessFactor;

		for (size_t j = i + 1; j < m_blobs.size(); ++j) {
			float dist = glm::distance(m_blobs[i].position, m_blobs[j].position);
			float combinedRadius = m_blobs[i].radius + m_blobs[j].radius;

			// Merge threshold scales with blob sizes
			if (dist < mergeDist * combinedRadius && dist > 1e-5f) {
				// conserve approximate volume (radius^3)
				float vol1 = pow(m_blobs[i].radius, 3.0f);
				float vol2 = pow(m_blobs[j].radius, 3.0f);
				float newRadius = pow(vol1 + vol2, 1.0f / 3.0f);

				float w1 = vol1 / (vol1 + vol2);
				float w2 = vol2 / (vol1 + vol2);

				m_blobs[i].position = m_blobs[i].position * w1 + m_blobs[j].position * w2;
				m_blobs[i].velocity = m_blobs[i].velocity * w1 + m_blobs[j].velocity * w2;
				m_blobs[i].radius = newRadius;
				m_blobs[i].temperature = m_blobs[i].temperature * w1 + m_blobs[j].temperature * w2;

				m_blobs.erase(m_blobs.begin() + j);
				--j;
			}
		}
	}
}

void LavaLamp::splitLargeBlobs() {
	// Hotter blobs have lower surface tension -> split easier
	const float coldMaxRadius = 1.8f;  // Let cold blobs get bigger
	const float hotMaxRadius = 1.3f;   // Hot blobs still split, but not too aggressively

	size_t originalSize = m_blobs.size();

	for (size_t i = 0; i < originalSize; ++i) {
		float tempFactor = glm::clamp(
			(m_blobs[i].temperature - m_ambientTemp) / (m_heaterTemp - m_ambientTemp),
			0.0f, 1.0f
		);

		// Interpolate threshold based on temperature
		float maxRadius = glm::mix(coldMaxRadius, hotMaxRadius, tempFactor);

		if (m_blobs[i].radius > maxRadius) {
			float childVolumeFraction = 0.2f + m_randomDist(m_rng) * 0.1f; // 0.2-0.3
			float parentVol = pow(m_blobs[i].radius, 3.0f);
			float childVol = parentVol * childVolumeFraction;

			float childRadius = pow(childVol, 1.0f / 3.0f);
			float parentRadius = pow(parentVol - childVol, 1.0f / 3.0f);

			// Random offset direction (not just upward)
			glm::vec3 splitDir = glm::normalize(glm::vec3(
				m_randomDist(m_rng),
				0.5f + 0.5f * tempFactor, // bias upward when hot
				m_randomDist(m_rng)
			));

			// Offset distance based on combined radii
			float separation = (childRadius + parentRadius) * 1.2f;
			glm::vec3 offset = splitDir * separation;

			// Create child blob
			LavaBlob child = m_blobs[i];
			child.radius = childRadius;
			child.position = m_blobs[i].position + offset;
			child.velocity = m_blobs[i].velocity + splitDir * (0.3f + 0.5f * tempFactor);
			// Inherit slightly cooler temp (surface was cooler)
			child.temperature = m_blobs[i].temperature - 2.0f;

			// Update parent
			m_blobs[i].radius = parentRadius;
			m_blobs[i].velocity -= splitDir * (0.1f * childVolumeFraction); // recoil

			m_blobs.push_back(child);
		}
	}
}

void LavaLamp::addBlob(const glm::vec3& position, float radius) {
	LavaBlob blob(position, radius);
	blob.temperature = m_ambientTemp;
	blob.blobbiness = -0.5f;
	m_blobs.push_back(blob);
}

void LavaLamp::removeBlob() {
	if (!m_blobs.empty())
		m_blobs.pop_back();
}

cgra::gl_mesh LavaLamp::getMesh() {
	// Not used in shader-based raymarch rendering; return empty mesh
	cgra::mesh_builder builder;
	return builder.build();
}

std::vector<glm::vec4> LavaLamp::getBlobPositions() const {
	std::vector<glm::vec4> out;
	out.reserve(m_blobs.size());
	for (const auto& b : m_blobs)
		out.push_back(glm::vec4(b.position, 1.0f));
	// Ensure at least one element to avoid UB when passing pointer to glUniform (caller clamps count)
	if (out.empty())
		out.push_back(glm::vec4(0.0f));
	return out;
}

std::vector<float> LavaLamp::getBlobRadii() const {
	std::vector<float> out;
	out.reserve(m_blobs.size());
	for (const auto& b : m_blobs)
		out.push_back(b.radius);
	if (out.empty())
		out.push_back(0.0f);
	return out;
}

std::vector<float> LavaLamp::getBlobBlobbiness() const {
	std::vector<float> out;
	out.reserve(m_blobs.size());
	for (const auto& b : m_blobs)
		out.push_back(b.blobbiness);
	if (out.empty())
		out.push_back(0.0f);
	return out;
}

std::vector<glm::vec3> LavaLamp::getBlobColors() const {
	std::vector<glm::vec3> out;
	out.reserve(m_blobs.size());
	for (const auto& b : m_blobs)
		out.push_back(b.color);
	if (out.empty())
		out.push_back(glm::vec3(1.0f, 0.3f, 0.0f));
	return out;
}
