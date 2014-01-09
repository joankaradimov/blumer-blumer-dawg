#include <string>
#include <cassert>
#include <fstream>

template <typename T> class AllocatorPtr;
template <typename T, int chunk_size = 8 * 1024 * 1024, int max_chunks = 32> class ChunkedAllocator; // 5 + 23 = 28 bits for addressing
template <typename CharType> class Node;
template <typename CharType> class Edge;
template <typename CharType> class LabeledEdge;
template <typename CharType> class SingleEdgeCollection;
template <typename CharType, int max_list_size = 3> class PartialEdgeList;
template <typename CharType, int alphabet_size = 26> class FullEdgeMap;

enum EdgeType
{
	primary, secondary,
};

#define Allocator ChunkedAllocator

template <typename T>
class AllocatorPtr
{
public:
	AllocatorPtr(int data) : data(data) {}

	void free()
	{
		assert(data);
		return Allocator<T>::get_instance().free(data);
	}

	int operator==(const AllocatorPtr<T> other) const
	{
		return data == other.data;
	}

	int operator!=(const AllocatorPtr<T> other) const
	{
		return data != other.data;
	}

	T* operator->() const
	{
		assert(data);
		return Allocator<T>::get_instance().get(data);
	}

	T& operator*() const
	{
		assert(data);
		return *Allocator<T>::get_instance().get(data);
	}

	bool not_null() { return data; }

	int to_int() const
	{
		return data;
	}

	bool is_valid() const
	{
		return Allocator<T>::get_instance().is_valid(data);
	}
private:
	int data;
};

template <typename T, int chunk_size, int max_chunks>
class ChunkedAllocator
{
public:
	friend class NodeStatsBuilder;

	const AllocatorPtr<T> alloc()
	{
		if (free_list_head)
		{
			const AllocatorPtr<T> result = free_list_head;
			T* chunk = get(free_list_head);
			free_list_head = *((int*)chunk);
			return result;
		}
		if (counter >= chunk_size)
		{
			assert(chunk_counter < max_chunks);
			memory_chunks[chunk_counter] = (T*)malloc(chunk_size * sizeof(T));
			counter = 0;
			++chunk_counter;
		}
		const AllocatorPtr<T> result = allocations_count();
		++counter;
		return result;
	}

	void free(AllocatorPtr<T> ptr)
	{
		int* item = (int*) get(ptr.to_int());
		*item = free_list_head;
		free_list_head = ptr.to_int();
	}

	T* get(int index) const
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

	static ChunkedAllocator<T, chunk_size, max_chunks>& get_instance()
	{
		static ChunkedAllocator<T, chunk_size, max_chunks> instance;
		return instance;
	}
private:
	ChunkedAllocator() : counter(chunk_size), chunk_counter(0), free_list_head(0)
	{
		alloc(); // create a NULL pointer for this allocator
	}

	~ChunkedAllocator()
	{
		for (int i = 0; i < chunk_counter; i++)
		{
			::free(memory_chunks[i]);
		}
	}

	int allocations_count() const
	{
		int filled_chunk_count = chunk_counter - 1;
		return filled_chunk_count * chunk_size + counter;
	}

	int counter;
	int chunk_counter;
	T* memory_chunks[max_chunks];
	int free_list_head;
};

template <typename CharType>
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

	Node()
		: edge_collection_type(EdgeCollectionType::single_node), outgoing_edge_label(0)
	{
	}

	~Node()
	{
		if (edge_collection_type == EdgeCollectionType::full_edge_map)
		{
			Allocator<FullEdgeMap<CharType>>& allocator = Allocator<FullEdgeMap<CharType>>::get_instance();
			ptr_to_full_edge_map().~FullEdgeMap<CharType>();
			allocator.free(ptr);
		}
		else if (edge_collection_type == EdgeCollectionType::partial_edge_list)
		{
			Allocator<PartialEdgeList<CharType>>& allocator = Allocator<PartialEdgeList<CharType>>::get_instance();
			ptr_to_partial_edge_list().~PartialEdgeList<CharType>();
			allocator.free(ptr);
		}
	}

	int get_edge_count()
	{
		if (edge_collection_type == EdgeCollectionType::single_node)
		{
			return ptr_to_single_edge_collection().size();
		}
		else if (edge_collection_type == EdgeCollectionType::partial_edge_list)
		{
			return ptr_to_partial_edge_list().size();
		}
		else // (edge_collection_type == EdgeCollectionType::full_edge_map)
		{
			return ptr_to_full_edge_map().size();
		}
	}

	void add_edge(CharType label, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		if (edge_collection_type == EdgeCollectionType::single_node)
		{
			SingleEdgeCollection<CharType>& edges = ptr_to_single_edge_collection();
			if (edges.is_full())
			{
				edge_collection_type = EdgeCollectionType::partial_edge_list;
				AllocatorPtr<PartialEdgeList<CharType>> new_edges_ptr = PartialEdgeList<CharType>::create();
				PartialEdgeList<CharType>& new_edges = *new_edges_ptr;
				new_edges.add_edge(outgoing_edge_label, ptr, (EdgeType)outgoing_edge_type);
				ptr = new_edges_ptr.to_int();
				new_edges.add_edge(label, exit_node, type);
			}
			else
			{
				edges.add_edge(label, exit_node, type);
			}
		}
		else if (edge_collection_type == EdgeCollectionType::partial_edge_list)
		{
			PartialEdgeList<CharType>& edges = ptr_to_partial_edge_list();
			if (edges.is_full())
			{
				edge_collection_type = EdgeCollectionType::full_edge_map;
				AllocatorPtr<FullEdgeMap<CharType>> new_edges_ptr = FullEdgeMap<CharType>::create();
				AllocatorPtr<PartialEdgeList<CharType>> old_ptr = ptr;
				ptr = new_edges_ptr.to_int();
				FullEdgeMap<CharType>& new_edges = *new_edges_ptr;
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
		else // (edge_collection_type == EdgeCollectionType::full_edge_map)
		{
			ptr_to_full_edge_map().add_edge(label, exit_node, type);
		}
	}

	void add_secondary_edges(const Node<CharType>& node)
	{
		if (node.edge_collection_type == EdgeCollectionType::single_node)
		{
			this->add_edge(node.outgoing_edge_label, node.ptr, EdgeType::secondary);
		}
		else if (node.edge_collection_type == EdgeCollectionType::partial_edge_list)
		{
			const PartialEdgeList<CharType>& edges = node.ptr_to_partial_edge_list();
			for (const LabeledEdge<CharType> edge : edges)
			{
				this->add_edge(edge.label, edge.exit_node_ptr, EdgeType::secondary);
			}
		}
		else // (node.edge_collection_type == EdgeCollectionType::full_edge_map)
		{
			const FullEdgeMap<CharType>& edges = node.ptr_to_full_edge_map();
			for (const LabeledEdge<CharType> edge : edges)
			{
				this->add_edge(edge.label, edge.exit_node_ptr, EdgeType::secondary);
			}
		}
	}

	void set_outgoing_edge_props(CharType label, EdgeType edge_type, AllocatorPtr<Node<CharType>> exit_node)
	{
		if (edge_collection_type == EdgeCollectionType::single_node)
		{
			ptr_to_single_edge_collection().set_edge_props(label, exit_node, edge_type);
		}
		else if (edge_collection_type == EdgeCollectionType::partial_edge_list)
		{
			ptr_to_partial_edge_list().set_edge_props(label, exit_node, edge_type);
		}
		else // (edge_collection_type == EdgeCollectionType::full_edge_map)
		{
			ptr_to_full_edge_map().set_edge_props(label, exit_node, edge_type);
		}
	}

	const Edge<CharType> get_outgoing_edge(CharType letter)
	{
		if (edge_collection_type == EdgeCollectionType::single_node)
		{
			return ptr_to_single_edge_collection().get_edge(letter);
		}
		else if (edge_collection_type == EdgeCollectionType::partial_edge_list)
		{
			return ptr_to_partial_edge_list().get_edge(letter);
		}
		else // (edge_collection_type == EdgeCollectionType::full_edge_map)
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

	SingleEdgeCollection<CharType>& ptr_to_single_edge_collection()
	{
		assert(edge_collection_type == EdgeCollectionType::single_node);
		return * reinterpret_cast<SingleEdgeCollection<CharType>*>(this);
	}

	PartialEdgeList<CharType>& ptr_to_partial_edge_list() const
	{
		assert(edge_collection_type == EdgeCollectionType::partial_edge_list);
		AllocatorPtr<PartialEdgeList<CharType>> result_ptr = ptr;
		return *result_ptr;
	}

	FullEdgeMap<CharType>& ptr_to_full_edge_map() const
	{
		assert(edge_collection_type == EdgeCollectionType::full_edge_map);
		AllocatorPtr<FullEdgeMap<CharType>> result_ptr = ptr;
		return *result_ptr;
	}
protected:
	enum EdgeCollectionType
	{
		single_node,
		partial_edge_list,
		full_edge_map,
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
	}

	const AllocatorPtr<Node<CharType>> exit_node_ptr;
	const EdgeType type;
	const CharType label;
};

template <typename CharType>
class SingleEdgeCollection : protected Node<CharType>
{
public:
	const Edge<CharType> get_edge(CharType letter) const
	{
		if (this->outgoing_edge_label == letter)
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
		this->outgoing_edge_label = letter;
		this->ptr = exit_node.to_int();
		this->outgoing_edge_type = type;
	}

	void set_edge_props(CharType letter, AllocatorPtr<Node<CharType>> exit_node, EdgeType type)
	{
		assert(letter == outgoing_edge_label);
		this->outgoing_edge_type = type;
		this->ptr = exit_node.to_int();
	}

	int size() const
	{
		return this->outgoing_edge_label != 0;
	}

	bool is_full() const
	{
		return this->outgoing_edge_label != 0;
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
			return (edge_list->edges + index) != (other.edge_list->edges + other.index);
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
		Iterator(const Edge<CharType>* edge, int index) : first_edge(edge), index(index)
		{
		}

		bool operator!=(const Iterator other) const
		{
			return (first_edge + index) != (other.first_edge + other.index);
		}

		const Iterator& operator++()
		{
			do
			{
				++index;
			} while (index < alphabet_size && !first_edge[index].is_present());
			return *this;
		}

		const LabeledEdge<CharType> operator*() const
		{
			return LabeledEdge<CharType>(first_edge[index], index + 1);
		}
	private:
		const Edge<CharType>* const first_edge;
		int index;
	};

	Iterator begin() const
	{
		return Iterator(edges, 0);
	}

	Iterator end() const
	{
		return Iterator(edges, alphabet_size);
	}

private:
	Edge<CharType> edges[alphabet_size];
};

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
		const Allocator<Node<char>>& allocator = Allocator<Node<char>>::get_instance();
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
	Dawg<char> dawg(content);

	NodeStatsBuilder stats;
	stats.build();
	stats.print();

	return 0;
}
