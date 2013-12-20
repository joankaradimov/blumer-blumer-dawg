#include <stdio.h>
#include <string>
#include <vector>
#include <cassert>
#include <unordered_map>

template <typename CharType> class Node;
template <typename CharType> class Edge;
template <typename CharType> class EdgeCollection;
template <typename CharType> class EdgeIterator;

template <typename CharType> Node<CharType>* update(Node<CharType>* source, Node<CharType>* active_node, CharType letter);
template <typename CharType> Node<CharType>* split(Node<CharType>* source, Node<CharType>* active_node, Node<CharType>* new_active_node);

enum EdgeType
{
	primary, secondary,
};

template <typename CharType>
class Node
{
public:
	void add_edge(CharType label, Node<CharType>* exit_node, EdgeType type)
	{
		Edge<CharType>* edge = create_edge<CharType>(exit_node, type);
		edges.add_edge(label, edge);
	}

	Edge<CharType>* get_outgoing_edge(CharType letter)
	{
		return edges.get_edge(letter);
	}

	EdgeCollection<CharType> get_outgoing_edges()
	{
		EdgeCollection<CharType> collection;
		return collection;
	}

	Node<CharType>* suffix;

private:
	EdgeCollection<CharType> edges;
};

template <typename CharType>
class Edge
{
public:
	Edge(Node<CharType>* exit_node, EdgeType type)
		:exit_node(exit_node), type(type)
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

	Node<CharType>* get_exit_node()
	{
		return exit_node;
	}

	void set_exit_node(Node<CharType>* node)
	{
		exit_node = node;
	}

private:
	EdgeType type;
	Node<CharType>* exit_node;
};

template <typename CharType>
class EdgeIterator
{
public:
	Edge<CharType> operator*()
	{
		Edge<CharType>* x;
		return *x; // TODO
	}

	EdgeIterator operator++()
	{
		return *this; // TODO
	}

	bool operator!=(const EdgeIterator<CharType>& iter) const
	{
		return true; // TODO
	}
};

template <typename CharType>
class EdgeCollection
{
public:
	typedef std::unordered_map<CharType, Edge<CharType>*> EdgeCollectionType;

	EdgeIterator<CharType> begin()
	{
		EdgeIterator<CharType> x;
		return x; // TODO
	}

	EdgeIterator<CharType> end()
	{
		EdgeIterator<CharType> x;
		return x; // TODO
	}

	Edge<CharType>* get_edge(CharType letter)
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

	void add_edge(CharType letter, Edge<CharType>* edge)
	{
		edges[letter] = edge;
	}
	
//private: TODO
	EdgeCollectionType edges;
};

template <typename CharType>
Node<CharType> *create_node()
{
	return new Node<CharType>; // TODO
}

template <typename CharType>
Edge<CharType> *create_edge(Node<CharType>* exit_node, EdgeType type)
{
	return new Edge<CharType>(exit_node, type); // TODO
}

template <typename CharType>
Node<CharType>* build_dawg(std::basic_string<CharType> word)
{
	Node<CharType>* source = create_node<CharType>();
	source->suffix = NULL;
	Node<CharType>* active_node = source;
	for (CharType letter : word)
	{
		active_node = update(source, active_node, letter);
	}
	return source;
}

/*
builddawg(S)
1. Create a node named source.
2. Let activenode be source.
3. For each word w of S do:*
A.For each letter a of w do :
Let activenode be update(activenode, a).
B.Let activenode be source.
4. Return source.
*/

template <typename CharType>
Node<CharType>* update(Node<CharType>* source, Node<CharType>* active_node, CharType letter)
{
	Node<CharType>* new_active_node = create_node<CharType>();
	active_node->add_edge(letter, new_active_node, EdgeType::primary);
	Node<CharType>* current_node = active_node;
	Node<CharType>* suffix_node = NULL;

	while (current_node != source && suffix_node == NULL)
	{
		current_node = current_node->suffix;
		Edge<CharType>* outgoing_edge = current_node->get_outgoing_edge(letter);
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

/*
update (activenode, a)
  1. If activenode has an outgoing edge labeled a, then*
    A. Let newactivenode be the node that this edge leads to.*
    B. If this edge is primary, return newactivenode.*
    C. Else, return split (activenode, newactivenode).*
  2. Else
    A. Create a node named newactivenode.
    B. Create a primary edge labeled a from activenode to newactivenode.
    C. Let currentnode be activenode.
    D. Let suflxnode be undefined.
    E. While currentnode isn’t source and sufixnode is undefined do:
        i. Let currentnode be the node pointed to by the sufftx pointer
           of currentnode.
       ii. If currentnode has a primary outgoing edge labeled a,
           then let sufixnode be the node that this edge leads to.
      iii. Else, if currentnode has a secondary outgoing edge labeled a then
             a. Let childnode be the node that this edge leads to.
             b. Let suffixnode be split (currentnode, childnode).
       iv. Else, create a secondary edge from currentnode to
           newactivenode labeled a. 
    F. If sufixnode is still undefined, let suffixnode be source.
    G. Set the suffix pointer of newactivenode to point to sufixnode.
    H. Return newactivenode.
*/

template <typename CharType>
Node<CharType>* split(Node<CharType>* source, Node<CharType>* parent_node, CharType label)
{
	Node<CharType>* new_child_node = create_node<CharType>();
	Edge<CharType>* outgoing_edge = parent_node->get_outgoing_edge(label);
	Node<CharType>* child_node = outgoing_edge->get_exit_node();

	assert(outgoing_edge->get_type() == EdgeType::secondary);
	outgoing_edge->set_type(EdgeType::primary);
	outgoing_edge->set_exit_node(new_child_node);
	for (std::pair<CharType, Edge<CharType>*> pair : child_node->get_outgoing_edges().edges) // TODO: fix this mess
	{
		CharType label = pair.first;
		Edge<CharType>* edge = pair.second;
		new_child_node->add_edge(label, edge->get_exit_node(), EdgeType::secondary);
	}
	new_child_node->suffix = child_node->suffix;
	child_node->suffix = new_child_node;
	Node<CharType>* current_node = parent_node;
	while (current_node != source)
	{
		current_node = current_node->suffix;
		Edge<CharType>* edge = current_node->get_outgoing_edge(label);
		if (edge->get_type() == EdgeType::secondary)
		{
			assert(edge->get_exit_node() == child_node);
			edge->set_exit_node(new_child_node);
		}
		else
		{
			break;
		}
	}
	return new_child_node;
}

/*
split (parentnode, childnode)
  1. Create a node called newchildnode.
  2. Make the secondary edge from parentnode to childnode into a primary
     edge from parentnode to newchildnode (with the same label).
  3. For every primary and secondary outgoing edge of childnode, create a
     secondary outgoing edge of newchildnode with the same label and
     leading to the same node.
  4. Set the suffix pointer of newchildnode equal to that of childnode.
  5. Reset the suffix pointer of childnode to point to newchildnode.
  6. Let currentnode be parentnode.
  7. While currentnode isn’t source do:
    A. Let currentnode be the node pointed to by the suffrx pointer
       of currentnode.
    B. If currentnode has a secondary edge to childnode, then make it
       a secondary edge to newchildnode (with the same label).
    C. Else, break out of the while loop.
  8. Return newchildnode.
*/

int main(int argc, char* argv[])
{
	// TODO: count the final state too (it has 0 children; 0 != 1)

	std::string word = "asdasdasdasqwezxc";
	build_dawg<char>(word);
	return 0;
}
