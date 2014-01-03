#include <string>
#include <cassert>
#include <unordered_map>
#include <fstream>

template <typename T> class AllocatorPtr;
template <typename T, int chunk_size = 8 * 1024 * 1024, int max_chunks = 32> class SimpleAllocator; // 5 + 23 = 28 bits for addressing
template <typename CharType> class Node;
template <typename CharType> class Edge;
template <typename CharType> class EdgeHashCollection;
template <typename CharType> class EdgeIterator;

template <typename CharType> Node<CharType>* update(Node<CharType>* source, Node<CharType>* active_node, CharType letter);
template <typename CharType> Node<CharType>* split(Node<CharType>* source, Node<CharType>* active_node, Node<CharType>* new_active_node);

enum EdgeType
{
	primary, secondary,
};

template <typename T>
class AllocatorPtr
{
public:
	AllocatorPtr() : AllocatorPtr(0) {} // TODO: get rid of this constructor
	AllocatorPtr(int data) : data(data) {}

	int operator/(int x) { return data / x; }
	int operator%(int x) { return data % x; }
	int operator==(const AllocatorPtr<T>& other) { return data == other.data; }
	int operator!=(const AllocatorPtr<T>& other) { return data != other.data; }

	T* operator->()
	{
		assert(data);
		return SimpleAllocator<T>::get_instance().get(data);
	}

	T& operator*()
	{
		assert(data);
		return *SimpleAllocator<T>::get_instance().get(data);
	}

	bool not_null() { return data; }

	int to_int()
	{
		return data;
	}

	bool is_valid()
	{
		return SimpleAllocator<T>::get_instance().is_valid(data);
	}
private:
	int data;
};

template <typename T, int chunk_size, int max_chunks>
class SimpleAllocator
{
public:
	friend class NodeStatsBuilder;

	AllocatorPtr<T> alloc()
	{
		if (counter >= chunk_size)
		{
			assert(chunk_counter < max_chunks);
			memory_chunks[chunk_counter] = (T*)malloc(chunk_size * sizeof(T));
			counter = 0;
			++chunk_counter;
		}
		T* top_chunk = memory_chunks[chunk_counter - 1];
		AllocatorPtr<T> result = allocations_count();
		++counter;
		return result;
	}

	T* get(int index)
	{
		int chunk_index = index / chunk_size;
		int inner_index = index % chunk_size;
		T* chunk = memory_chunks[chunk_index];
		return chunk + inner_index;
	}

	bool is_valid(int index)
	{
		int chunk_index = index / chunk_size;
		int inner_index = index % chunk_size;
		return chunk_index < chunk_counter && inner_index < counter;
	}

	static SimpleAllocator<T>& get_instance()
	{
		static SimpleAllocator<T> instance;
		return instance;
	}
private:
	SimpleAllocator() : counter(chunk_size), chunk_counter(0)
	{
		alloc(); // create a NULL pointer for this allocator
	}

	~SimpleAllocator()
	{
		for (int i = 0; i < chunk_counter; i++)
		{
			free(memory_chunks[i]);
		}
	}

	int allocations_count()
	{
		int filled_chunk_count = chunk_counter - 1;
		return filled_chunk_count * chunk_size + counter;
	}

	int counter;
	int chunk_counter;
	T* memory_chunks[max_chunks];
};

template <typename CharType>
class Node
{
public:
	static AllocatorPtr<Node<CharType>> create()
	{
		SimpleAllocator<Node<CharType>>& allocator = SimpleAllocator<Node<CharType>>::get_instance();
		AllocatorPtr<Node<CharType>> result = allocator.alloc();
		new(allocator.get(result.to_int())) Node<CharType>; // TODO: overload new, maybe
		return result;
	}

	Node():
		edge_collection_type(EdgeCollectionType::single_node), outgoing_edge_label(0)
	{
	}

	~Node()
	{
		if (edge_collection_type == EdgeCollectionType::edges_hash)
		{
			// TODO
		}
	}

	int get_edge_count()
	{
		if (edge_collection_type == EdgeCollectionType::single_node)
		{
			return outgoing_edge_label != 0;
		}
		else if (edge_collection_type == EdgeCollectionType::edges_hash)
		{
			return ptr_as_edges_hash()->edges.size();
		}
		else
		{
			assert(false);
		}
	}

	void add_edge(CharType label, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		if (edge_collection_type == EdgeCollectionType::single_node)
		{
			if (outgoing_edge_label == 0)
			{
				outgoing_edge_label = label;
				ptr = exit_node.to_int();
				outgoing_edge_type = type;
			}
			else
			{
				edge_collection_type = EdgeCollectionType::edges_hash;
				AllocatorPtr<EdgeHashCollection<CharType>> edges_ptr = EdgeHashCollection<CharType>::create();
				edges_ptr->add_edge(outgoing_edge_label, ptr, (EdgeType) outgoing_edge_type);
				ptr = edges_ptr.to_int();
				edges_ptr->add_edge(label, exit_node, type);
			}
		}
		else if (edge_collection_type == EdgeCollectionType::edges_hash)
		{
			ptr_as_edges_hash()->add_edge(label, exit_node, type);
		}
	}

	void add_secondary_edges(AllocatorPtr<Node<CharType>> node)
	{
		if (node->edge_collection_type == EdgeCollectionType::single_node)
		{
			this->add_edge(node->outgoing_edge_label, node->ptr, EdgeType::secondary);
		}
		else if (node->edge_collection_type == EdgeCollectionType::edges_hash)
		{
			AllocatorPtr<EdgeHashCollection<CharType>> edges = node->ptr_as_edges_hash();
			for (std::pair<CharType, AllocatorPtr<Edge<CharType>>> pair : edges->edges) // TODO: fix this mess
			{
				CharType label = pair.first;
				AllocatorPtr<Edge<CharType>> edge = pair.second;
				this->add_edge(label, edge->get_exit_node(), EdgeType::secondary);
			}
		}
	}

	void set_outgoing_edge_props(CharType label, EdgeType edge_type, AllocatorPtr<Node<CharType>> exit_node)
	{
		if (edge_collection_type == EdgeCollectionType::single_node)
		{
			assert(label == outgoing_edge_label);
			outgoing_edge_type = edge_type;
			ptr = exit_node.to_int();
		}
		else if (edge_collection_type == EdgeCollectionType::edges_hash)
		{
			AllocatorPtr<EdgeHashCollection<CharType>> edges = ptr_as_edges_hash();
			AllocatorPtr<Edge<CharType>> edge = edges->get_edge(label);
			edge->set_type(edge_type);
			edge->set_exit_node(exit_node);
		}
		else
		{
			assert(false);
		}
	}

	const Edge<CharType> get_outgoing_edge(CharType letter)
	{
		if (edge_collection_type == EdgeCollectionType::single_node)
		{
			if (outgoing_edge_label == letter)
			{
				AllocatorPtr<Node<CharType>> exit_node = ptr_as_node();
				EdgeType type = (EdgeType) outgoing_edge_type;
				const Edge<CharType> result(exit_node, type);
				return result;
			}
			else
			{
				return Edge<CharType>::non_existant();
			}
		}
		else if (edge_collection_type == EdgeCollectionType::edges_hash)
		{
			AllocatorPtr<EdgeHashCollection<CharType>> edges = ptr_as_edges_hash();
			AllocatorPtr<Edge<CharType>> edge = edges->get_edge(letter);
			if (edge.not_null())
			{
				return *edge;
			}
			else
			{
				return Edge<CharType>::non_existant();
			}
		}
		else
		{
			assert(false);
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

	AllocatorPtr<Node<CharType>> ptr_as_node()
	{
		assert(edge_collection_type == EdgeCollectionType::single_node);
		return ptr;
	}

	AllocatorPtr<EdgeHashCollection<CharType>> ptr_as_edges_hash()
	{
		assert(edge_collection_type == EdgeCollectionType::edges_hash);
		return ptr;
	}
private:
	enum EdgeCollectionType
	{
		single_node,
		edges_hash,
	};

	unsigned long long suffix : 28;
	unsigned long long edge_collection_type : 2;
	unsigned long long outgoing_edge_label : 5;
	unsigned long long outgoing_edge_type : 1;
	unsigned long long ptr : 28;
};

template <typename CharType>
class Edge
{
public:
	static AllocatorPtr<Edge<CharType>> create(AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		SimpleAllocator<Edge<CharType>>& allocator = SimpleAllocator<Edge<CharType>>::get_instance();
		AllocatorPtr<Edge<CharType>> result = allocator.alloc();
		new(allocator.get(result.to_int()))Edge<CharType>(exit_node, type); // TODO
		return result;
	}

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
		return (EdgeType) type;
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

	static Edge<CharType> non_existant()
	{
		static Edge non_existant_edge;
		return non_existant_edge;
	}
private:
	unsigned int exists : 1;
	unsigned int type : 1;
	unsigned int exit_node_ptr : 28;
};

template <typename CharType>
class EdgeHashCollection
{
public:
	static AllocatorPtr<EdgeHashCollection<CharType>> create()
	{
		SimpleAllocator<EdgeHashCollection<CharType>>& allocator = SimpleAllocator<EdgeHashCollection<CharType>>::get_instance();
		AllocatorPtr<EdgeHashCollection<CharType>> result = allocator.alloc();
		new(allocator.get(result.to_int())) EdgeHashCollection<CharType>; // TODO: overload new, maybe
		return result;
	}

	AllocatorPtr<Edge<CharType>> get_edge(CharType letter)
	{
		try
		{
			return edges.at(letter);
		}
		catch (std::out_of_range)
		{
			return NULL;
		}
	}

	void add_edge(CharType letter, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		AllocatorPtr<Edge<CharType>> edge = Edge<CharType>::create(exit_node, type);
		edges[letter] = edge;
	}
	
// private: TODO
	std::unordered_map<CharType, AllocatorPtr<Edge<CharType>>> edges;
};

template <typename CharType>
AllocatorPtr<Node<CharType>> build_dawg(std::basic_string<CharType> word)
{
	AllocatorPtr<Node<CharType>> source = Node<CharType>::create();
	source->set_suffix(NULL);
	AllocatorPtr<Node<CharType>> active_node = source;
	for (CharType letter : word)
	{
		letter &= 0x1f; // TODO: a nicer solution to this
		active_node = update(source, active_node, letter);
	}
	return source;
}

template <typename CharType>
AllocatorPtr<Node<CharType>> update(AllocatorPtr<Node<CharType>> source, AllocatorPtr<Node<CharType>> active_node, CharType letter)
{
	AllocatorPtr<Node<CharType>> new_active_node = Node<CharType>::create();
	active_node->add_edge(letter, new_active_node, EdgeType::primary);
	AllocatorPtr<Node<CharType>> current_node = active_node;
	AllocatorPtr<Node<CharType>> suffix_node = NULL;

	while (current_node != source && suffix_node == NULL)
	{
		current_node = current_node->get_suffix();
		const Edge<CharType> outgoing_edge = current_node->get_outgoing_edge(letter);
		if (outgoing_edge.is_present() == false)
		{
			current_node->add_edge(letter, new_active_node, EdgeType::secondary);
		}
		else if (outgoing_edge.get_type() == EdgeType::primary)
		{
			suffix_node = outgoing_edge.get_exit_node();
		}
		else // (outgoing_edge.get_type() == EdgeType::secondary)
		{
			suffix_node = split(source, current_node, letter);
		}
	}
	if (suffix_node == NULL)
	{
		suffix_node = source;
	}
	new_active_node->set_suffix(suffix_node);
	return new_active_node;
}

template <typename CharType>
AllocatorPtr<Node<CharType>> split(AllocatorPtr<Node<CharType>> source, AllocatorPtr<Node<CharType>> parent_node, CharType label)
{
	AllocatorPtr<Node<CharType>> new_child_node = Node<CharType>::create();
	const Edge<CharType> outgoing_edge = parent_node->get_outgoing_edge(label);
	AllocatorPtr<Node<CharType>> child_node = outgoing_edge.get_exit_node();

	assert(outgoing_edge.get_type() == EdgeType::secondary);
	parent_node->set_outgoing_edge_props(label, EdgeType::primary, new_child_node);
	new_child_node->add_secondary_edges(child_node);
	new_child_node->set_suffix(child_node->get_suffix());
	child_node->set_suffix(new_child_node);

	AllocatorPtr<Node<CharType>> current_node = parent_node;
	while (current_node != source)
	{
		current_node = current_node->get_suffix();
		const Edge<CharType> edge = current_node->get_outgoing_edge(label);
		if (edge.is_present() && edge.get_exit_node() == child_node)
		{
			assert(edge.get_type() == EdgeType::secondary);
			current_node->set_outgoing_edge_props(label, EdgeType::secondary, new_child_node);
		}
		else
		{
			break;
		}
	}
	return new_child_node;
}

class NodeStatsBuilder
{
public:
	typedef Node<char> NodeT;

	void build()
	{
		for (int i = 0; i < 27; ++i)
		{
			counts[i] = 0;
		}
		SimpleAllocator<Node<char>>& allocator = SimpleAllocator<Node<char>>::get_instance();
		int allocations_count = allocator.allocations_count();
		for (int i = 1; i < allocations_count; ++i)
		{
			Node<char>* ptr = allocator.get(i);
			int count = ptr->get_edge_count();
			counts[count] += 1;
		}
	}

	void print()
	{
		for (int i = 0; i < 27; ++i)
		{
			printf("%d: %d\n", i, counts[i]);
		}
	}

private:
	int counts[27];
};

int main(int argc, char* argv[])
{
	// TODO: count the final state too (it has 0 children; 0 != 1)

	char* input_filename = argv[1];
	std::ifstream ifs(input_filename);
	std::string content((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));
	build_dawg<char>(content);

	NodeStatsBuilder stats;
	stats.build();
	stats.print();

	return 0;
}
