#pragma once
#include <cstdint>
#include <vulkan/vulkan.hpp>

#include "ResourceIndex.hpp"
#include "ResourceUsage.hpp"

using TaskIndex = uint32_t;

struct Task {
	struct SetupContext;
	struct BuildContext;

	class DataProvider;

	struct ResourceDependency {
		rendergraph::ResourceIndex resource;
		ResourceUsage::Type usage;
	};

	void (*setup)(SetupContext&);
	void (*build)(BuildContext&);
};
