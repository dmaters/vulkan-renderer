#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

#include "memory/MemoryAllocator.hpp"
#include "resources/ResourceManager.hpp"

const std::unordered_map<std::string_view, ResourceManager::BufferDescription>
	ResourceManager::_defaultNamedBufferData = {
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
	ResourceManager::_defaultNamedImageData = {
		{
         "gbuffer_albedo",   {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eR16G16B16A16Sfloat,
				.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                     vk::ImageUsageFlagBits::eInputAttachment |
	                     vk::ImageUsageFlagBits::eSampled,
				.transient = true,
			}, },
		{
         "gbuffer_normal",				 {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eR16G16B16A16Sfloat,
				.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                     vk::ImageUsageFlagBits::eInputAttachment |
	                     vk::ImageUsageFlagBits::eSampled,
				.transient = true,
			}, },
		{
         "gbuffer_worldpos",   {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eR16G16B16A16Sfloat,
				.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                     vk::ImageUsageFlagBits::eInputAttachment |
	                     vk::ImageUsageFlagBits::eSampled,
				.transient = true,
			}, },
		{
         "gbuffer_roughnessMetallic",				 {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eR16G16B16A16Sfloat,
				.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                     vk::ImageUsageFlagBits::eInputAttachment |
	                     vk::ImageUsageFlagBits::eSampled,
				.transient = true,
			}, },

		{
         "main_color", {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eR16G16B16A16Sfloat,
				.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                     vk::ImageUsageFlagBits::eTransferSrc,
				.transient = true,
			}, },
		{
         "depth",				 {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eD16Unorm,
				.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
				.transient = true,
			}, }
};

const std::unordered_map<std::string_view, int8_t>
	ResourceManager::_swapchainRatio = {
		{ "gbuffer_albedo",   1 },
		{ "gbuffer_normal",   1 },
		{ "gbuffer_worldpos", 1 },
		{ "main_color",       1 },
		{ "depth",            1 },
};
