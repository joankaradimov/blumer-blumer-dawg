#pragma once

#include "memory.hpp"
#include "nodes.hpp"

template <typename CharType>
class Dawg
{
public:
	Dawg(std::basic_string<CharType> word) : source_ptr(Node<CharType>::create())
	{
		Node<CharType>& source = *source_ptr;
		source.set_suffix(0);
		AllocatorPtr<Node<CharType>> active_node = source_ptr;
		for (CharType letter : word)
		{
			letter &= 0x1f; // TODO: a nicer solution to this
			active_node = update(active_node, letter);
		}
	}

private:
	AllocatorPtr<Node<CharType>> update(AllocatorPtr<Node<CharType>> active_node_ptr, CharType letter)
	{
		const AllocatorPtr<Node<CharType>> new_active_node = Node<CharType>::create();
		Node<CharType>& active_node = *active_node_ptr;
		active_node.add_edge(letter, new_active_node, EdgeType::primary);
		AllocatorPtr<Node<CharType>> current_node_ptr = active_node_ptr;
		AllocatorPtr<Node<CharType>> suffix_node = 0;

		while (current_node_ptr != source_ptr && suffix_node == 0)
		{
			current_node_ptr = current_node_ptr->get_suffix();
			Node<CharType>& current_node = *current_node_ptr;
			const Edge<CharType> outgoing_edge = current_node.get_outgoing_edge(letter);
			if (outgoing_edge.is_present() == false)
			{
				current_node.add_edge(letter, new_active_node, EdgeType::secondary);
			}
			else if (outgoing_edge.get_type() == EdgeType::primary)
			{
				suffix_node = outgoing_edge.get_exit_node();
			}
			else // (outgoing_edge.get_type() == EdgeType::secondary)
			{
				suffix_node = split(current_node_ptr, letter);
			}
		}
		if (suffix_node == 0)
		{
			suffix_node = source_ptr;
		}
		new_active_node->set_suffix(suffix_node);
		return new_active_node;
	}

	AllocatorPtr<Node<CharType>> split(AllocatorPtr<Node<CharType>> parent_node_ptr, CharType label)
	{
		const AllocatorPtr<Node<CharType>> new_child_node_ptr = Node<CharType>::create();
		Node<CharType>& new_child_node = *new_child_node_ptr;
		Node<CharType>& parent_node = *parent_node_ptr;
		const Edge<CharType> outgoing_edge = parent_node.get_outgoing_edge(label);
		const AllocatorPtr<Node<CharType>> child_node_ptr = outgoing_edge.get_exit_node();
		Node<CharType>& child_node = *child_node_ptr;

		assert(outgoing_edge.get_type() == EdgeType::secondary);
		parent_node.set_outgoing_edge_props(label, EdgeType::primary, new_child_node_ptr);
		new_child_node.add_secondary_edges(child_node);
		new_child_node.set_suffix(child_node.get_suffix());
		child_node.set_suffix(new_child_node_ptr);

		AllocatorPtr<Node<CharType>> current_node_ptr = parent_node_ptr;
		while (current_node_ptr != source_ptr)
		{
			current_node_ptr = current_node_ptr->get_suffix();
			Node<CharType>& current_node = *current_node_ptr;
			const Edge<CharType> edge = current_node.get_outgoing_edge(label);
			if (edge.is_present() && edge.get_exit_node() == child_node_ptr)
			{
				assert(edge.get_type() == EdgeType::secondary);
				current_node.set_outgoing_edge_props(label, EdgeType::secondary, new_child_node_ptr);
			}
			else
			{
				break;
			}
		}
		return new_child_node_ptr;
	}

	const AllocatorPtr<Node<CharType>> source_ptr;
};
