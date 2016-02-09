#include "positionWindow.hpp"
#include "strus/postingJoinOperatorInterface.hpp"
#include "strus/postingIteratorInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include <cstdio>

using namespace strus;

#undef STRUS_LOWLEVEL_DEBUG

// Constructor that fills the sliding window implemented as set 
// with the argument element start positions:
PositionWindow::PositionWindow(
		const std::vector<PostingIteratorInterface*>& args,
		unsigned int range_,
		unsigned int cardinality_,
		Index firstpos_)
	:m_args(args)
	,m_setsize(0)
	,m_range(range_)
	,m_cardinality(cardinality_>0?cardinality_:args.size())
{
	std::vector<PostingIteratorInterface*>::iterator
		ai = m_args.begin(), ae = m_args.end();
	for (; ai != ae; ++ai)
	{
		PostingIteratorInterface* itr = *ai;
		Index pos = itr->skipPos( firstpos_);
		if (pos)
		{
			m_set.insert( Element( itr, pos));
			++m_setsize;
		}
	}
}

// Get the top element of the current window:
std::set<PositionWindow::Element>::iterator PositionWindow::getWinTopElement() const
{
	if (m_cardinality == 0) return m_set.end();
	unsigned int ii=0;
#ifdef STRUS_LOWLEVEL_DEBUG
	printf( "window ");
#endif
	std::set<Element>::iterator
		si = m_set.begin(), se = m_set.end();
	for (; ii<m_cardinality && si != se; ++ii,++si)
	{
#ifdef STRUS_LOWLEVEL_DEBUG
		printf( " [%s %u]", si->itr->featureid(), si->pos);
#endif
	}
#ifdef STRUS_LOWLEVEL_DEBUG
	printf( "\n");
#endif
	return --si;
}

// Check if there is any valid window:
bool PositionWindow::first()
{
	// Return, if there is valid window left:
	return m_setsize >= m_cardinality;
}

// Skip to the next window and return true, if there is one more:
bool PositionWindow::next()
{
	// Calculate how many positions we can skip with the 
	// first element without loosing any window of a size
	// within the defined proximity range:
	PostingIteratorInterface* itr = m_set.begin()->itr;
	std::set<Element>::iterator top = getWinTopElement();
#ifdef STRUS_LOWLEVEL_DEBUG
	printf( "top [%s %u] begin [%s %u]\n",
			top->itr->featureid(), top->pos,
			m_set.begin()->itr->featureid(), m_set.begin()->pos);
#endif
	Index posdiff = top->pos - m_set.begin()->pos;
	Index skipsize = posdiff > (Index)m_range ? (posdiff - (Index)m_range) : 1;
	// Change the position of the first element in the 
	// sliding window by the calculated size:
	Index pos = m_set.begin()->pos;

#ifdef STRUS_LOWLEVEL_DEBUG
	printf( "delete %s %u\n", itr->featureid(), pos);
#endif
	m_set.erase( m_set.begin());

	Index newpos = itr->skipPos( pos + skipsize);
	if (newpos)
	{
#ifdef STRUS_LOWLEVEL_DEBUG
		printf( "insert %s %u\n", itr->featureid(), newpos);
#endif
		m_set.insert( Element( itr, newpos));
	}
	else
	{
		--m_setsize;
	}
	// Return, if there is valid window left:
	return m_setsize >= m_cardinality;
}

// Return the size of the current window:
unsigned int PositionWindow::size() const
{
	std::set<Element>::iterator si = getWinTopElement();
	return si->pos - m_set.begin()->pos;
}

// Return the starting position of the current window:
unsigned int PositionWindow::pos() const
{
	return (m_setsize >= m_cardinality)?m_set.begin()->pos:0;
}

