#pragma once

#include "memory.hpp"

template <typename CharType, int alphabet_size = 26> class Node;
template <typename CharType> class Edge;
template <typename CharType> class LabeledEdge;
template <typename CharType> class EmptyEdgeCollection;
template <typename CharType> class SingleEdgeCollection;
template <typename CharType, int max_list_size = 3> class PartialEdgeList;
template <typename CharType, int alphabet_size> class FullEdgeMap;

enum EdgeType
{
	primary, secondary,
};

template <typename CharType, int alphabet_size>
class Node
{
public:
	static AllocatorPtr<Node<CharType>> create()
	{
		Allocator<Node<CharType>>& allocator = Allocator<Node<CharType>>::get_instance();
		AllocatorPtr<Node<CharType>> result = allocator.alloc();
		new(allocator.get(result.to_int())) Node<CharType>; // TODO: overload new, maybe
		return result;
	}

	Node() : ptr_type(0)
	{
	}

	~Node()
	{
		if (is_of_type(EdgeCollectionType::full_edge_map))
		{
			Allocator<FullEdgeMap<CharType, alphabet_size>>& allocator = Allocator<FullEdgeMap<CharType, alphabet_size>>::get_instance();
			ptr_to_full_edge_map().~FullEdgeMap<CharType, alphabet_size>();
			allocator.free(ptr);
		}
		else if (is_of_type(EdgeCollectionType::partial_edge_list))
		{
			Allocator<PartialEdgeList<CharType, alphabet_size>>& allocator = Allocator<PartialEdgeList<CharType>>::get_instance();
			ptr_to_partial_edge_list().~PartialEdgeList<CharType>();
			allocator.free(ptr);
		}
	}

	int get_edge_count()
	{
		if (is_of_type(EdgeCollectionType::empty_edge_collection))
		{
			return ptr_to_empty_edge_collection().size();
		}
		else if (is_of_type(EdgeCollectionType::single_node))
		{
			return ptr_to_single_edge_collection().size();
		}
		else if (is_of_type(EdgeCollectionType::partial_edge_list))
		{
			return ptr_to_partial_edge_list().size();
		}
		else // (is_of_type(EdgeCollectionType::full_edge_map))
		{
			return ptr_to_full_edge_map().size();
		}
	}

	void add_edge(CharType label, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		if (is_of_type(EdgeCollectionType::empty_edge_collection))
		{
			ptr_to_empty_edge_collection().add_edge(label, exit_node, type);
			assert(is_of_type(EdgeCollectionType::single_node));
		}
		else if (is_of_type(EdgeCollectionType::single_node))
		{
			AllocatorPtr<PartialEdgeList<CharType>> new_edges_ptr = PartialEdgeList<CharType>::create();
			PartialEdgeList<CharType>& new_edges = *new_edges_ptr;
			new_edges.add_edge(ptr_type, ptr, (EdgeType)outgoing_edge_type);
			set_edge_collection_type(EdgeCollectionType::partial_edge_list);
			ptr = new_edges_ptr.to_int();
			new_edges.add_edge(label, exit_node, type);
		}
		else if (is_of_type(EdgeCollectionType::partial_edge_list))
		{
			PartialEdgeList<CharType>& edges = ptr_to_partial_edge_list();
			if (edges.is_full())
			{
				set_edge_collection_type(EdgeCollectionType::full_edge_map);
				AllocatorPtr<FullEdgeMap<CharType, alphabet_size>> new_edges_ptr = FullEdgeMap<CharType, alphabet_size>::create();
				AllocatorPtr<PartialEdgeList<CharType>> old_ptr = ptr;
				ptr = new_edges_ptr.to_int();
				FullEdgeMap<CharType, alphabet_size>& new_edges = *new_edges_ptr;
				for (const LabeledEdge<CharType> edge : edges)
				{
					new_edges.add_edge(edge.label, edge.exit_node_ptr, edge.type);
				}
				new_edges.add_edge(label, exit_node, type);
				old_ptr.free();
			}
			else
			{
				edges.add_edge(label, exit_node, type);
			}
		}
		else // (is_of_type(EdgeCollectionType::full_edge_map))
		{
			ptr_to_full_edge_map().add_edge(label, exit_node, type);
		}
	}

	void add_secondary_edges(const Node<CharType>& node)
	{
		if (node.is_of_type(EdgeCollectionType::empty_edge_collection))
		{
			// Do nothing
		}
		else if (node.is_of_type(EdgeCollectionType::single_node))
		{
			this->add_edge(node.ptr_type, node.ptr, EdgeType::secondary);
		}
		else if (node.is_of_type(EdgeCollectionType::partial_edge_list))
		{
			const PartialEdgeList<CharType>& edges = node.ptr_to_partial_edge_list();
			for (const LabeledEdge<CharType> edge : edges)
			{
				this->add_edge(edge.label, edge.exit_node_ptr, EdgeType::secondary);
			}
		}
		else // (node.is_of_type(EdgeCollectionType::full_edge_map))
		{
			const FullEdgeMap<CharType, alphabet_size>& edges = node.ptr_to_full_edge_map();
			for (const LabeledEdge<CharType> edge : edges)
			{
				this->add_edge(edge.label, edge.exit_node_ptr, EdgeType::secondary);
			}
		}
	}

	void set_outgoing_edge_props(CharType label, EdgeType edge_type, AllocatorPtr<Node<CharType>> exit_node)
	{
		if (is_of_type(EdgeCollectionType::empty_edge_collection))
		{
			ptr_to_empty_edge_collection().set_edge_props(label, exit_node, edge_type);
		}
		else if (is_of_type(EdgeCollectionType::single_node))
		{
			ptr_to_single_edge_collection().set_edge_props(label, exit_node, edge_type);
		}
		else if (is_of_type(EdgeCollectionType::partial_edge_list))
		{
			ptr_to_partial_edge_list().set_edge_props(label, exit_node, edge_type);
		}
		else // (is_of_type(EdgeCollectionType::full_edge_map))
		{
			ptr_to_full_edge_map().set_edge_props(label, exit_node, edge_type);
		}
	}

	const Edge<CharType> get_outgoing_edge(CharType letter)
	{
		if (is_of_type(EdgeCollectionType::empty_edge_collection))
		{
			return ptr_to_empty_edge_collection().get_edge(letter);
		}
		else if (is_of_type(EdgeCollectionType::single_node))
		{
			return ptr_to_single_edge_collection().get_edge(letter);
		}
		else if (is_of_type(EdgeCollectionType::partial_edge_list))
		{
			return ptr_to_partial_edge_list().get_edge(letter);
		}
		else // (is_of_type(EdgeCollectionType::full_edge_map))
		{
			return ptr_to_full_edge_map().get_edge(letter);
		}
	}

	void set_suffix(AllocatorPtr<Node<CharType>> suffix)
	{
		this->suffix = suffix.to_int();
	}

	AllocatorPtr<Node<CharType>> get_suffix()
	{
		return suffix;
	}

	EmptyEdgeCollection<CharType>& ptr_to_empty_edge_collection()
	{
		assert(is_of_type(EdgeCollectionType::empty_edge_collection));
		return *reinterpret_cast<EmptyEdgeCollection<CharType>*>(this);
	}

	SingleEdgeCollection<CharType>& ptr_to_single_edge_collection()
	{
		assert(is_of_type(EdgeCollectionType::single_node));
		return *reinterpret_cast<SingleEdgeCollection<CharType>*>(this);
	}

	PartialEdgeList<CharType>& ptr_to_partial_edge_list() const
	{
		assert(is_of_type(EdgeCollectionType::partial_edge_list));
		AllocatorPtr<PartialEdgeList<CharType>> result_ptr = ptr;
		return *result_ptr;
	}

	FullEdgeMap<CharType, alphabet_size>& ptr_to_full_edge_map() const
	{
		assert(is_of_type(EdgeCollectionType::full_edge_map));
		AllocatorPtr<FullEdgeMap<CharType, alphabet_size>> result_ptr = ptr;
		return *result_ptr;
	}
protected:
	enum EdgeCollectionType
	{
		empty_edge_collection,
		single_node = alphabet_size,
		partial_edge_list,
		full_edge_map,
	};

	bool is_of_type(EdgeCollectionType type) const
	{
		switch (type)
		{
		case EdgeCollectionType::single_node:
			return ptr_type > 0 && ptr_type <= alphabet_size;
		default:
			return ptr_type == type;
		}
	}

	void set_edge_collection_type(EdgeCollectionType type)
	{
		ptr_type = type;
	}

	unsigned long long suffix : 28;
	unsigned long long ptr_type : 5;
	unsigned long long outgoing_edge_type : 1;
	unsigned long long ptr : 28;
};

template <typename CharType>
class Edge
{
public:
	Edge()
		:exists(false)
	{
	}

	Edge(AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
		:exists(true), type(type), exit_node_ptr(exit_node.to_int())
	{
	}

	EdgeType get_type() const
	{
		return (EdgeType)type;
	}

	void set_type(EdgeType type)
	{
		this->type = type;
	}

	AllocatorPtr<Node<CharType>> get_exit_node() const
	{
		AllocatorPtr<Node<CharType>> result = exit_node_ptr;
		assert(result.is_valid());
		return result;
	}

	void set_exit_node(AllocatorPtr<Node<CharType>> node)
	{
		exit_node_ptr = node.to_int();
	}

	bool is_present() const
	{
		return exists;
	}

	static const Edge<CharType> non_existant()
	{
		static const Edge<CharType> non_existant_edge;
		return non_existant_edge;
	}
private:
	unsigned int exists : 1;
	unsigned int type : 1;
	unsigned int exit_node_ptr : 28;
};

template <typename CharType>
class LabeledEdge
{
public:
	LabeledEdge(Edge<CharType> edge, CharType label)
		: exit_node_ptr(edge.get_exit_node()), type(edge.get_type()), label(label)
	{
		assert(edge.get_exit_node().is_valid());
	}

	const AllocatorPtr<Node<CharType>> exit_node_ptr;
	const EdgeType type;
	const CharType label;
};

template <typename CharType>
class EmptyEdgeCollection : private Node<CharType>
{
public:
	const Edge<CharType> get_edge(CharType letter) const
	{
		return Edge<CharType>::non_existant();
	}

	void add_edge(CharType letter, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		this->ptr_type = letter;
		this->ptr = exit_node.to_int();
		this->outgoing_edge_type = type;
	}

	void set_edge_props(CharType letter, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		assert(false);
	}

	int size() const
	{
		return 0;
	}

	bool is_full() const
	{
		return false;
	}
};

template <typename CharType>
class SingleEdgeCollection : private Node<CharType>
{
public:
	const Edge<CharType> get_edge(CharType letter) const
	{
		if (this->ptr_type == letter)
		{
			EdgeType type = (EdgeType)this->outgoing_edge_type;
			return Edge<CharType>(this->ptr, type);
		}
		else
		{
			return Edge<CharType>::non_existant();
		}
	}

	void add_edge(CharType letter, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		assert(false);
	}

	void set_edge_props(CharType letter, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		assert(this->ptr_type == letter);
		this->outgoing_edge_type = type;
		this->ptr = exit_node.to_int();
	}

	int size() const
	{
		return 1;
	}

	bool is_full() const
	{
		return true;
	}
};

template <typename CharType, int max_list_size>
class PartialEdgeList
{
public:
	static AllocatorPtr<PartialEdgeList<CharType, max_list_size>> create()
	{
		Allocator<PartialEdgeList<CharType, max_list_size>>& allocator = Allocator<PartialEdgeList<CharType, max_list_size>>::get_instance();
		AllocatorPtr<PartialEdgeList<CharType, max_list_size>> result = allocator.alloc();
		new(allocator.get(result.to_int())) PartialEdgeList<CharType, max_list_size>; // TODO: overload new, maybe
		return result;
	}

	PartialEdgeList()
	{
		// TODO: move this to the class that handles the array
		for (int i = 0; i < max_list_size + 1; i++)
		{
			label_data[i] = 0;
		}
	}

	const Edge<CharType> get_edge(CharType letter) const
	{
		int edge_index = get_edge_index(letter);
		if (edge_index != -1)
		{
			return edges[edge_index];
		}
		else
		{
			return Edge<CharType>::non_existant();
		}
	}

	void add_edge(CharType letter, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		assert(!is_full());
		assert(get_edge_index(letter) == -1);

		int current_size = size();
		label_data[current_size] = letter;
		edges[current_size] = Edge<CharType>(exit_node, type);
		increment_size();
	}

	void set_edge_props(CharType letter, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		int edge_index = get_edge_index(letter);
		assert(edge_index != -1);

		edges[edge_index] = Edge<CharType>(exit_node, type);
	}

	int size() const
	{
		return label_data[max_list_size];
	}

	bool is_full() const
	{
		return size() == max_list_size;
	}

	friend class Iterator;

	class Iterator
	{
	public:
		Iterator(const PartialEdgeList<CharType, max_list_size>* edges, int index) : edge_list(edges), index(index)
		{
		}

		bool operator!=(const Iterator other) const
		{
			assert(edge_list == other.edge_list);
			return index != other.index;
		}

		const Iterator& operator++()
		{
			++index;
			return *this;
		}

		const LabeledEdge<CharType> operator*() const
		{
			return LabeledEdge<CharType>(edge_list->edges[index], edge_list->label_data[index]);
		}
	private:
		const PartialEdgeList<CharType, max_list_size>* edge_list;
		int index;
	};

	Iterator begin() const
	{
		return Iterator(this, 0);
	}

	Iterator end() const
	{
		return Iterator(this, size());
	}
private:
	int get_edge_index(CharType label) const
	{
		for (int i = 0; i < max_list_size; i++)
		{
			if (label_data[i] == label)
			{
				return i;
			}
		}
		return -1;
	}

	void increment_size()
	{
		label_data[max_list_size] += 1;
	}

	// TODO: test alignment and waste of memory
	// label_data[0 ... max_list_size-1] contains the taken labels
	// label_data[0 ... max_list_size] contains the number of taken labels
	unsigned char label_data[max_list_size + 1]; // TODO: make these a bit field array (5 bits each)
	Edge<CharType> edges[max_list_size];
};

template <typename CharType, int alphabet_size>
class FullEdgeMap
{
public:
	static AllocatorPtr<FullEdgeMap<CharType, alphabet_size>> create()
	{
		Allocator<FullEdgeMap<CharType, alphabet_size>>& allocator = Allocator<FullEdgeMap<CharType, alphabet_size>>::get_instance();
		AllocatorPtr<FullEdgeMap<CharType, alphabet_size>> result = allocator.alloc();
		new(allocator.get(result.to_int())) FullEdgeMap<CharType, alphabet_size>; // TODO: overload new, maybe
		return result;
	}

	FullEdgeMap()
	{
	}

	const Edge<CharType> get_edge(CharType letter) const
	{
		return edges[letter - 1];
	}

	void add_edge(CharType letter, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		assert(edges[letter - 1].is_present() == false);
		edges[letter - 1] = Edge<CharType>(exit_node, type);
	}

	void set_edge_props(CharType letter, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		assert(edges[letter - 1].is_present());
		edges[letter - 1] = Edge<CharType>(exit_node, type);
	}

	int size()
	{
		int result = 0;
		for (int i = 0; i < alphabet_size; i++)
		{
			result += edges[i].is_present();
		}
		return result;
	}

	class Iterator
	{
	public:
		Iterator(const Edge<CharType>* edges, int index) : edges(edges), index(index)
		{
		}

		bool operator!=(const Iterator other) const
		{
			assert(edges == other.edges);
			return index != other.index;
		}

		const Iterator& operator++()
		{
			do
			{
				++index;
			} while (index < alphabet_size && !edges[index].is_present());
			return *this;
		}

		const LabeledEdge<CharType> operator*() const
		{
			return LabeledEdge<CharType>(edges[index], index + 1);
		}
	private:
		const Edge<CharType>* const edges;
		int index;
	};

	Iterator begin() const
	{
		Iterator result(edges, -1);
		++result;
		return result;
	}

	Iterator end() const
	{
		return Iterator(edges, alphabet_size);
	}

private:
	Edge<CharType> edges[alphabet_size];
};
