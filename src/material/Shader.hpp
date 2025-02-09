#pragma once

#include <filesystem>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_handles.hpp>

class Shader {
public:
	enum class ShaderStage {
		Vertex,
		Fragment,
		Compute
	};

	static vk::ShaderModule GetShader(
		vk::Device& device, const std::filesystem::path& path
	);
};