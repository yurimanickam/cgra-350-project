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
