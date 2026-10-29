#pragma once

#include <cstddef>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

// Middle-ground alternative to unordered-map and indexed vector
//
//  Data is unsually referenced with a index,
//  indexing is usually dense and tends around the first available indices,
//  we can store the empty values as well for cache and performance reasons

template <typename Idx, typename T>
class IndexMap {
private:
	std::vector<std::optional<T>> m_elements;
	std::size_t m_elementCount = 0;

public:
	T& operator[](Idx i) {
		if (m_elements.size() <= i) {
			m_elements.insert(m_elements.end(), i - m_elements.size() + 1, std::nullopt);
		}
		if (m_elements[i].has_value()) return *m_elements[i];
		m_elementCount++;
		m_elements[i] = T {};
		return *m_elements[i];
	}
	const T* operator[](Idx i) const {
		return i < m_elements.size() && m_elements[i].has_value() ? &(*m_elements[i]) : nullptr;
	}

	auto items() const {
		return std::views::iota(std::size_t { 0 }, m_elements.size()) |
			   std::views::filter([this](std::size_t i) { return m_elements[i].has_value(); }) |
			   std::views::transform([this](std::size_t i) {
				   return std::pair<Idx, const T&>(static_cast<Idx>(i), *m_elements[i]);
			   });
	};

	void remove(Idx id) {
		m_elementCount--;
		m_elements[id] = std::nullopt;
	}

	std::size_t size() const { return m_elementCount; }

	std::pair<Idx, T&> topIndex() {
		auto el = std::find_if(m_elements.rbegin(), m_elements.rend(), [](auto v) { return v.has_value(); });

		return std::pair<Idx, T&>(std::distance(el, m_elements.rend()) - 1, (*(*el)));
	}
};
