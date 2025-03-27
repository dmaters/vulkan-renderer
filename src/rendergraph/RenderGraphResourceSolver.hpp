#pragma once

#include <optional>
#include <string_view>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>
#include <xstring>

class RenderGraph;
struct ResourceUsage {
	enum class Type {
		READ,
		WRITE,
		READ_WRITE,
	};
	Type type;
	vk::AccessFlags2 access;
	vk::PipelineStageFlags2 stage;

	bool operator==(const ResourceUsage& b) const {
		return (type == Type::READ && b.type == ResourceUsage::Type::READ) ||
		       (type == Type::WRITE && b.type == ResourceUsage::Type::WRITE);
	}
};
class RenderGraphResourceSolver {
public:
	struct ImageDependencyInfo {
		std::string_view name;
		ResourceUsage usage;
		std::optional<vk::ClearColorValue> clearValue;
		std::optional<vk::ImageLayout> requiredLayout;
	};

	struct BufferDependencyInfo {
		std::string_view name;
		ResourceUsage usage;
	};

private:
	friend class RenderGraph;
	std::vector<ImageDependencyInfo> m_imageDependencies;
	std::vector<BufferDependencyInfo> m_bufferDependencies;

public:
	void registerDependency(ImageDependencyInfo info) {
		m_imageDependencies.push_back(info);
	}
	void registerDependency(BufferDependencyInfo info) {
		m_bufferDependencies.push_back(info);
	}
};
