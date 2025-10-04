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
	// Heat diffusion - blobs near bottom get hotter
	float targetTemp = m_ambientTemp;
	if ((blob.position.y - blob.radius) < (m_baseHeight + 1.0f)) {
		targetTemp = m_heaterTemp;
	}
	blob.temperature += (targetTemp - blob.temperature) * m_heatDiffusion * dt;

	// Buoyancy: positive upward when hotter
	float tempDiff = blob.temperature - m_ambientTemp;
	float buoyancy = 0.0f;
	if (tempDiff > 0.0f) {
		float denom = glm::max(1.0f, (m_heaterTemp - m_ambientTemp));
		float normalized = tempDiff / denom;
		buoyancy = normalized * (-m_gravity) * 1.8f;
	}

	// Basic forces
	glm::vec3 acceleration(0.0f);
	acceleration.y += m_gravity;  // gravity (negative)
	acceleration.y += buoyancy;   // buoyant upward

	// Per-blob temperature-dependent viscosity:
	// tempFactor ranges 0..1 (ambient..heater). Hotter => less viscous.
	float tempFactor = glm::clamp((blob.temperature - m_ambientTemp) / (m_heaterTemp - m_ambientTemp), 0.0f, 1.0f);
	// base viscosity 0.5 at ambient; when hot reduce down to ~0.15 (adjustable)
	float viscosity = 0.5f * (1.0f - 0.7f * tempFactor);
	acceleration -= blob.velocity * viscosity;

	// Add some turbulence (Perlin noise)
	float noiseScale = 0.25f;
	glm::vec3 noiseVec(
		glm::perlin(blob.position * noiseScale + glm::vec3(0.0f, dt, 0.0f)),
		glm::perlin(blob.position * noiseScale + glm::vec3(100.0f, dt, 0.0f)),
		glm::perlin(blob.position * noiseScale + glm::vec3(200.0f, dt, 0.0f))
	);
	acceleration += noiseVec * 0.5f;

	// integrate
	blob.velocity += acceleration * dt;
	blob.position += blob.velocity * dt;

	// boundary handling
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
				glm::vec3 force = dir * (overlap * 0.12f);
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
	const float mergeDistance = 0.25f;

	for (size_t i = 0; i < m_blobs.size(); ++i) {
		for (size_t j = i + 1; j < m_blobs.size(); ++j) {
			float dist = glm::distance(m_blobs[i].position, m_blobs[j].position);
			if (dist < mergeDistance && dist > 1e-5f) {
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
	const float baseMaxRadius = 1.5f; // base threshold (when cold)
	size_t originalSize = m_blobs.size();

	for (size_t i = 0; i < originalSize; ++i) {
		// Compute temperature factor [0..1]
		float tempFactor = glm::clamp((m_blobs[i].temperature - m_ambientTemp) / (m_heaterTemp - m_ambientTemp), 0.0f, 1.0f);

		// Hotter blobs should split at smaller radii -> effective threshold decreases with temp
		// e.g. at tempFactor=0 -> 1.2 * baseMaxRadius, at tempFactor=1 -> 0.8 * baseMaxRadius
		float effectiveMaxRadius = baseMaxRadius * (1.2f - 0.4f * tempFactor);

		if (m_blobs[i].radius > effectiveMaxRadius) {
			// child radius ratio
			float newRadius = m_blobs[i].radius * 0.7f;

			// vertical offset is larger for hotter blobs so they separate upwards more
			glm::vec3 offset(
				m_randomDist(m_rng) * 0.4f,
				glm::abs(m_randomDist(m_rng)) * (0.4f + 0.8f * tempFactor), // stronger upward kick when hot
				m_randomDist(m_rng) * 0.4f
			);

			LavaBlob newBlob = m_blobs[i];
			newBlob.radius = newRadius;
			newBlob.position += offset;
			// give a slightly stronger velocity kick to separate when hot
			newBlob.velocity += offset * (0.15f + 0.25f * tempFactor);

			m_blobs[i].radius = newRadius;
			m_blobs[i].position -= offset;
			m_blobs[i].velocity -= offset * (0.15f + 0.25f * tempFactor);

			m_blobs.push_back(newBlob);
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
