#pragma once

// This include should be used everywhere glm is included.
// It contains defines that should be globally applied throughout the project.

// Forces GLM to use depth [0, 1] instead of [-1, 1].
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/glm.hpp"
