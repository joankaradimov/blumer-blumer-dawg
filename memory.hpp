#pragma once

template <typename T> class AllocatorPtr;
template <typename T, int chunk_size = 8 * 1024 * 1024, int max_chunks = 32> class ChunkedAllocator; // 5 + 23 = 28 bits for addressing

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
