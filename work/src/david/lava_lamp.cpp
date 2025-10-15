// lava_lamp.cpp
#define GLM_ENABLE_EXPERIMENTAL
#include "lava_lamp.hpp"
#include <glm/gtc/noise.hpp>
#include <glm/gtc/random.hpp>
#include <algorithm>
#include <cmath>
#include <glm/gtx/extended_min_max.hpp>
#include <iostream>
#include <glm/gtc/constants.hpp>

// project
#include "cgra/cgra_shader.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>

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
		float angle = (2.0f * glm::pi<float>() * i) / numBlobs;
		float radialDist = 0.3f + m_randomDist(m_rng) * 0.2f;

		glm::vec3 pos;
		pos.x = cos(angle) * radialDist * m_radius;
		pos.z = sin(angle) * radialDist * m_radius;
		pos.y = m_baseHeight + 1.0f + (float(i) / numBlobs) * 3.0f;

		float radius = 0.5f + (m_randomDist(m_rng) + 1.0f) * 0.15f;

		LavaBlob blob(pos, radius);
		blob.temperature = m_ambientTemp + m_randomDist(m_rng) * 10.0f;
		blob.blobbiness = -0.15f - (m_randomDist(m_rng) * 0.15f);
		blob.color = glm::vec3(
			glm::clamp(0.9f + m_randomDist(m_rng) * 0.1f, 0.0f, 1.0f),
			glm::clamp(0.3f + m_randomDist(m_rng) * 0.2f, 0.0f, 1.0f),
			glm::clamp(0.0f + m_randomDist(m_rng) * 0.05f, 0.0f, 1.0f)
		);

		// Initialize spring parameters
		blob.heatPhase = float(i) / numBlobs; // Stagger phases
		blob.cycleSpeed = 0.8f + m_randomDist(m_rng) * 0.4f; // Vary speed
		blob.anchorStrength = 1.0f;
		blob.anchorPoint = pos; // Start at current position
		blob.velocity = glm::vec3(0.0f);

		m_blobs.push_back(blob);
	}
}

void LavaLamp::update(float deltaTime) {
	if (deltaTime <= 0.0f) return;

	// Update anchor points (drift over time)
	updateAnchorPoints(deltaTime);

	// Update each blob with repulsion forces
	for (size_t i = 0; i < m_blobs.size(); ++i) {
		LavaBlob& blob = m_blobs[i];

		// Calculate actual blob temperature based on position (heat rises from bottom)
		float distFromBottom = blob.position.y - m_baseHeight;
		float heightFrac = glm::clamp(distFromBottom / (m_height - m_baseHeight), 0.0f, 1.0f);

		// Temperature gradient: hot at bottom (near heater), cool at top
		float heatZoneHeight = 2.0f; // Bottom 2 units are heated
		float heatZoneFactor = glm::clamp(1.0f - (distFromBottom / heatZoneHeight), 0.0f, 1.0f);

		// Blob heats up at bottom, cools at top
		float targetTemp = m_ambientTemp + heatZoneFactor * (m_heaterTemp - m_ambientTemp);
		blob.temperature += (targetTemp - blob.temperature) * 20.0f * deltaTime; // INCREASED for responsiveness

		float tempFactor = glm::clamp(
			(blob.temperature - m_ambientTemp) / glm::max(1.0f, m_heaterTemp - m_ambientTemp),
			0.0f, 1.0f
		);

		// When hot (tempFactor > 0.5): speed up rising phase, slow down falling phase
		// When cold (tempFactor < 0.5): slow down rising phase, speed up falling phase
		float baseCycleSpeed = blob.cycleSpeed * 0.1f;

		// Determine if currently rising (phase 0-0.5) or falling (phase 0.5-1.0)
		float currentPhase = fmod(blob.heatPhase, 1.0f);
		bool isRising = currentPhase < 0.5f;

		float phaseSpeed;
		if (isRising) {
			// Rising phase: faster when hot
			phaseSpeed = baseCycleSpeed * (0.5f + tempFactor * 1.5f); // 0.5x to 2x speed
		}
		else {
			// Falling phase: faster when cold
			phaseSpeed = baseCycleSpeed * (2.0f - tempFactor * 1.5f); // 2x to 0.5x speed
		}

		blob.heatPhase += phaseSpeed * deltaTime;
		if (blob.heatPhase > 1.0f) blob.heatPhase -= 1.0f;

		// Anchor point moves up and down based on heat phase
		float cyclePos = sin(blob.heatPhase * 2.0f * glm::pi<float>()) * 0.5f + 0.5f;

		// Temperature also affects target height range
		// Hot blobs can go higher, cold blobs stay lower
		float minHeight = m_baseHeight + blob.radius;
		float maxHeight = m_height - blob.radius - 0.2f; // Reduced margin

		// Hot blobs prefer top, cold blobs prefer bottom
		float heightBias = tempFactor * tempFactor; // Square for more extreme separation
		float effectiveMinHeight = glm::mix(minHeight, minHeight + (maxHeight - minHeight) * 0.15f, heightBias);
		float effectiveMaxHeight = glm::mix(maxHeight - (maxHeight - minHeight) * 0.15f, maxHeight, heightBias);

		float targetY = effectiveMinHeight + cyclePos * (effectiveMaxHeight - effectiveMinHeight);

		// Gentle horizontal drift
		float driftTime = glfwGetTime() * 0.3f + blob.heatPhase * 10.0f;
		float anchorX = cos(driftTime) * 0.4f * m_radius;
		float anchorZ = sin(driftTime) * 0.4f * m_radius;
		blob.anchorPoint = glm::vec3(anchorX, targetY, anchorZ);

		float tempAnchorStrength = glm::mix(2.0f, 0.6f, tempFactor);

		glm::vec3 toAnchor = blob.anchorPoint - blob.position;
		glm::vec3 springForce = toAnchor * m_springConstant * tempAnchorStrength;

		// Temperature affects damping: hot = less damping (more fluid), cold = more damping (more viscous)
		float tempDamping = glm::mix(m_dampingConstant * 1.5f, m_dampingConstant * 0.5f, tempFactor);
		glm::vec3 dampingForce = -blob.velocity * tempDamping;

		// Repulsion from all other blobs
		glm::vec3 repulsionForce(0.0f);
		for (size_t j = 0; j < m_blobs.size(); ++j) {
			if (i == j) continue;

			glm::vec3 toOther = blob.position - m_blobs[j].position;
			float dist = glm::length(toOther);

			if (dist < 0.01f) {
				toOther = glm::vec3(m_randomDist(m_rng), m_randomDist(m_rng), m_randomDist(m_rng));
				dist = 0.1f;
			}

			float minDist = (blob.radius + m_blobs[j].radius) * m_repulsionRange;

			if (dist < minDist) {
				float repulsionMag = m_repulsionStrength * (1.0f - dist / minDist);
				repulsionMag = repulsionMag * repulsionMag;
				repulsionForce += (toOther / dist) * repulsionMag;
			}
		}

		// Total force
		glm::vec3 totalForce = springForce + dampingForce + repulsionForce;

		// Integrate
		glm::vec3 oldVelocity = blob.velocity;
		blob.velocity += totalForce * deltaTime;
		blob.position += (oldVelocity + blob.velocity) * 0.5f * deltaTime;

		// Boundaries
		applyBoundaryConditions(blob);
	}

	// Merge/split operations
	mergeBlobsIfClose();
	splitLargeBlobs();
}


void LavaLamp::updateBlobPhysics(LavaBlob& blob, float dt) {
	// Update heat phase (continuous cycle 0->1->0)
	blob.heatPhase += blob.cycleSpeed * dt * 0.1f; // Slow cycle
	if (blob.heatPhase > 1.0f) blob.heatPhase -= 1.0f;

	// Update temperature based on position (for visual effect only)
	float distFromBottom = blob.position.y - m_baseHeight;
	float heightFrac = distFromBottom / (m_height - m_baseHeight);
	blob.temperature = m_ambientTemp + (1.0f - heightFrac) * (m_heaterTemp - m_ambientTemp);

	// Calculate anchor point position based on heat phase
	// Uses smooth sine wave for natural motion
	float cyclePos = sin(blob.heatPhase * 2.0f * glm::pi<float>()) * 0.5f + 0.5f; // 0 to 1 and back
	float targetY = m_baseHeight + blob.radius + cyclePos * (m_height - m_baseHeight - 2.0f * blob.radius);

	// Gentle horizontal drift for anchor point
	float driftTime = glfwGetTime() * 0.3f + blob.heatPhase * 10.0f;
	float anchorX = cos(driftTime) * 0.4f * m_radius;
	float anchorZ = sin(driftTime) * 0.4f * m_radius;

	blob.anchorPoint = glm::vec3(anchorX, targetY, anchorZ);

	// Spring force toward anchor point (Hooke's law)
	glm::vec3 toAnchor = blob.anchorPoint - blob.position;
	glm::vec3 springForce = toAnchor * m_springConstant * blob.anchorStrength;

	// Damping force (opposes velocity)
	glm::vec3 dampingForce = -blob.velocity * m_dampingConstant;

	// Repulsion from other blobs (prevents overlap)
	glm::vec3 repulsionForce = computeRepulsionForce(blob, 0); // Index computed in loop below

	// Total acceleration
	glm::vec3 totalForce = springForce + dampingForce + repulsionForce;
	glm::vec3 acceleration = totalForce; // Assume unit mass

	// Integrate velocity and position (Verlet integration for stability)
	glm::vec3 oldVelocity = blob.velocity;
	blob.velocity += acceleration * dt;
	blob.position += (oldVelocity + blob.velocity) * 0.5f * dt; // Average velocity

	// Apply soft boundary conditions
	applyBoundaryConditions(blob);
}

glm::vec3 LavaLamp::computeRepulsionForce(const LavaBlob& blob, size_t blobIndex) {
	glm::vec3 totalRepulsion(0.0f);

	for (size_t i = 0; i < m_blobs.size(); ++i) {
		if (i == blobIndex) continue; // Can't repel self

		glm::vec3 toOther = blob.position - m_blobs[i].position;
		float dist = glm::length(toOther);

		if (dist < 0.01f) {
			// Blobs too close, apply random nudge
			toOther = glm::vec3(
				m_randomDist(m_rng),
				m_randomDist(m_rng),
				m_randomDist(m_rng)
			);
			dist = 0.1f;
		}

		float minDist = (blob.radius + m_blobs[i].radius) * m_repulsionRange;

		if (dist < minDist) {
			// Soft repulsion using inverse square (like charges)
			float repulsionMag = m_repulsionStrength * (1.0f - dist / minDist);
			repulsionMag = repulsionMag * repulsionMag; // Square for stronger effect when close

			totalRepulsion += (toOther / dist) * repulsionMag;
		}
	}

	return totalRepulsion;
}

void LavaLamp::applyBoundaryConditions(LavaBlob& blob) {
	// Glass geometry: radius tapers from 1.8 at bottom (y=1.7) to 1.0 at top (y=10.0)
	const float glassBottomY = 1.7f;
	const float glassTopY = 10.0f;
	const float glassBottomRadius = 1.8f;
	const float glassTopRadius = 1.0f;
	const float glassThickness = 0.1f; // Interior offset from glass surface

	// Calculate current glass radius at blob's height
	float yFrac = glm::clamp((blob.position.y - glassBottomY) / (glassTopY - glassBottomY), 0.0f, 1.0f);
	float glassRadiusAtY = glm::mix(glassBottomRadius, glassTopRadius, yFrac);

	// Maximum allowed distance from center (account for blob radius and glass thickness)
	float maxDist = glassRadiusAtY - blob.radius - glassThickness;

	// Current distance from center axis
	float distFromCenter = sqrt(blob.position.x * blob.position.x + blob.position.z * blob.position.z);

	if (distFromCenter > maxDist) {
		// Soft spring force back toward center
		glm::vec2 xz(blob.position.x, blob.position.z);
		glm::vec2 dir = glm::normalize(xz);
		float penetration = distFromCenter - maxDist;

		// Gradual correction (not instant snap)
		glm::vec2 correction = -dir * penetration * 0.3f; // Weak spring
		blob.position.x += correction.x;
		blob.position.z += correction.y;

		// Velocity damping at boundary
		glm::vec2 vel2d(blob.velocity.x, blob.velocity.z);
		float radialVel = glm::dot(vel2d, dir);
		if (radialVel > 0.0f) {
			// Dampen outward velocity
			vel2d -= dir * radialVel * 0.8f;
			blob.velocity.x = vel2d.x;
			blob.velocity.z = vel2d.y;
		}
	}

	float minY = m_baseHeight + blob.radius;
	float maxY = m_height - blob.radius;

	if (blob.position.y < minY) {
		float penetration = minY - blob.position.y;
		blob.position.y += penetration * 0.3f;
		if (blob.velocity.y < 0.0f) {
			blob.velocity.y *= -0.3f;
		}
	}

	if (blob.position.y > maxY) {
		float penetration = blob.position.y - maxY;
		blob.position.y -= penetration * 0.3f;
		if (blob.velocity.y > 0.0f) {
			blob.velocity.y *= -0.3f;
		}
	}
}

void LavaLamp::handleBlobInteractions() {
	// Minimal repulsion - metaballs handle visual merging, we just prevent 
	// blobs from occupying the exact same position
	for (size_t i = 0; i < m_blobs.size(); ++i) {
		for (size_t j = i + 1; j < m_blobs.size(); ++j) {
			glm::vec3 diff = m_blobs[i].position - m_blobs[j].position;
			float d = glm::length(diff);
			float sumRadii = m_blobs[i].radius + m_blobs[j].radius;

			// Very gentle repulsion only when centers are extremely close
			// Metaballs will visually merge them smoothly anyway
			if (d > 1e-5f && d < sumRadii * 0.3f) {
				glm::vec3 dir = diff / d;

				// Extremely gentle soft repulsion force
				float overlap = sumRadii * 0.3f - d;
				float repulsionStrength = overlap / (sumRadii * 0.3f);

				// Very weak force - just prevent exact overlap
				float force = repulsionStrength * repulsionStrength * 0.2f;

				m_blobs[i].velocity += dir * force;
				m_blobs[j].velocity -= dir * force;
			}
		}
	}
}

float LavaLamp::computeDensityField(const glm::vec3& point) const {
	float fieldSum = 0.0f;
	for (const auto& blob : m_blobs) {
		float dist = glm::length(point - blob.position);
		float radius = blob.radius;

		if (radius <= 0.0f) continue;

		// Prevent division by zero
		dist = glm::max(dist, 0.01f);

		// Classic metaball formula: (r^2/d^2)^2
		float normalizedDist = radius / dist;
		float contribution = normalizedDist * normalizedDist;
		contribution = contribution * contribution;

		fieldSum += contribution;
	}
	return fieldSum;
}

glm::vec3 LavaLamp::computeDensityGradient(const glm::vec3& point) const {
	const float eps = 0.01f;
	float dx = computeDensityField(point + glm::vec3(eps, 0, 0)) - computeDensityField(point - glm::vec3(eps, 0, 0));
	float dy = computeDensityField(point + glm::vec3(0, eps, 0)) - computeDensityField(point - glm::vec3(0, eps, 0));
	float dz = computeDensityField(point + glm::vec3(0, 0, eps)) - computeDensityField(point - glm::vec3(0, 0, eps));
	return glm::vec3(dx, dy, dz);
}

void LavaLamp::mergeBlobsIfClose() {
	for (size_t i = 0; i < m_blobs.size(); ++i) {
		for (size_t j = i + 1; j < m_blobs.size(); ++j) {
			float dist = glm::distance(m_blobs[i].position, m_blobs[j].position);
			float combinedRadius = m_blobs[i].radius + m_blobs[j].radius;

			bool closeEnough = dist < combinedRadius * 0.25f;

			if (!closeEnough) continue; // Early exit if not close

			// Must be moving in similar directions
			glm::vec3 relVel = m_blobs[i].velocity - m_blobs[j].velocity;
			bool similarMotion = glm::length(relVel) < 0.2f; // Stricter: was 0.3f

			// Hot wax is fluid and merges easily; cold wax is viscous and bounces
			float tempDiff = abs(m_blobs[i].temperature - m_blobs[j].temperature);
			float avgTemp = (m_blobs[i].temperature + m_blobs[j].temperature) * 0.5f;

			bool similarTemp = tempDiff < 15.0f;
			bool warmEnough = avgTemp > (m_ambientTemp + 30.0f);

			// Prevents merging blobs going in opposite vertical directions
			float phaseDiff = abs(m_blobs[i].heatPhase - m_blobs[j].heatPhase);
			if (phaseDiff > 0.5f) phaseDiff = 1.0f - phaseDiff; // Wrap around
			bool similarPhase = phaseDiff < 0.2f; // Stricter: was 0.3f

			float avgY = (m_blobs[i].position.y + m_blobs[j].position.y) * 0.5f;
			float heightFrac = (avgY - m_baseHeight) / (m_height - m_baseHeight);

			//Merge conditions based on height
			float mergeProbability = 1.0f;
			if (heightFrac > 0.2f && heightFrac < 0.8f) {
				mergeProbability = 0.3f;
			}

			// Random roll based on probability
			bool allowedByHeight = (m_randomDist(m_rng) + 1.0f) * 0.5f < mergeProbability;

			float vol1 = pow(m_blobs[i].radius, 3.0f);
			float vol2 = pow(m_blobs[j].radius, 3.0f);
			float newRadius = pow(vol1 + vol2, 1.0f / 3.0f);
			bool notTooLarge = newRadius < 1.2f; // Prevent super-blobs

			if (closeEnough && similarMotion && similarTemp && warmEnough &&
				similarPhase && allowedByHeight && notTooLarge) {

				// Volume-conserving merge
				float w1 = vol1 / (vol1 + vol2);
				float w2 = vol2 / (vol1 + vol2);

				// Weighted averaging
				m_blobs[i].position = m_blobs[i].position * w1 + m_blobs[j].position * w2;
				m_blobs[i].velocity = (m_blobs[i].velocity * w1 + m_blobs[j].velocity * w2) * 0.9f;
				m_blobs[i].anchorPoint = m_blobs[i].anchorPoint * w1 + m_blobs[j].anchorPoint * w2;
				m_blobs[i].radius = newRadius;
				m_blobs[i].temperature = m_blobs[i].temperature * w1 + m_blobs[j].temperature * w2;
				m_blobs[i].heatPhase = m_blobs[i].heatPhase * w1 + m_blobs[j].heatPhase * w2;
				m_blobs[i].cycleSpeed = m_blobs[i].cycleSpeed * w1 + m_blobs[j].cycleSpeed * w2;
				m_blobs[i].color = m_blobs[i].color * w1 + m_blobs[j].color * w2;
				m_blobs[i].blobbiness = m_blobs[i].blobbiness * w1 + m_blobs[j].blobbiness * w2;

				m_blobs.erase(m_blobs.begin() + j);
				--j;
			}
		}
	}
}

void LavaLamp::splitLargeBlobs() {
	// Lower threshold for splitting to counterbalance merging
	const float maxRadius = 0.85f; // Was 1.0f - split earlier
	size_t originalSize = m_blobs.size();

	for (size_t i = 0; i < originalSize; ++i) {
		if (m_blobs[i].radius > maxRadius) {

			float heightFrac = (m_blobs[i].position.y - m_baseHeight) / (m_height - m_baseHeight);
			bool inCoolingZone = heightFrac > 0.6f; // Top 40% of lamp

			float speed = glm::length(m_blobs[i].velocity);
			bool highVelocity = speed > 1.5f; // Moving fast

			// Cool blobs split more easily (more viscous, less able to hold together)
			float tempFactor = (m_blobs[i].temperature - m_ambientTemp) /
				glm::max(1.0f, m_heaterTemp - m_ambientTemp);
			bool coolEnough = tempFactor < 0.4f; // Below 40% of max heat

			// Split if
			bool shouldSplit = inCoolingZone || highVelocity ||
				(coolEnough && m_blobs[i].radius > 0.95f);

			if (!shouldSplit) continue;

			// 50-50 split for balance
			float parentVol = pow(m_blobs[i].radius, 3.0f);
			float childVol = parentVol * 0.5f;

			float childRadius = pow(childVol, 1.0f / 3.0f);
			float parentRadius = childRadius;

			// Split perpendicular to motion direction (creates natural separation)
			glm::vec3 motionDir = glm::normalize(m_blobs[i].velocity);
			glm::vec3 perpDir = glm::vec3(-motionDir.z, 0.0f, motionDir.x); // Perpendicular in XZ plane
			if (glm::length(perpDir) < 0.1f) {
				perpDir = glm::vec3(1.0f, 0.0f, 0.0f); // Fallback
			}
			else {
				perpDir = glm::normalize(perpDir);
			}

			float separation = (childRadius + parentRadius) * 1.1f; // Slightly more separation

			// Create child blob
			LavaBlob child = m_blobs[i];
			child.radius = childRadius;
			child.position = m_blobs[i].position + perpDir * separation;

			// Give child a slight velocity boost away from parent
			child.velocity = m_blobs[i].velocity + perpDir * 0.2f;

			child.heatPhase += 0.15f; // More offset phase (was 0.1f)
			if (child.heatPhase > 1.0f) child.heatPhase -= 1.0f;
			child.cycleSpeed = m_blobs[i].cycleSpeed + m_randomDist(m_rng) * 0.15f; // More variation

			// Update parent
			m_blobs[i].radius = parentRadius;
			m_blobs[i].position -= perpDir * separation * 0.5f;
			m_blobs[i].velocity -= perpDir * 0.2f; // Push parent away too

			m_blobs.push_back(child);
		}
	}
}

void LavaLamp::updateAnchorPoints(float dt) {
	// Anchor points are computed per-blob based on their heat phase
	// No global update needed in this design
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





//Code From Application

void LavaLamp::ensureDepthFBO(int width, int height) {
	if (m_depthFBO != 0 && m_depthTexW == width && m_depthTexH == height)
		return;

	if (m_depthFBO != 0) {
		glDeleteFramebuffers(1, &m_depthFBO);
		m_depthFBO = 0;
	}
	if (m_depthTextureFront != 0) {
		glDeleteTextures(1, &m_depthTextureFront);
		m_depthTextureFront = 0;
	}
	if (m_depthTextureBack != 0) {
		glDeleteTextures(1, &m_depthTextureBack);
		m_depthTextureBack = 0;
	}

	glGenTextures(1, &m_depthTextureFront);
	glBindTexture(GL_TEXTURE_2D, m_depthTextureFront);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	glGenTextures(1, &m_depthTextureBack);
	glBindTexture(GL_TEXTURE_2D, m_depthTextureBack);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &m_depthFBO);
	m_depthTexW = width;
	m_depthTexH = height;
}

void LavaLamp::initialiseLavaLamp(const std::string& shader_vertex_path, const std::string& shader_fragment_path) {
	// Build lava lamp shader
	cgra::shader_builder lava_sb;
	lava_sb.set_shader(GL_VERTEX_SHADER, shader_vertex_path);
	lava_sb.set_shader(GL_FRAGMENT_SHADER, shader_fragment_path);
	m_lavaShader = lava_sb.build();

	// Initialize the lava lamp simulation with 5 blobs
	initialize(5);

	m_threshold = 1.0f;         // Lower threshold
	m_heaterTemp = 120.0f;      // increased heater for stronger rise
	m_gravity = -9.8f;          // fixed gravity (permanent)

	// Depth FBO will be created lazily in render (size depends on framebuffer)
	m_depthFBO = 0;
	m_depthTextureFront = 0;
	m_depthTextureBack = 0;
	m_depthTexW = m_depthTexH = 0;

	// Set initial time
	m_lastTime = static_cast<float>(glfwGetTime());

	// Geometry
	m_lampGlassMesh = createLampContainerGlass();
	m_lampMetalMesh = createLampContainerMetal();
	m_fullscreenQuadMesh = createFullscreenQuad();
}

cgra::gl_mesh LavaLamp::createFullscreenQuad() {
	cgra::mesh_builder builder;

	cgra::mesh_vertex v0, v1, v2, v3;
	v0.pos = vec3(-1, -1, 0); v0.uv = vec2(0, 0);
	v1.pos = vec3(1, -1, 0);  v1.uv = vec2(1, 0);
	v2.pos = vec3(1, 1, 0);   v2.uv = vec2(1, 1);
	v3.pos = vec3(-1, 1, 0);  v3.uv = vec2(0, 1);

	builder.push_vertex(v0);
	builder.push_vertex(v1);
	builder.push_vertex(v2);
	builder.push_vertex(v3);

	builder.push_index(0); builder.push_index(1); builder.push_index(2);
	builder.push_index(0); builder.push_index(2); builder.push_index(3);

	return builder.build();
}

cgra::gl_mesh LavaLamp::createLampContainerGlass() {
	cgra::mesh_builder builder;
	const int segments = 64;

	struct ProfilePoint {
		float height;
		float radius;
	};

	std::vector<ProfilePoint> profile = {
		{1.7f, 1.8f},
		{10.0f, 1.0f}
	};

	std::vector<int> ringStarts;
	for (size_t p = 0; p < profile.size(); ++p) {
		ringStarts.push_back(builder.vertices.size());
		for (int i = 0; i <= segments; ++i) {
			float angle = 2.0f * pi<float>() * i / segments;
			float x = profile[p].radius * cos(angle);
			float z = profile[p].radius * sin(angle);
			float y = profile[p].height;

			cgra::mesh_vertex v;
			v.pos = vec3(x, y, z);
			v.norm = normalize(vec3(x, 0.0f, z));
			v.uv = vec2(float(i) / segments, float(p) / (profile.size() - 1));
			builder.push_vertex(v);
		}
	}

	// Connect the two rings into quads
	int currentRing = ringStarts[0];
	int nextRing = ringStarts[1];
	for (int i = 0; i < segments; ++i) {
		int curr = currentRing + i;
		int next = currentRing + i + 1;
		int currAbove = nextRing + i;
		int nextAbove = nextRing + i + 1;

		builder.push_index(curr);
		builder.push_index(currAbove);
		builder.push_index(next);

		builder.push_index(next);
		builder.push_index(currAbove);
		builder.push_index(nextAbove);
	}

	return builder.build();
}

cgra::gl_mesh LavaLamp::createLampContainerMetal() {
	cgra::mesh_builder builder;
	const int segments = 64;

	// Lower third of bulb (metal)
	{
		struct ProfilePoint {
			float height;
			float radius;
		};

		std::vector<ProfilePoint> lowerBulb = {
			{0.0f, 1.2f},
			{1.7f, 1.8f}
		};

		std::vector<int> ringStarts;
		for (size_t p = 0; p < lowerBulb.size(); ++p) {
			ringStarts.push_back(builder.vertices.size());
			for (int i = 0; i <= segments; ++i) {
				float angle = 2.0f * pi<float>() * i / segments;
				float x = lowerBulb[p].radius * cos(angle);
				float z = lowerBulb[p].radius * sin(angle);
				float y = lowerBulb[p].height;

				cgra::mesh_vertex v;
				v.pos = vec3(x, y, z);
				v.norm = normalize(vec3(x, 0.0f, z));
				v.uv = vec2(float(i) / segments, float(p) / (lowerBulb.size() - 1));
				builder.push_vertex(v);
			}
		}

		for (size_t p = 0; p < lowerBulb.size() - 1; ++p) {
			int currentRing = ringStarts[p];
			int nextRing = ringStarts[p + 1];
			for (int i = 0; i < segments; ++i) {
				int curr = currentRing + i;
				int next = currentRing + i + 1;
				int currAbove = nextRing + i;
				int nextAbove = nextRing + i + 1;

				builder.push_index(curr);
				builder.push_index(currAbove);
				builder.push_index(next);

				builder.push_index(next);
				builder.push_index(currAbove);
				builder.push_index(nextAbove);
			}
		}
	}

	// Glass bottom cap (inverted)
	{
		float capHeight = 0.0f;
		float capDepth = 0.8f;
		float capRadius = 1.2f;
		int capStart = builder.vertices.size();

		for (int i = 0; i <= segments; ++i) {
			float angle = 2.0f * pi<float>() * i / segments;
			float xOuter = capRadius * cos(angle);
			float zOuter = capRadius * sin(angle);

			cgra::mesh_vertex vTop;
			vTop.pos = vec3(xOuter, capHeight, zOuter);
			vTop.norm = normalize(vec3(xOuter, 0, zOuter));
			vTop.uv = vec2(float(i) / segments, 0.0f);

			cgra::mesh_vertex vBottom;
			vBottom.pos = vec3(0, capHeight - capDepth, 0);
			vBottom.norm = vec3(0, -1, 0);
			vBottom.uv = vec2(0.5f, 1.0f);

			builder.push_vertex(vTop);
			builder.push_vertex(vBottom);
		}

		for (int i = 0; i < segments; ++i) {
			int i0 = capStart + i * 2;
			int i1 = capStart + i * 2 + 1;
			int i2 = capStart + (i + 1) * 2;
			int i3 = capStart + (i + 1) * 2 + 1;

			builder.push_index(i0);
			builder.push_index(i2);
			builder.push_index(i1);

			builder.push_index(i1);
			builder.push_index(i2);
			builder.push_index(i3);
		}
	}

	// Glass top cap
	{
		float capBottomY = 10.0f;
		float capTopY = 11.0f;
		float capRadiusBottom = 1.0f;
		float capRadiusTop = 0.8f;
		int capStart = builder.vertices.size();

		for (int i = 0; i <= segments; ++i) {
			float angle = 2.0f * pi<float>() * i / segments;
			float x1 = capRadiusBottom * cos(angle);
			float z1 = capRadiusBottom * sin(angle);
			float x2 = capRadiusTop * cos(angle);
			float z2 = capRadiusTop * sin(angle);

			cgra::mesh_vertex v1, v2;
			v1.pos = vec3(x1, capBottomY, z1);
			v2.pos = vec3(x2, capTopY, z2);
			v1.norm = normalize(vec3(x1, 0, z1));
			v2.norm = normalize(vec3(x2, 0, z2));

			builder.push_vertex(v1);
			builder.push_vertex(v2);
		}

		for (int i = 0; i < segments; ++i) {
			int i0 = capStart + i * 2;
			int i1 = capStart + i * 2 + 1;
			int i2 = capStart + (i + 1) * 2;
			int i3 = capStart + (i + 1) * 2 + 1;

			builder.push_index(i0);
			builder.push_index(i2);
			builder.push_index(i1);

			builder.push_index(i1);
			builder.push_index(i2);
			builder.push_index(i3);
		}

		int topCenter = builder.vertices.size();
		cgra::mesh_vertex vTopCenter;
		vTopCenter.pos = vec3(0, capTopY, 0);
		vTopCenter.norm = vec3(0, 1, 0);
		vTopCenter.uv = vec2(0.5f, 0.5f);
		builder.push_vertex(vTopCenter);

		for (int i = 0; i < segments; ++i) {
			int outer0 = capStart + i * 2 + 1;
			int outer1 = capStart + (i + 1) * 2 + 1;
			builder.push_index(topCenter);
			builder.push_index(outer0);
			builder.push_index(outer1);
		}
	}

	// Metal base
	{
		float baseHeight = -1.5f;
		float baseRadiusBottom = 2.5f;
		float baseRadiusTop = 1.2f;
		int baseStart = builder.vertices.size();

		for (int i = 0; i <= segments; ++i) {
			float angle = 2.0f * pi<float>() * i / segments;
			float x1 = baseRadiusBottom * cos(angle);
			float z1 = baseRadiusBottom * sin(angle);
			float y1 = baseHeight;

			float x2 = baseRadiusTop * cos(angle);
			float z2 = baseRadiusTop * sin(angle);
			float y2 = 0.0f;

			cgra::mesh_vertex v1, v2;
			v1.pos = vec3(x1, y1, z1);
			v2.pos = vec3(x2, y2, z2);
			v1.norm = normalize(vec3(x1, 0, z1));
			v2.norm = normalize(vec3(x2, 0, z2));
			v1.uv = vec2(float(i) / segments, 0.0f);
			v2.uv = vec2(float(i) / segments, 1.0f);

			builder.push_vertex(v1);
			builder.push_vertex(v2);
		}

		for (int i = 0; i < segments; ++i) {
			int i0 = baseStart + i * 2;
			int i1 = baseStart + i * 2 + 1;
			int i2 = baseStart + (i + 1) * 2;
			int i3 = baseStart + (i + 1) * 2 + 1;

			builder.push_index(i0);
			builder.push_index(i2);
			builder.push_index(i1);

			builder.push_index(i1);
			builder.push_index(i2);
			builder.push_index(i3);
		}

		int baseBottomCenter = builder.vertices.size();
		cgra::mesh_vertex baseBottomV;
		baseBottomV.pos = vec3(0, baseHeight, 0);
		baseBottomV.norm = vec3(0, -1, 0);
		baseBottomV.uv = vec2(0.5f, 0.5f);
		builder.push_vertex(baseBottomV);

		for (int i = 0; i < segments; ++i) {
			int outer0 = baseStart + i * 2;
			int outer1 = baseStart + (i + 1) * 2;
			builder.push_index(baseBottomCenter);
			builder.push_index(outer1);
			builder.push_index(outer0);
		}
	}

	return builder.build();
}

// The main rendering function, previously Application::renderLavaLamp
void LavaLamp::renderLavaLamp(const glm::mat4& view, const glm::mat4& proj, GLFWwindow* window,
	bool animate, bool show, float threshold,
	float heaterTemp, float gravity)
{
	if (!show) return;

	// Save current state
	GLboolean depthMask;
	glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
	GLint depthFunc;
	glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
	GLboolean blendEnabled = glIsEnabled(GL_BLEND);

	// retrieve the window size
	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	// Ensure depth FBO/textures exist at current size
	ensureDepthFBO(width, height);

	// Update simulation
	if (animate) {
		float currentTime = static_cast<float>(glfwGetTime());
		float deltaTime = currentTime - m_lastTime;
		m_lastTime = currentTime;
		deltaTime = std::min(deltaTime, 0.05f);
		update(deltaTime);
	}

	glUseProgram(m_lavaShader);

	// Set up matrices
	mat4 model = mat4(1.0f);
	mat4 modelView = view * model;
	mat4 normalMatrix = transpose(inverse(model));
	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uProjectionMatrix"), 1, GL_FALSE, value_ptr(proj));
	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uModelViewMatrix"), 1, GL_FALSE, value_ptr(modelView));
	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uModelMatrix"), 1, GL_FALSE, value_ptr(model));
	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uNormalMatrix"), 1, GL_FALSE, value_ptr(normalMatrix));
	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uViewMatrix"), 1, GL_FALSE, value_ptr(view));

	// Pass inverse matrices for raymarching
	mat4 invProj = inverse(proj);
	mat4 invView = inverse(view);
	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uInvProjectionMatrix"), 1, GL_FALSE, value_ptr(invProj));
	glUniformMatrix4fv(glGetUniformLocation(m_lavaShader, "uInvViewMatrix"), 1, GL_FALSE, value_ptr(invView));

	// Set time uniform
	glUniform1f(glGetUniformLocation(m_lavaShader, "uTime"), static_cast<float>(glfwGetTime()));

	// Set camera position (world-space)
	vec3 cameraPos = vec3(inverse(view) * vec4(0, 0, 0, 1));
	glUniform3fv(glGetUniformLocation(m_lavaShader, "uCameraPos"), 1, value_ptr(cameraPos));

	// Pass framebuffer resolution
	m_windowsize = vec2(width, height);
	glUniform2fv(glGetUniformLocation(m_lavaShader, "uResolution"), 1, value_ptr(m_windowsize));

	// Lamp parameters (use simulation getters so geometry + sim match)
	glUniform1f(glGetUniformLocation(m_lavaShader, "uLampRadius"), getRadius());
	glUniform1f(glGetUniformLocation(m_lavaShader, "uLampTopRadius"), 1.0f);
	glUniform1f(glGetUniformLocation(m_lavaShader, "uLampHeight"), getHeight());
	glUniform1f(glGetUniformLocation(m_lavaShader, "uThreshold"), threshold);

	// Radius padding (small safety margin)
	GLint locPad = glGetUniformLocation(m_lavaShader, "uRadiusPadding");
	if (locPad != -1) {
		float padding = glm::max(0.02f, 0.02f * getRadius());
		glUniform1f(locPad, padding + 0.02f);
	}

	// Lighting
	vec3 lightPos = vec3(5.0f, 15.0f, 5.0f);
	vec3 lightColor = vec3(1.0f, 1.0f, 1.0f);
	vec3 ambientColor = vec3(0.2f, 0.1f, 0.1f);
	glUniform3fv(glGetUniformLocation(m_lavaShader, "uLightPos"), 1, value_ptr(lightPos));
	glUniform3fv(glGetUniformLocation(m_lavaShader, "uLightColor"), 1, value_ptr(lightColor));
	glUniform3fv(glGetUniformLocation(m_lavaShader, "uAmbientColor"), 1, value_ptr(ambientColor));

	// Blob data
	auto positions = getBlobPositions();
	auto radii = getBlobRadii();
	auto blobbiness = getBlobBlobbiness();
	auto colors = getBlobColors();
	int blobCount = getBlobCount();

	glUniform1i(glGetUniformLocation(m_lavaShader, "uBlobCount"), blobCount);
	if (blobCount > 0) {
		int count = std::min(blobCount, 16);
		glUniform4fv(glGetUniformLocation(m_lavaShader, "uBlobPositions"), count, value_ptr(positions[0]));
		glUniform1fv(glGetUniformLocation(m_lavaShader, "uBlobRadii"), count, radii.data());
		glUniform1fv(glGetUniformLocation(m_lavaShader, "uBlobBlobbiness"), count, blobbiness.data());
		glUniform3fv(glGetUniformLocation(m_lavaShader, "uBlobColors"), count, value_ptr(colors[0]));
	}

	// PASS 1: Depth-only pre-pass (metal + glass write depth)
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	// Render opaque metal (writes depth)
	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 2);
	m_lampMetalMesh.draw();

	// PASS 2: Metaball raymarching
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);

	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 1);
	glUniform1i(glGetUniformLocation(m_lavaShader, "uIsFullscreenQuad"), 1);

	// Bind depth texture from metal pre-pass
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_depthTextureFront);
	glUniform1i(glGetUniformLocation(m_lavaShader, "uDepthTexture"), 0);

	m_fullscreenQuadMesh.draw();
	glUniform1i(glGetUniformLocation(m_lavaShader, "uIsFullscreenQuad"), 0);

	// PASS 3: Metal color pass
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);

	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 2);
	m_lampMetalMesh.draw();

	// PASS 4: Glass color pass
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);

	glUniform1i(glGetUniformLocation(m_lavaShader, "uRenderMode"), 0);
	m_lampGlassMesh.draw();

	// Restore state
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	glDisable(GL_BLEND);

	glDepthMask(depthMask);
	glDepthFunc(depthFunc);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	if (!blendEnabled) {
		glDisable(GL_BLEND);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
	glUseProgram(0);
}

