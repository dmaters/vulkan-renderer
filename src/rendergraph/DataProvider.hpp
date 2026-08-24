#pragma once
#include <unordered_map>

#include "Task.hpp"
class Task::DataProvider {
private:
	std::unordered_map<TaskIndex, std::vector<std::byte>>& m_taskData;

public:
	DataProvider(std::unordered_map<TaskIndex, std::vector<std::byte>>& taskData) : m_taskData(taskData) {}

	template <typename T>
	T& getData(TaskIndex task) {
		auto& rawData = m_taskData[task];
		assert(rawData.size() == sizeof(T));
		return *reinterpret_cast<T*>(rawData.data());
	}
};
