#pragma once

#include "ResourceIndex.hpp"
#include "Task.hpp"
#include "utils/IndexMap.hpp"

class TaskRegister {
public:
	struct TaskData {
		IndexMap<TaskIndex, std::string> names;
		IndexMap<TaskIndex, Task> tasks;
		IndexMap<TaskIndex, std::vector<std::byte>> taskData;
	};

	struct ResourceDefinitions {
		IndexMap<rendergraph::ResourceIndex, std::string> names;
		IndexMap<rendergraph::ResourceIndex, rendergraph::ResourceType> types;
	};

private:
	TaskData m_tasks;
	ResourceDefinitions m_resources;

public:
	rendergraph::ResourceIndex registerStaticImage(std::string name);
	rendergraph::ResourceIndex registerStaticBuffer(std::string name);

	TaskIndex addTask(std::string name, Task task, auto taskData) {
		TaskIndex index = m_tasks.tasks.size();

		std::vector<std::byte> rawData(sizeof(taskData));
		std::memcpy(rawData.data(), &taskData, sizeof(taskData));

		m_tasks.names[index] = std::move(name);
		m_tasks.tasks[index] = task;
		m_tasks.taskData[index] = std::move(rawData);

		return index;
	}
	TaskIndex addTask(std::string name, Task task) {
		TaskIndex index = m_tasks.tasks.size();

		m_tasks.names[index] = std::move(name);
		m_tasks.tasks[index] = task;
		m_tasks.taskData[index] = {};

		return index;
	}


	TaskData& getTasks() { return m_tasks; }
	const TaskData& getTasks() const { return m_tasks; }
	const ResourceDefinitions& getStaticResources() const { return m_resources; }
};
