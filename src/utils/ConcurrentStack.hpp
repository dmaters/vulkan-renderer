#pragma once
#include <condition_variable>
#include <mutex>
#include <optional>
#include <vector>

template <typename T>
class ConcurrentStack {
public:
	class Inserter {
	private:
		ConcurrentStack& m_baseStack;

	public:
		Inserter(ConcurrentStack& baseStack) : m_baseStack(baseStack) {
			baseStack.registerProvider();
		}
		~Inserter() { m_baseStack.unregisterProvider(); }

		void push(T value) { m_baseStack.push(value); }
	};

private:
	std::mutex m_mutex;
	std::condition_variable m_cv;
	std::vector<T> m_values;
	std::atomic_uint m_providerCount = 0;

	// Additional flag to avoid skipping the wait for values in case the
	// provider thread is slow to start
	bool m_activated = false;

	void registerProvider() {
		m_activated = true;
		m_providerCount.fetch_add(1);
	}
	void unregisterProvider() {
		m_providerCount.fetch_sub(1);
		m_cv.notify_all();
	}

	void push(T value) {
		std::scoped_lock lock(m_mutex);
		m_values.push_back(value);
		m_cv.notify_one();
	}

	bool isRetired() {
		return (m_activated && m_providerCount.load() == 0 && m_values.empty());
	}

public:
	ConcurrentStack() {}

	Inserter getInserter() { return Inserter(*this); }

	// When empty, it waits for value if least one inserter is alive
	std::optional<T> pop_wait() {
		std::unique_lock lock(m_mutex);

		m_cv.wait(lock, [&] { return isRetired() || !m_values.empty(); });

		if (isRetired()) return std::nullopt;

		auto value = m_values.back();
		m_values.pop_back();

		return value;
	}

	std::optional<T> pop() {
		std::scoped_lock lock(m_mutex);
		if (m_values.empty()) return std::nullopt;
		auto value = m_values.back();
		m_values.pop_back();
		return value;
	}

	std::size_t size() {
		std::scoped_lock lock(m_mutex);
		return m_values.size();
	}

	std::vector<T>& container() { return m_values; }

	void reserve(std::size_t n) { m_values.reserve(n); }
};
