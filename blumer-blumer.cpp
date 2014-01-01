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
	AllocatorPtr() {}
	AllocatorPtr(int data) : data(data) {}
	int operator/(int x) { return data / x; }
	int operator%(int x) { return data % x; }
	int operator==(const AllocatorPtr<T>& other) { return data == other.data; }
	int operator!=(const AllocatorPtr<T>& other) { return data != other.data; }

	T* operator->()
	{
		return SimpleAllocator<T>::get_instance().get(data);
	}

	bool not_null() { return data;  }
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

	int get_edge_count()
	{
		return edges.edges.size();
	}

	void add_edge(CharType label, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		edges.add_edge(label, exit_node, type);
	}

	void add_secondary_edges(AllocatorPtr<Node<CharType>> node)
	{
		for (std::pair<CharType, AllocatorPtr<Edge<CharType>>> pair : node->edges.edges) // TODO: fix this mess
		{
			CharType label = pair.first;
			AllocatorPtr<Edge<CharType>> edge = pair.second;
			this->add_edge(label, edge->get_exit_node(), EdgeType::secondary);
		}
	}

	AllocatorPtr<Edge<CharType>> get_outgoing_edge(CharType letter)
	{
		return edges.get_edge(letter);
	}

	AllocatorPtr<Node<CharType>> suffix;
private:
	EdgeHashCollection<CharType> edges;
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

	Edge(AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
		:type(type), exit_node(exit_node)
	{
	}

	EdgeType get_type()
	{
		return type;
	}

	void set_type(EdgeType type)
	{
		this->type = type;
	}

	AllocatorPtr<Node<CharType>> get_exit_node()
	{
		return exit_node;
	}

	void set_exit_node(AllocatorPtr<Node<CharType>> node)
	{
		exit_node = node;
	}
private:
	EdgeType type;
	AllocatorPtr<Node<CharType>> exit_node;
};

template <typename CharType>
class EdgeHashCollection
{
public:
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
	source->suffix = NULL;
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
		current_node = current_node->suffix;
		AllocatorPtr<Edge<CharType>> outgoing_edge = current_node->get_outgoing_edge(letter);
		if (outgoing_edge == NULL)
		{
			current_node->add_edge(letter, new_active_node, EdgeType::secondary);
		}
		else if (outgoing_edge->get_type() == EdgeType::primary)
		{
			suffix_node = outgoing_edge->get_exit_node();
		}
		else // (outgoing_edge->get_type() == EdgeType::secondary)
		{
			suffix_node = split(source, current_node, letter);
		}
	}
	if (suffix_node == NULL)
	{
		suffix_node = source;
	}
	new_active_node->suffix = suffix_node;
	return new_active_node;
}

template <typename CharType>
AllocatorPtr<Node<CharType>> split(AllocatorPtr<Node<CharType>> source, AllocatorPtr<Node<CharType>> parent_node, CharType label)
{
	AllocatorPtr<Node<CharType>> new_child_node = Node<CharType>::create();
	AllocatorPtr<Edge<CharType>> outgoing_edge = parent_node->get_outgoing_edge(label);
	AllocatorPtr<Node<CharType>> child_node = outgoing_edge->get_exit_node();

	assert(outgoing_edge->get_type() == EdgeType::secondary);
	outgoing_edge->set_type(EdgeType::primary);
	outgoing_edge->set_exit_node(new_child_node);
	new_child_node->add_secondary_edges(child_node);
	new_child_node->suffix = child_node->suffix;
	child_node->suffix = new_child_node;

	AllocatorPtr<Node<CharType>> current_node = parent_node;
	while (current_node != source)
	{
		current_node = current_node->suffix;
		AllocatorPtr<Edge<CharType>> edge = current_node->get_outgoing_edge(label);
		if (edge.not_null() && edge->get_exit_node() == child_node)
		{
			assert(edge->get_type() == EdgeType::secondary);
			edge->set_exit_node(new_child_node);
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
		for (int i = 0; i < 26; ++i)
		{
			counts[i] = 0;
		}
		SimpleAllocator<Node<char>>& allocator = SimpleAllocator<Node<char>>::get_instance();
		int allocations_count = allocator.allocations_count();
		for (int i = 0; i < allocations_count; ++i)
		{
			Node<char>* ptr = allocator.get(i);
			int count = ptr->get_edge_count();
			counts[count] += 1;
		}
	}

	void print()
	{
		for (int i = 0; i < 26; ++i)
		{
			printf("%d: %d\n", i, counts[i]);
		}
	}

private:
	int counts[26];
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
