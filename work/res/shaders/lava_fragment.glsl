#version 330 core

out vec4 FragColor;

// Inputs from vertex shader
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

#define MAX_BLOBS 16
uniform vec4 uBlobPositions[MAX_BLOBS];
uniform float uBlobRadii[MAX_BLOBS];
uniform float uBlobBlobbiness[MAX_BLOBS];
uniform vec3 uBlobColors[MAX_BLOBS];
uniform int uBlobCount;

// Matrices
uniform mat4 uProjectionMatrix;
uniform mat4 uModelViewMatrix;
uniform mat4 uInvProjectionMatrix;
uniform mat4 uInvViewMatrix;
uniform mat4 uViewMatrix;

// Other uniforms
uniform vec3 uCameraPos;
uniform float uThreshold;
uniform vec2 uResolution;

// Depth textures (front + back)
uniform sampler2D uDepthFrontTex;
uniform sampler2D uDepthBackTex;
uniform int uHaveDepthTex; // 0 = no, 1 = yes

// Lighting
uniform vec3 uLightPos;
uniform vec3 uLightColor;
uniform vec3 uAmbientColor;

// Lamp geometry
uniform float uLampRadius;
uniform float uLampTopRadius;
uniform float uLampHeight;
uniform float uRadiusPadding;

// Render mode: 0 = glass, 1 = metaballs, 2 = metal
uniform int uRenderMode;
uniform int uIsFullscreenQuad;

// Compute metaball field density at a point
float computeField(vec3 point) {
	float density = 0.0;
	for (int i = 0; i < uBlobCount && i < MAX_BLOBS; i++) {
		vec3 blobPos = uBlobPositions[i].xyz;
		float radius = max(0.0001, uBlobRadii[i]);
		vec3 diff = point - blobPos;
		float dist = length(diff);

		// Simple field function
		if (dist < radius * 2.0) {
			float normalized = dist / (radius * 2.0);
			float field = 1.0 - normalized;
			density += field * field;
		}
	}
	return density;
}

// Compute color at a point
vec3 computeBlobColor(vec3 point) {
	if (uBlobCount == 0) {
		return vec3(1.0, 0.3, 0.0); // Orange test
	}

	vec3 colorSum = vec3(0.0);
	float weightSum = 0.0;

	for (int i = 0; i < uBlobCount && i < MAX_BLOBS; i++) {
		vec3 blobPos = uBlobPositions[i].xyz;
		float radius = max(0.0001, uBlobRadii[i]);
		float dist = length(point - blobPos);

		if (dist < radius * 2.0) {
			float weight = 1.0 - (dist / (radius * 2.0));
			weight = weight * weight;
			colorSum += uBlobColors[i] * weight;
			weightSum += weight;
		}
	}

	return (weightSum > 0.001) ? colorSum / weightSum : vec3(1.0, 0.3, 0.0);
}

// Compute gradient (normal) using finite differences
vec3 computeGradient(vec3 p) {
	float eps = 0.02;
	float dx = computeField(p + vec3(eps, 0, 0)) - computeField(p - vec3(eps, 0, 0));
	float dy = computeField(p + vec3(0, eps, 0)) - computeField(p - vec3(0, eps, 0));
	float dz = computeField(p + vec3(0, 0, eps)) - computeField(p - vec3(0, 0, eps));
	vec3 grad = vec3(dx, dy, dz);
	if (length(grad) > 0.001) {
		return normalize(grad);
	}
	return vec3(0, 1, 0);
}

// Simple shading
vec3 simpleShading(vec3 pos, vec3 normal, vec3 baseColor) {
	vec3 lightDir = normalize(uLightPos - pos);
	float diff = max(dot(normal, lightDir), 0.0) * 0.8 + 0.2;
	return baseColor * diff;
}

// Robust ray-cylinder intersection (capped by minY,maxY)
vec2 rayCylinderIntersect(vec3 ro, vec3 rd, float radius, float minY, float maxY) {
	float a = rd.x * rd.x + rd.z * rd.z;

	if (abs(a) < 1e-8) {
		if (abs(rd.y) < 1e-8) return vec2(-1.0);

		float tCap = (minY - ro.y) / rd.y;
		vec3 p = ro + rd * tCap;
		if (tCap >= 0.0 && (p.x * p.x + p.z * p.z) <= radius * radius)
			return vec2(tCap, tCap);

		tCap = (maxY - ro.y) / rd.y;
		p = ro + rd * tCap;
		if (tCap >= 0.0 && (p.x * p.x + p.z * p.z) <= radius * radius)
			return vec2(tCap, tCap);

		return vec2(-1.0);
	}

	float b = 2.0 * (ro.x * rd.x + ro.z * rd.z);
	float c = ro.x * ro.x + ro.z * ro.z - radius * radius;
	float disc = b * b - 4.0 * a * c;

	if (disc < 0.0) return vec2(-1.0);

	float s = sqrt(disc);
	float t0 = (-b - s) / (2.0 * a);
	float t1 = (-b + s) / (2.0 * a);

	if (t0 > t1) {
		float tmp = t0;
		t0 = t1;
		t1 = tmp;
	}

	float y0 = ro.y + rd.y * t0;
	float y1 = ro.y + rd.y * t1;

	if ((y0 < minY && y1 < minY) || (y0 > maxY && y1 > maxY)) {
		if (abs(rd.y) > 1e-8) {
			float tPlane = (minY - ro.y) / rd.y;
			vec3 p = ro + rd * tPlane;
			if (tPlane >= 0.0 && (p.x * p.x + p.z * p.z) <= radius * radius)
				return vec2(tPlane, tPlane);

			tPlane = (maxY - ro.y) / rd.y;
			p = ro + rd * tPlane;
			if (tPlane >= 0.0 && (p.x * p.x + p.z * p.z) <= radius * radius)
				return vec2(tPlane, tPlane);
		}
		return vec2(-1.0);
	}

	float ty0 = (minY - ro.y) / rd.y;
	float ty1 = (maxY - ro.y) / rd.y;
	float tymin = min(ty0, ty1);
	float tymax = max(ty0, ty1);

	float tEnter = max(t0, tymin);
	float tExit = min(t1, tymax);

	if (tExit < 0.0) return vec2(-1.0);
	if (tEnter < 0.0) tEnter = 0.0;
	if (tEnter > tExit) return vec2(-1.0);

	return vec2(tEnter, tExit);
}

void main() {
	// Glass rendering
	if (uRenderMode == 0) {
		vec3 baseColor = vec3(0.2, 0.6, 1.0);
		vec3 shaded = simpleShading(FragPos, normalize(Normal), baseColor);
		FragColor = vec4(shaded, 0.35);
		return;
	}

	// Metal rendering
	if (uRenderMode == 2) {
		vec3 baseColor = vec3(0.7, 0.7, 0.75);
		vec3 shaded = simpleShading(FragPos, normalize(Normal), baseColor);
		FragColor = vec4(shaded, 1.0);
		return;
	}

	// METABALL RAYMARCHING (uRenderMode == 1)
	vec2 uv = TexCoord; // use TexCoord for depth sampling
	vec2 ndcXY = uv * 2.0 - 1.0;

	// Reconstruct ray in world space using inverse matrices
	vec4 nearClip = vec4(ndcXY, -1.0, 1.0);
	vec4 farClip = vec4(ndcXY, 1.0, 1.0);

	vec4 nearView = uInvProjectionMatrix * nearClip;
	nearView /= nearView.w;
	vec4 farView = uInvProjectionMatrix * farClip;
	farView /= farView.w;

	vec3 nearWorld = (uInvViewMatrix * vec4(nearView.xyz, 1.0)).xyz;
	vec3 farWorld = (uInvViewMatrix * vec4(farView.xyz, 1.0)).xyz;

	// Use the near-plane world-space point as the ray origin.
	// Using nearWorld keeps ray origin and reconstructed depth points consistent.
	vec3 rayOrigin = nearWorld;
	vec3 rayDir = normalize(farWorld - nearWorld);

	// Use max radius for initial cylinder intersection (fast reject)
	float maxRadius = max(uLampRadius, uLampTopRadius) + uRadiusPadding;
	vec2 cylinderHit = rayCylinderIntersect(rayOrigin, rayDir, maxRadius, 0.0, uLampHeight);

	if (cylinderHit.x < 0.0) {
		discard;
		return;
	}

	float tStart = cylinderHit.x;
	float tEnd = cylinderHit.y;
	if (tEnd < 0.0) tEnd = tStart + 20.0;

	// If we have depth textures, reconstruct front AND back world points and clamp
	if (uHaveDepthTex == 1) {
		float depthFront = texture(uDepthFrontTex, uv).r;
		float depthBack = texture(uDepthBackTex, uv).r;

		// reconstruct front world
		if (depthFront < 0.9999) {
			float ndcZ = depthFront * 2.0 - 1.0;
			vec4 clip = vec4(ndcXY, ndcZ, 1.0);
			vec4 viewPos = uInvProjectionMatrix * clip;
			viewPos /= viewPos.w;
			vec3 worldFront = (uInvViewMatrix * vec4(viewPos.xyz, 1.0)).xyz;
			float tSurfaceFront = dot(worldFront - nearWorld, rayDir);
			if (tSurfaceFront > 0.0) {
				tStart = max(tStart, tSurfaceFront + 1e-4);
			}
		}

		// reconstruct back world
		if (depthBack < 0.9999) {
			float ndcZb = depthBack * 2.0 - 1.0;
			vec4 clipb = vec4(ndcXY, ndcZb, 1.0);
			vec4 viewPosB = uInvProjectionMatrix * clipb;
			viewPosB /= viewPosB.w;
			vec3 worldBack = (uInvViewMatrix * vec4(viewPosB.xyz, 1.0)).xyz;
			float tSurfaceBack = dot(worldBack - nearWorld, rayDir);
			if (tSurfaceBack > 0.0) {
				tEnd = min(tEnd, tSurfaceBack - 1e-4);
			}
		}

		// if tEnd ends up <= tStart, nothing inside
		if (tEnd <= tStart + 1e-6) {
			discard;
			return;
		}
	}

	float t = tStart + 1e-5;
	float stepSize = 0.03;
	bool hit = false;
	vec3 hitPos;

	while (t < tEnd) {
		vec3 p = rayOrigin + rayDir * t;

		// compute tapered radius at p.y (linear interp)
		float yNorm = clamp(p.y / uLampHeight, 0.0, 1.0);
		float radiusAtY = mix(uLampRadius, uLampTopRadius, yNorm) + uRadiusPadding;

		// skip samples outside tapered boundary
		float distXZ = length(p.xz);
		if (distXZ > radiusAtY) {
			t += stepSize;
			continue;
		}

		if (p.y >= 0.0 && p.y <= uLampHeight) {
			float field = computeField(p);
			if (field >= uThreshold) {
				hitPos = p;
				hit = true;
				break;
			}
		}

		t += stepSize;
	}

	if (hit) {
		vec3 normal = computeGradient(hitPos);
		vec3 baseColor = computeBlobColor(hitPos);
		vec3 color = simpleShading(hitPos, normal, baseColor);
		FragColor = vec4(color, 1.0);

		// Correct depth for compositing
		vec4 clipPos = uProjectionMatrix * uViewMatrix * vec4(hitPos, 1.0);
		float ndcZ = clipPos.z / clipPos.w;
		gl_FragDepth = ndcZ * 0.5 + 0.5;
	}
	else {
		discard;
	}
}
