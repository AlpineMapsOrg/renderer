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

#include <algorithm>
#include <array>
#include <cassert>
#include <memory>
#include <vector>
#include <iterator>

using std::size_t;

// This is a quick implementation of a quad tree.
// for now it's not planned to optimise it for performance, so no move constructors or other friends.
// also, it's likely possible to do things in a way, that is better for cpu caches. this is also something that is not done.
// As many developers learned, such optimisations should be done after profiling, and right now it's more important to finish it.

template <typename DataType>
class QuadTreeNode {
    using QuadTreeNodePtr = std::unique_ptr<QuadTreeNode>;
    DataType m_data = {};
    std::array<QuadTreeNodePtr, 4> m_children = {};
    bool m_children_present = false;

public:
    QuadTreeNode(const DataType& data)
        : m_data(data)
    {
    }
    [[nodiscard]] bool hasChildren() const { return m_children_present; }
    void addChildren(const std::array<DataType, 4>& data);
    void removeChildren();
    QuadTreeNode& operator[](unsigned index);
    const QuadTreeNode& operator[](unsigned index) const;
    auto begin() { return m_children.begin(); }
    auto begin() const { return m_children.begin(); }
    auto end() { return m_children.end(); }
    auto end() const { return m_children.end(); }
    DataType& data() { return m_data; }
    const DataType& data() const { return m_data; }
};

namespace quad_tree {

template <typename DataType, typename PredicateFunction, typename RefineFunction>
std::vector<DataType> onTheFlyTraverse(const DataType& root, const PredicateFunction& predicate, const RefineFunction& generate_children)
{
    if (!predicate(root)) {
        std::vector<DataType> leaves;
        leaves.push_back(root);
        return leaves;
    }
    const auto children = generate_children(root);
    std::vector<DataType> leaves;
    for (const auto& child : children) {
        const auto tmp = onTheFlyTraverse(child, predicate, generate_children);
        std::copy(tmp.begin(), tmp.end(), std::back_inserter(leaves));
    }

    return leaves;
}

template <typename DataType, typename Function>
void visit(QuadTreeNode<DataType>* root, const Function& visitor)
{
    using QuadTreeNodePtr = std::unique_ptr<QuadTreeNode<DataType>>;
    visitor(root->data());
    if (!root->hasChildren()) {
        return;
    }
    for (QuadTreeNodePtr& node : *root) {
        assert(node);
        visit(node.get(), visitor);
    }
}

template <typename DataType, typename Function>
void visitInnerNodes(QuadTreeNode<DataType>* root, const Function& visitor)
{
    using QuadTreeNodePtr = std::unique_ptr<QuadTreeNode<DataType>>;
    if (!root->hasChildren()) {
        return;
    }
    visitor(root->data());
    for (QuadTreeNodePtr& node : *root) {
        assert(node);
        visitInnerNodes(node.get(), visitor);
    }
}

template <typename DataType, typename Function>
void visitLeaves(QuadTreeNode<DataType>* root, const Function& visitor)
{
    using QuadTreeNodePtr = std::unique_ptr<QuadTreeNode<DataType>>;
    if (!root->hasChildren()) {
        visitor(root->data());
        return;
    }
    for (QuadTreeNodePtr& node : *root) {
        assert(node);
        visitLeaves(node.get(), visitor);
    }
}

template <typename DataType, typename Predicate>
std::vector<QuadTreeNode<DataType>*> collectSubtreesWithLeafCondition(QuadTreeNode<DataType>* root, const Predicate& check_leaf)
{
    if (!root->hasChildren()) {
        if (check_leaf(root->data()))
            return { root };
        return {};
    }
    auto all_leaves_match_condition = true;
    const auto visitor = [&check_leaf, &all_leaves_match_condition](const auto& node_data) { all_leaves_match_condition &= check_leaf(node_data); };
    visitLeaves(root, visitor);
    if (all_leaves_match_condition)
        return { root };

    std::vector<QuadTreeNode<DataType>*> subtrees;
    for (const auto& child : *root) {
        const auto tmp = collectSubtreesWithLeafCondition(child.get(), check_leaf);
        std::copy(tmp.begin(), tmp.end(), std::back_inserter(subtrees));
    }
    return subtrees;
}

template <typename DataType, typename PredicateFunction, typename RefineFunction>
void refine(QuadTreeNode<DataType>* root, const PredicateFunction& node_needs_refinement, const RefineFunction& generate_children)
{
    using QuadTreeNodePtr = std::unique_ptr<QuadTreeNode<DataType>>;

    if (!root->hasChildren() && node_needs_refinement(root->data()))
        root->addChildren(generate_children(root->data()));

    if (!root->hasChildren())
        return;

    for (QuadTreeNodePtr& node : *root) {
        assert(node);
        refine(node.get(), node_needs_refinement, generate_children);
    }
}

// removes all unnecessary children (i.e., if the parent doesn't need refinement).
template <typename DataType, typename PredicateFunction>
void reduce(QuadTreeNode<DataType>* root, const PredicateFunction& node_needs_refinement)
{
    using QuadTreeNodePtr = std::unique_ptr<QuadTreeNode<DataType>>;
    if (!root->hasChildren())
        return;
    auto remove_children = !node_needs_refinement(root->data());
    if (remove_children) {
        root->removeChildren();
        return;
    }
    for (QuadTreeNodePtr& node : *root) {
        assert(node);
        reduce(node.get(), node_needs_refinement);
    }
}
}

template <typename DataType>
void QuadTreeNode<DataType>::addChildren(const std::array<DataType, 4>& data)
{
    if (m_children_present)
        return;

    m_children_present = true;
    std::transform(data.begin(), data.end(), m_children.begin(), [](const auto& data) { return std::make_unique<QuadTreeNode>(data); });
}

template <typename DataType>
void QuadTreeNode<DataType>::removeChildren()
{
    if (!m_children_present)
        return;

    m_children_present = false;
    for (auto& ch : m_children) {
        ch.reset();
    }
}

template <typename DataType>
QuadTreeNode<DataType>& QuadTreeNode<DataType>::operator[](unsigned index)
{
    assert(m_children_present);
    return *m_children[index];
}

template <typename DataType>
const QuadTreeNode<DataType>& QuadTreeNode<DataType>::operator[](unsigned index) const
{
    assert(m_children_present);
    return *m_children[index];
}
