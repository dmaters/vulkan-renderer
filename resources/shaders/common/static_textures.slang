#pragma once

[vk_binding(0, 1)] Texture2D _staticTextures[];

[vk_binding(1, 1)] SamplerState[4] _samplers;

enum class SamplerType {
	LinearClamp,
	LinearRepeat,
	NearestClamp,
	NearestRepeat
};
