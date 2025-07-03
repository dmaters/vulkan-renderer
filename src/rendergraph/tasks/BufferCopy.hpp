#pragma once

#include <string_view>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

struct Resources;
struct ResourceDependency;

class BufferCopy {
public:
	struct BufferSlice {
		std::string name;
		size_t length;
		size_t offset = 0;
	};
	struct BufferCopyInfo {
		BufferSlice origin;
		BufferSlice destination;
	};

	BufferCopyInfo m_info;

private:
public:
	void setup(
		std::unordered_map<std::string_view, ResourceDependency>& images,
		std::unordered_map<std::string_view, ResourceDependency>& buffers
	);
	void execute(vk::CommandBuffer& buffer, const Resources& resources);

	BufferCopy(BufferCopyInfo info) : m_info(info) {}
};