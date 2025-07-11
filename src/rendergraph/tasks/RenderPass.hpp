#pragma once

#include <string_view>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "material/MaterialManager.hpp"

struct Resources;
struct ResourceDependency;

class RenderPass {
public:
private:
	MaterialIndex m_material;
	std::vector<MaterialManager::ResourceDependency> m_dependencies;

public:
	RenderPass(
		MaterialIndex material,
		std::vector<MaterialManager::ResourceDependency> dependencies
	) :
		m_material(material), m_dependencies(dependencies) {}

	void setup(
		std::unordered_map<std::string_view, ResourceDependency>& images,
		std::unordered_map<std::string_view, ResourceDependency>& buffers
	);

	void execute(vk::CommandBuffer& buffer, const Resources& resources);
};
