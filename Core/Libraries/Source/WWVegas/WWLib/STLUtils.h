/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <algorithm>
#include <cstddef>
#include <map>
#include <utility>

namespace stl
{

// Convenience struct to avoid the std::pair<iterator, iterator>
template <typename Container>
struct range
{
	typedef typename Container::iterator iterator;

	range(iterator b, iterator e) : begin(b), end(e) {}

	iterator get() const { return begin; }
	bool valid() const { return begin != end; }
	ptrdiff_t distance() const { return std::distance(begin, end); }

	iterator begin;
	iterator end;
};

template <typename Container>
struct const_range
{
	typedef typename Container::const_iterator iterator;

	const_range(iterator b, iterator e) : begin(b), end(e) {}
	const_range(const range<Container>& other) : begin(other.begin), end(other.end) {}

	iterator get() const { return begin; }
	bool valid() const { return begin != end; }
	ptrdiff_t distance() const { return std::distance(begin, end); }

	iterator begin;
	iterator end;
};


// Finds first matching element in vector-like container and erases it.
template <typename Container>
bool find_and_erase(Container& container, const typename Container::value_type& value)
{
	typename Container::const_iterator it = container.begin();
	for (; it != container.end(); ++it)
	{
		if (*it == value)
		{
			container.erase(it);
			return true;
		}
	}
	return false;
}

// Finds first matching element in vector-like container and removes it by swapping it with the last element.
// This is generally faster than erasing from a vector, but will change the element sorting.
template <typename Container>
bool find_and_erase_unordered(Container& container, const typename Container::value_type& value)
{
	typename Container::iterator it = container.begin();
	for (; it != container.end(); ++it)
	{
		if (*it == value)
		{
			*it = CPP_11(std::move)(container.back());
			container.pop_back();
			return true;
		}
	}
	return false;
}

// Push back value into vector-like container if it does not yet contain that value.
template <typename Container>
bool push_back_unique(Container& container, const typename Container::value_type& value)
{
	typename Container::iterator it = std::find(container.begin(), container.end(), value);
	if (it == container.end())
	{
		container.push_back(value);
		return true;
	}

	return false;
}


template <typename Iter>
Iter advance_in_range(Iter first, Iter last, ptrdiff_t n)
{
	if (n <= 0)
		return first;

	const ptrdiff_t count = std::distance(first, last);

	if (n >= count)
		return last;

	std::advance(first, n);
	return first;
}

template <typename Key, typename Val>
range<std::multimap<Key, Val>/**/> get_range(std::multimap<Key, Val>& mm, const Key& key, ptrdiff_t n = 0)
{
	typedef typename std::multimap<Key, Val>::iterator Iter;
	const std::pair<Iter, Iter> pair = mm.equal_range(key);
	const Iter it = advance_in_range(pair.first, pair.second, n);
	return range<std::multimap<Key, Val>/**/>(it, pair.second);
}

template <typename Key, typename Val>
const_range<std::multimap<Key, Val>/**/> get_range(const std::multimap<Key, Val>& mm, const Key& key, ptrdiff_t n = 0)
{
	typedef typename std::multimap<Key, Val>::const_iterator Iter;
	const std::pair<Iter, Iter> pair = mm.equal_range(key);
	const Iter it = advance_in_range(pair.first, pair.second, n);
	return const_range<std::multimap<Key, Val>/**/>(it, pair.second);
}

} // namespace stl
