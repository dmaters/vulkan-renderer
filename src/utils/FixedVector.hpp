#pragma once

#include <array>

template <typename T, std::size_t N>
class FixedVector {
private:
	std::array<T, N> m_elements;
	std::size_t m_count = 0;

public:
	void push_back(T value) { m_elements[m_count++] = value; }
	std::size_t size() const { return m_count; }

	T& operator[](std::size_t i) { return m_elements[i]; }
	const T& operator[](std::size_t i) const { return m_elements[i]; }
};
