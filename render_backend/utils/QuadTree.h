/*****************************************************************************
 * Alpine Terrain Builder
 * Copyright (C) 2022 alpinemaps.org
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#pragma once

#include <array>
#include <memory>
#include <algorithm>
#include <cassert>

using std::size_t;

// This is a quick implementation of a quad tree.
// for now it's not planned to optimise it for performance, so no move constructors or other friends.
// also, it's likely possible to do things in a way, that is better for cpu caches. this is also something that is not done.
// As many developers learned, such optimisations should be done after profiling, and right now it's more important to finish it.


template <typename DataType>
class QuadTreeNode {
  using QuadTreeNodePtr = std::unique_ptr<QuadTreeNode>;
  DataType m_data = {};
  std::array<QuadTreeNodePtr, 4> m_children;
  bool m_children_present = false;
public:

  QuadTreeNode(const DataType& data) : m_data(data) {}
  [[nodiscard]] bool hasChildren() const { return m_children_present; }
  void addChildren(const std::array<DataType, 4>& data);
  void removeChildren();
  QuadTreeNode& operator[](unsigned index);
  const QuadTreeNode& operator[](unsigned index) const;
  DataType& data() { return m_data; }
  const DataType& data() const { return m_data; }
};

template<typename DataType>
void QuadTreeNode<DataType>::addChildren(const std::array<DataType, 4>& data)
{
  assert(!m_children_present);
  m_children_present = true;
  std::transform(data.begin(), data.end(), m_children.begin(), [](const auto& data) { return std::make_unique<QuadTreeNode>(data); });
}

template<typename DataType>
void QuadTreeNode<DataType>::removeChildren()
{
  assert(m_children_present);
  m_children_present = false;
  for (auto& ch : m_children) {
    ch.reset();
  }
}

template<typename DataType>
QuadTreeNode<DataType>& QuadTreeNode<DataType>::operator[](unsigned index)
{
  return *m_children[index];
}

template<typename DataType>
const QuadTreeNode<DataType>& QuadTreeNode<DataType>::operator[](unsigned index) const
{
  return *m_children[index];
}
