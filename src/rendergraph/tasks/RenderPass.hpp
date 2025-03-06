#pragma once

#include <optional>
#include <vulkan/vulkan.hpp>

#include "Task.hpp"
#include "material/MaterialManager.hpp"

class RenderPass : public Task {
public:
	struct Attachments {
		std::optional<std::string_view> color;
		std::optional<std::string_view> depth;
	};

private:
	Attachments m_attachments;

protected:
public:
	inline void setAttachments(Attachments attachments) {
		m_attachments = attachments;
	}

	void execute(vk::CommandBuffer& commandBuffer, const Resources& resources)
		override;
};