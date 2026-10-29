#include "FeatureRegister.hpp"

FeatureRegister::FeatureIndex FeatureRegister::addFeature(std::string_view name) {
	FeatureIndex index = features.size();
	features[index] = {
		.name = name,
		.tasks = {},
	};
	return index;
}
void FeatureRegister::addTask(FeatureIndex feature, TaskIndex task, std::string_view name) {
	features[feature].tasks.push_back(task);
	taskNames[task] = name;
}
