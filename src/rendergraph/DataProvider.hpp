#pragma once

#include "Task.hpp"
#include "TaskRegister.hpp"

class Task::DataProvider {
private:
	TaskRegister& m_taskRegister;

public:
	DataProvider(TaskRegister& taskRegister) : m_taskRegister(taskRegister) {}

	template <typename T>
	T& getData(TaskIndex task) {
		auto& rawData = m_taskRegister.getTasks().taskData[task];
		assert(rawData.size() == sizeof(T));
		return *reinterpret_cast<T*>(rawData.data());
	}
};
