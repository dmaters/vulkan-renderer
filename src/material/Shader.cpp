

#include "Shader.hpp"

#include <cstdint>
#include <fstream>
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>

vk::ShaderModule Shader::GetShader(
	vk::Device& device, const std::filesystem::path& path
) {
	assert(path.extension() == ".spv");

	std::ifstream file(path, std::ios::ate | std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("failed to open file!");
	}
	std::vector<char> code(file.tellg());
	file.seekg(0);
	file.read(code.data(), code.size());
	file.close();

	vk::ShaderModuleCreateInfo info;
	info.pCode = (uint32_t*)code.data();
	info.codeSize = code.size();
	return device.createShaderModule(info);
}
