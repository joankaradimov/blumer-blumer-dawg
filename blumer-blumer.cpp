#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "memory.hpp"
#include "nodes.hpp"
#include "dawg.hpp"

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
		int allocations_count = allocator.get_allocations_count();
		for (int i = 1; i < allocations_count; ++i)
		{
			Node<char>* ptr = allocator.get(i);
			int count = ptr->get_edge_count();
			counts[count] += 1;
		}

		assert(counts[0] == 1);
		branched_nodes = 0;
		for (int i = 2; i < 27; ++i)
		{
			branched_nodes += counts[i];
		}
	}

	void print()
	{
		for (int i = 0; i < 27; ++i)
		{
			printf("%d: %d\n", i, counts[i]);
		}
		printf("%d\n", branched_nodes + 1);
	}

private:
	int counts[27];
	int branched_nodes;
};

#include <fcntl.h>
#include <sys/stat.h>

#ifdef WIN32
#include <io.h>

int open_sequential_read(const char* const filename)
{
	int fd = open(filename, O_RDONLY | O_BINARY | O_SEQUENTIAL);
	return fd;
}
#else
#include <unistd.h>

int open_sequential_read(const char* const filename)
{
	int fd = open(filename, O_RDONLY);
	posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL | POSIX_FADV_WILLNEED);
	return fd;
}
#endif

char* read_input(const char* const filename)
{
	int input_fd = open_sequential_read(filename);

	struct stat input_file_stats;
	fstat(input_fd, &input_file_stats);
	int size = input_file_stats.st_size;

	char* result = (char*)malloc(size + 1);
	read(input_fd, result, size);
	close(input_fd);
	result[size] = 0;
	return result;
}

int main(int argc, char* argv[])
{
	// TODO: count the final state too (it has 0 children; 0 != 1)

	char* input_filename = argv[1];
	char* const content = read_input(input_filename);
	Dawg<char> dawg(content);
	free(content);

	char* verbose_arg = argv[2];
	if (argc > 2 && strcmp(verbose_arg, "-r") == 0)
	{
		NodeStatsBuilder stats;
		stats.build();
		stats.print();
	}

	int allocations = 1 +
		Allocator<PartialEdgeList<char>>::get_instance().get_allocations_count() - 1 +
		Allocator<FullEdgeMap<char, 26>>::get_instance().get_allocations_count() - 1;
	printf("%d\n", allocations);

	return 0;
}
