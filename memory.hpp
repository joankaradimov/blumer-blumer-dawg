#pragma once

template <typename T> class AllocatorPtr;
template <typename T, int chunk_size = 8 * 1024 * 1024, int max_chunks = 64> class ChunkedAllocator; // 6 + 23 = 29 bits for addressing

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
			++allocations_count;
			return result;
		}
		if (allocations_count % chunk_size == 0)
		{
			assert(chunk_counter < max_chunks);
			memory_chunks[chunk_counter] = (T*)malloc(chunk_size * sizeof(T));
			++chunk_counter;
		}
		const AllocatorPtr<T> result = allocations_count;
		++allocations_count;
		return result;
	}

	void free(AllocatorPtr<T> ptr)
	{
		--allocations_count;
		int* item = (int*)get(ptr.to_int());
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
		return chunk_index < chunk_counter && index <= allocations_count;
	}

	static ChunkedAllocator<T, chunk_size, max_chunks>& get_instance()
	{
		static ChunkedAllocator<T, chunk_size, max_chunks> instance;
		return instance;
	}

	int get_allocations_count() const
	{
		return allocations_count;
	}
private:
	ChunkedAllocator()
		: chunk_counter(0), free_list_head(0), allocations_count(0)
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

	int chunk_counter;
	int free_list_head;
	int allocations_count;
	T* memory_chunks[max_chunks];
};
