#pragma once
#include <cstdint>
#include <variant>
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

	struct Dependencies {
		std::vector<ResourceDependency> inputs = {};
		std::vector<ResourceDependency> outputs = {};
	};

	Dependencies (*setup)(SetupContext&) = [](SetupContext&) -> Dependencies { return {}; };
	void (*build)(BuildContext&) = [](BuildContext&) {};
};
