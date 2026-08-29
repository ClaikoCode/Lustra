#include "Camera.h"

#include "LustraLib/Logger.h"
#include "SDL3/SDL.h"
#include "glm/trigonometric.hpp"

void Camera::SetAspect(uint32_t screenWidth, uint32_t screenHeight)
{
	if (screenWidth == 0 || screenHeight == 0)
	{
		return;
	}

	aspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
}

void Camera::UpdateViewMat()
{
	view = glm::lookAt(position, position + forward, kWorldUp);
}

void Camera::UpdateProj()
{
	const float verticalFOV = 2.0f * glm::atan((glm::tan(horizontalFOV / 2.0f) / aspect));
	proj                    = glm::perspective(verticalFOV, aspect, nearZ, farZ);

	// Reverse Y since GLM assumes OpenGL standard which has NDC +Y as up when Vulkan assumes +Y as down.
	proj[1][1] *= -1.0f;
}

void Camera::ForceLookAt(glm::vec3 focusPoint)
{
	const glm::vec3 lookDir = glm::normalize(focusPoint - position);

	pitch = glm::asin(lookDir.y);
	yaw   = glm::atan(lookDir.z, lookDir.x); // Two independent values instead of z / x to keep signdness info.
}

void Camera::Rotate(float dYaw, float dPitch)
{
	yaw += dYaw;
	pitch += dPitch;
}

void Camera::Update()
{
	// Avoid pitch being exactly +-90 deg.
	pitch = glm::clamp(pitch, -glm::half_pi<float>() + kPitchMargin, glm::half_pi<float>() - kPitchMargin);

	forward = glm::normalize(glm::vec3(cos(yaw) * cos(pitch), sin(pitch), sin(yaw) * cos(pitch)));
	right   = glm::normalize(glm::cross(forward, kWorldUp));

	UpdateViewMat();
	UpdateProj();

	viewProj = proj * view;
}

void FPSCamera::Update(float dt) const
{
	if (cam == nullptr)
	{
		PRINT_TRACE("Camera not attached to FPS camera.");
		return;
	}

	float mouseDeltaX = 0.0f;
	float mouseDeltaY = 0.0f;
	glm::vec3 moveDir = glm::vec3(0.0f);

	// Process inputs
	{
		// Keyboard
		{
			int numkeys;
			const bool* state = SDL_GetKeyboardState(&numkeys);

			if (state[SDL_SCANCODE_W])
			{
				moveDir += cam->forward;
			}

			if (state[SDL_SCANCODE_D])
			{
				moveDir += cam->right;
			}

			if (state[SDL_SCANCODE_S])
			{
				moveDir -= cam->forward;
			}

			if (state[SDL_SCANCODE_A])
			{
				moveDir -= cam->right;
			}

			if (state[SDL_SCANCODE_SPACE] || state[SDL_SCANCODE_E])
			{
				moveDir += Camera::kWorldUp;
			}

			if (state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_Q])
			{
				moveDir -= Camera::kWorldUp;
			}
		}

		// Mouse
		{
			SDL_GetRelativeMouseState(&mouseDeltaX, &mouseDeltaY);
		}
	}

	const float mouseSensitivity = 0.01f;

	const float dYaw   = mouseDeltaX * mouseSensitivity;
	const float dPitch = -mouseDeltaY * mouseSensitivity;

	cam->Rotate(dYaw, dPitch);

	const float speed = 5.0f;

	glm::vec3 deltaPos = glm::vec3(0.0f);
	// TODO: Change to squared length check.
	if (glm::length(moveDir) > glm::epsilon<float>())
	{
		deltaPos = dt * speed * glm::normalize(moveDir);
	}

	cam->position += deltaPos;

	cam->Update();
}
