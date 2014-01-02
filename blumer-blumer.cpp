#include <string>
#include <vector>
#include <cassert>
#include <unordered_map>
#include <fstream>

template <typename T> class AllocatorPtr;
template <typename T, int chunk_size = 10 * 1024 * 1024> class SimpleAllocator;
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
	AllocatorPtr() : data(0) {} // TODO: get rid of this constructor
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

	bool not_null() { return data;  }

	int to_int()
	{
		return data;
	}
private:
	int data;
};

template <typename T, int chunk_size>
class SimpleAllocator
{
public:
	friend class NodeStatsBuilder;

	AllocatorPtr<T> alloc()
	{
		if (counter >= chunk_size)
		{
			T* new_chunk = (T*) malloc(chunk_size * sizeof(T));
			counter = 0;
			memory_chunks.push_back(new_chunk);
		}
		T* top_chunk = memory_chunks.back();
		AllocatorPtr<T> result = allocations_count();
		++counter;
		return result;
	}

	T* get(AllocatorPtr<T> index)
	{
		int chunk_index = index / chunk_size;
		int inner_index = index % chunk_size;
		T* chunk = memory_chunks[chunk_index];
		return chunk + inner_index;
	}

	static SimpleAllocator<T>& get_instance()
	{
		static SimpleAllocator<T> instance;
		return instance;
	}
private:
	SimpleAllocator()
	{
		counter = chunk_size;
		alloc(); // create a NULL pointer for this allocator
	}

	~SimpleAllocator()
	{
		while (memory_chunks.empty() == false)
		{
			T* chunk = memory_chunks.back();
			memory_chunks.pop_back();
			free(chunk);
		}
	}

	int allocations_count()
	{
		int filled_chunk_count = memory_chunks.size() - 1;
		return filled_chunk_count * chunk_size + counter;
	}

	int counter;
	std::vector<T*> memory_chunks; // TODO: get rid of std::vector
};

template <typename CharType>
class Node
{
public:
	static AllocatorPtr<Node<CharType>> create()
	{
		SimpleAllocator<Node<CharType>>& allocator = SimpleAllocator<Node<CharType>>::get_instance();
		AllocatorPtr<Node<CharType>> result = allocator.alloc();
		new(allocator.get(result)) Node<CharType>; // TODO: overload new, maybe
		return result;
	}

	Node()
	{
		AllocatorPtr<EdgeHashCollection<CharType>> edges_hash_ptr = EdgeHashCollection<CharType>::create();
		ptr = edges_hash_ptr.to_int();
	}

	~Node()
	{
	}

	int get_edge_count()
	{
		AllocatorPtr<EdgeHashCollection<CharType>> edges = get_ptr_value<EdgeHashCollection<CharType>>();
		return edges->edges.size();
	}

	void add_edge(CharType label, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		AllocatorPtr<EdgeHashCollection<CharType>> edges = get_ptr_value<EdgeHashCollection<CharType>>();
		edges->add_edge(label, exit_node, type);
	}

	void add_secondary_edges(AllocatorPtr<Node<CharType>> node)
	{
		AllocatorPtr<EdgeHashCollection<CharType>> edges = node->get_ptr_value<EdgeHashCollection<CharType>>();
		for (std::pair<CharType, AllocatorPtr<Edge<CharType>>> pair : edges->edges) // TODO: fix this mess
		{
			CharType label = pair.first;
			AllocatorPtr<Edge<CharType>> edge = pair.second;
			this->add_edge(label, edge->get_exit_node(), EdgeType::secondary);
		}
	}

	void set_outgoing_edge_props(CharType label, EdgeType edge_type, AllocatorPtr<Node<CharType>> exit_node)
	{
		AllocatorPtr<EdgeHashCollection<CharType>> edges = get_ptr_value<EdgeHashCollection<CharType>>();
		AllocatorPtr<Edge<CharType>> edge = edges->get_edge(label);
		edge->set_type(edge_type);
		edge->set_exit_node(exit_node);
	}

	const Edge<CharType> get_outgoing_edge(CharType letter)
	{
		AllocatorPtr<EdgeHashCollection<CharType>> edges = get_ptr_value<EdgeHashCollection<CharType>>();
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

	void set_suffix(AllocatorPtr<Node<CharType>> suffix)
	{
		this->suffix = suffix.to_int();
	}

	AllocatorPtr<Node<CharType>> get_suffix()
	{
		return AllocatorPtr<Node<CharType>>(suffix);
	}
private:

	template <typename T> AllocatorPtr<T> get_ptr_value()
	{
		AllocatorPtr<T> allocator_ptr = ptr;
		return ptr;
	}

	unsigned long long suffix : 28;
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
		new(allocator.get(result))Edge<CharType>(exit_node, type); // TODO
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
		return exit_node_ptr;
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
		new(allocator.get(result)) EdgeHashCollection<CharType>; // TODO: overload new, maybe
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
