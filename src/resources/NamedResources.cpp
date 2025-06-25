#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

#include "memory/MemoryAllocator.hpp"
#include "resources/ResourceManager.hpp"

const std::unordered_map<std::string_view, ResourceManager::BufferDescription>
	ResourceManager::s_defaultNamedBufferData = {
		{ "buffer_vertex",
         ResourceManager::BufferDescription {
			  .size = 12 << 1,
			  .usage = vk::BufferUsageFlagBits::eTransferDst |
	                   vk::BufferUsageFlagBits::eVertexBuffer,
			  .location = AllocationLocation::Device,
			  .transient = false,
		  } },
		{ "buffer_index",
         ResourceManager::BufferDescription {
			  .size = 12 << 1,
			  .usage = vk::BufferUsageFlagBits::eTransferDst |
	                   vk::BufferUsageFlagBits::eIndexBuffer,
			  .location = AllocationLocation::Device,
			  .transient = false,
		  } }
};

const std::unordered_map<std::string_view, ResourceManager::ImageDescription>
	ResourceManager::s_defaultNamedImageData = {
		{
         "gbuffer_albedo",   {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eR8G8B8A8Srgb,
				.usage = vk::ImageUsageFlagBits::eColorAttachment,
				.transient = true,
			}, },
		{
         "gbuffer_normal",				 {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eR16G16B16A16Sfloat,
				.usage = vk::ImageUsageFlagBits::eColorAttachment,
				.transient = true,
			}, },
		{
         "gbuffer_worldpos",   {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eR16G16B16A16Sfloat,
				.usage = vk::ImageUsageFlagBits::eColorAttachment,
				.transient = true,
			}, },

		{
         "main_color",				 {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eR16G16B16A16Sfloat,
				.usage = vk::ImageUsageFlagBits::eColorAttachment,
				.transient = true,
			}, },
		{
         "main_depth", {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eD16Unorm,
				.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
				.transient = true,
			}, }
};