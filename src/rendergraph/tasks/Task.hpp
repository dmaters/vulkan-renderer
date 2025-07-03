#pragma once

#include <variant>
#include <vulkan/vulkan.hpp>

#include "BufferCopy.hpp"
#include "ImageCopy.hpp"
#include "RenderPass.hpp"
#include "resources/Buffer.hpp"

using Task = std::variant<RenderPass, ImageCopy, BufferCopy>;
