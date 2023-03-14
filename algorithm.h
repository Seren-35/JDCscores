#pragma once
#include <functional>
#include <ranges>

template<class R1, class R2, class Compare>
inline bool sets_overlap(R1&& r1, R2&& r2, Compare comp) {
	auto it1 = std::ranges::begin(r1);
	auto it2 = std::ranges::begin(r2);
	auto end1 = std::ranges::end(r1);
	auto end2 = std::ranges::end(r2);
	while (it1 != end1 && it2 != end2) {
		if (comp(*it1, *it2))
			++it1;
		else if (comp(*it2, *it1))
			++it2;
		else
			return true;
	}
	return false;
}

template<class R1, class R2>
inline bool sets_overlap(R1&& r1, R2&& r2) {
	return sets_overlap(std::forward<R1>(r1), std::forward<R2>(r2), std::less {});
}
