#ifndef _STRUS_POSITION_WINDOW_HPP_INCLUDED
#define _STRUS_POSITION_WINDOW_HPP_INCLUDED
#include "strus/index.hpp"
#include "strus/postingIteratorInterface.hpp"
#include <set>

namespace strus {

// Sliding window to iterate on the sets of posting iterator elements 
// that are withing a defined proximity range:
class PositionWindow
{
public:
	// Element in the sliding position window implemented as set:
	struct Element
	{
		PostingIteratorInterface* itr;	// ... reference this element belongs to
		Index pos;			// ... position of this element

		Element()
			:itr(0),pos(0){}
		Element( PostingIteratorInterface* itr_, Index pos_)
			:itr(itr_),pos(pos_){}
		Element( const Element& o)
			:itr(o.itr),pos(o.pos){}

		bool operator < ( const Element& o) const
		{
			if (pos == o.pos)
			{
				return itr < o.itr;
			}
			return pos < o.pos;
		}
	};

	// Constructor that fills the sliding window implemented as set 
	// with the argument element start positions:
	PositionWindow( 
		const std::vector<PostingIteratorInterface*>& args,
		unsigned int range_,
		unsigned int cardinality_,
		Index firstpos_);

	// Check if there is any valid window:
	bool first();

	// Skip to the next window and return true, if there is one more:
	bool next();

	// Return the size of the current window:
	unsigned int size() const;

	// Return the starting position of the current window:
	unsigned int pos() const;

private:
	// Get the top element of the current window:
	std::set<Element>::iterator getWinTopElement();

private:
	std::vector<PostingIteratorInterface*> m_args;
	std::set<Element> m_set;	// ... set used for sliding window
	unsigned int m_setsize;		// ... current size of m_set
	unsigned int m_range;		// ... maximum proximity range
	unsigned int m_cardinality;	// ... number of elements for a candidate window
};

}//namespace
#endif

