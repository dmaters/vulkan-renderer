#pragma once

#include <optional>

#include "Task.hpp"

class BufferCopy : public Task {
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
	void setup(RenderGraphResourceSolver& graph) override;
	void execute(vk::CommandBuffer& buffer, const Resources& resources)
		override;

	BufferCopy(BufferCopyInfo info) : m_info(info) {}
};