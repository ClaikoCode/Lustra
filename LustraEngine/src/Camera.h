#pragma once

#include "LustraGLM.h"

struct Camera
{
	static constexpr glm::vec3 kWorldUp = {0.0f, 1.0f, 0.0f};
	static constexpr float kPitchMargin = glm::radians(2.0f);

	glm::vec3 position = {0.0f, 0.0f, 0.0f};

	// Rotation around world up axis.
	float yaw = 0.0f;
	// Rotation around right vector.
	float pitch = 0.0f;

	glm::vec3 forward = {0.0f, 0.0f, -1.0f};
	glm::vec3 right   = {1.0f, 0.0f, 0.0f};

	float nearZ = 0.1f;
	float farZ  = 1000.0f;

	// In radians.
	float horizontalFOV = glm::half_pi<float>();

	float aspect = 1.0f;

	glm::mat4 view     = glm::mat4(1.0f);
	glm::mat4 proj     = glm::mat4(1.0f);
	glm::mat4 viewProj = glm::mat4(1.0f);

	void SetAspect(uint32_t screenWidth, uint32_t screenHeight);

	void UpdateViewMat();

	void UpdateProj();

	// Calculates the direction from current position to focus point and derivec pitch + yaw.
	void ForceLookAt(glm::vec3 focusPoint);

	void Rotate(float dYaw, float dPitch);

	// Updates all internal matrices.
	// Make sure to call in such a fashion that all places which use these matricies use the same version of
	// them. In other words, call once per frame before using any matrices.
	void Update();
};

// Wrapper around a camera that translates user inputs into FPS movement.
struct FPSCamera
{
	Camera* cam = nullptr;

	// Assumes mouse is in relative mode.
	// NOTE: This calls Camera::Update() internally. Make sure to not call it directly if in use.
	void Update(float dt) const;
};
