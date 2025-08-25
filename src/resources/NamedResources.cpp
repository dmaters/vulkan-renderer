#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

#include "resources/ResourceManager.hpp"

const std::unordered_map<std::string_view, ResourceManager::BufferDescription>
	ResourceManager::_defaultNamedBufferData = {
		{ "buffer_vertex",
         ResourceManager::BufferDescription {
			  .size = 12 << 1,
			  .usage = vk::BufferUsageFlagBits::eTransferDst |
	                   vk::BufferUsageFlagBits::eVertexBuffer,
		  } },
		{ "buffer_index",
         ResourceManager::BufferDescription {
			  .size = 12 << 1,
			  .usage = vk::BufferUsageFlagBits::eTransferDst |
	                   vk::BufferUsageFlagBits::eIndexBuffer,
		  } }
};

const std::unordered_map<std::string_view, ResourceManager::ImageDescription>
	ResourceManager::_defaultNamedImageData = {
		{
         "gbuffer_albedo", {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eR16G16B16A16Sfloat,
				.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                     vk::ImageUsageFlagBits::eInputAttachment |
	                     vk::ImageUsageFlagBits::eSampled,

			}, },
		{
         "gbuffer_normal", {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eR16G16B16A16Sfloat,
				.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                     vk::ImageUsageFlagBits::eInputAttachment |
	                     vk::ImageUsageFlagBits::eSampled,

			}, },
		{
         "gbuffer_worldpos", {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eR16G16B16A16Sfloat,
				.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                     vk::ImageUsageFlagBits::eInputAttachment |
	                     vk::ImageUsageFlagBits::eSampled,

			}, },
		{
         "gbuffer_roughnessMetallic", {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eR16G16B16A16Sfloat,
				.usage = vk::ImageUsageFlagBits::eColorAttachment |
	                     vk::ImageUsageFlagBits::eInputAttachment |
	                     vk::ImageUsageFlagBits::eSampled,

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

			}, },
		{
         "depth", {
				.width = 800,
				.height = 600,
				.depth = 1,
				.miplevels = 1,
				.format = vk::Format::eD24UnormS8Uint,
				.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,

			}, },
		{ "shadow_atlas",
         {
			  .width = 1024,
			  .height = 1024,
			  .depth = 1,
			  .miplevels = 1,
			  .format = vk::Format::eD16Unorm,
			  .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment |
	                   vk::ImageUsageFlagBits::eSampled,

		  } }
};

const std::unordered_map<std::string_view, int8_t>
	ResourceManager::_swapchainRatio = {
		{ "gbuffer_albedo",            1 },
        { "gbuffer_normal",            1 },
		{ "gbuffer_worldpos",          1 },
        { "gbuffer_roughnessMetallic", 1 },
		{ "main_color",                1 },
        { "depth",                     1 },
};
