#include <string>
#include <cassert>
#include <fstream>

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
