#pragma once

#include <cstddef>
#include <string_view>

#include "utils/FixedVector.hpp"
#include "utils/IndexMap.hpp"

struct FeatureRegister {
	using FeatureIndex = std::size_t;
	using TaskIndex = std::size_t;

	struct Feature {
		std::string_view name;
		FixedVector<TaskIndex, 10> tasks;
	};

	IndexMap<FeatureIndex, Feature> features;
	IndexMap<TaskIndex, std::string_view> taskNames;

	FeatureIndex addFeature(std::string_view name);
	void addTask(FeatureIndex feature, TaskIndex task, std::string_view taskName);
};
