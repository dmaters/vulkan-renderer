#pragma once

#include <cstdint>
#include <unordered_map>
#include <variant>

#include "rendergraph/RenderGraphBuilder.hpp"
#include "resources/ResourceManager.hpp"

typedef uint32_t AttachmentBinding;
typedef std::variant<ImageHandle, BufferHandle> Attachment;

struct AttachmentGroup {
	std::unordered_map<AttachmentBinding, Attachment> attachments;
	std::unordered_map<AttachmentBinding, ResourceUsage> usages;
};
