#include "window_joinop.hpp"
#include "positionWindow.hpp"
#include "strus/postingJoinOperatorInterface.hpp"
#include "strus/postingIteratorInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include <vector>

using namespace strus;

// Helper for boilerplate code catching exceptions and reporting them
// via an error buffer interface, returning an undefined value.
// The caller of the function can check for a failed operation by inspecting
// the ErrorBufferInterface passed to the object (ErrorBufferInterface::hasError()):
#define CATCH_ERROR_MAP_RETURN( HND, VALUE)\
	catch( const std::bad_alloc&)\
	{\
		(HND).report( "out of memory in window iterator");\
		return VALUE;\
	}\
	catch( const std::runtime_error& err)\
	{\
		(HND).report( "error in minwin window iterator: %s", err.what());\
		return VALUE;\
	}\

// Try to find a subset of args with a size of at least cardinality with each 
// element having the same upper bound of docno as match. Return the smallest
// fit of such an upper bound of docno as result.
// With allowEmpty it is specified, if the document returned should only be
// a candidate element, that may result in an empty set, or if it should
// only return documents with real matches.
static Index getFirstAllMatchDocnoSubset(
		std::vector<Reference< PostingIteratorInterface> >& args,
		Index docno,
		bool allowEmpty,
		std::size_t cardinality,
		std::vector<PostingIteratorInterface*>& candidate_set)
{
	if (args.empty()) return 0;
	candidate_set.clear();

	Index docno_iter = docno;
	for (;;)
	{
		std::vector<Reference< PostingIteratorInterface> >::iterator
			ai = args.begin(), ae = args.end();

		std::size_t nof_matches = 0;
		Index match_docno = 0;

		// Iterate on all arguments:
		for (; ai != ae; ++ai)
		{
			// Try to get another candidate document number:
			Index docno_next = (*ai)->skipDocCandidate( docno_iter);
			if (docno_next)
			{
				// ... if another element found:
				if (match_docno)
				{
					// ... there are elements in the current candidate:
					if (match_docno == docno_next)
					{
						// ... it is a member of the current candidate set
						candidate_set.push_back( ai->get());
						++nof_matches;
					}
					else if (match_docno > docno_next)
					{
						// ... it is smaller than the members 
						// of the current candidate set. Recreate
						// the current candidate set with the element
						// found as only member:
						candidate_set.clear();
						candidate_set.push_back( ai->get());
						match_docno = docno_next;
						nof_matches = 1;
					}
				}
				else
				{
					// ... there are no elements yet
					// in the current candidate set:
					candidate_set.clear();
					candidate_set.push_back( ai->get());
					match_docno = docno_next;
					nof_matches = 1;
				}
			}
		}
		if (nof_matches >= cardinality)
		{
			// ... we have a candidate set big enough:
			if (!allowEmpty)
			{
				// ... we need to check the arguments to be real matches:
				std::vector<PostingIteratorInterface*>::iterator
					ci = candidate_set.begin(),
					ce = candidate_set.end();
				while (ci != ce)
				{
					if (match_docno != (*ci)->skipDoc( match_docno))
					{
						--nof_matches;
						if (nof_matches < cardinality) break;
						candidate_set.erase( ci);
					}
					else
					{
						++ci;
					}
				}
				if (nof_matches < cardinality)
				{
					docno_iter = match_docno+1;
					candidate_set.clear();
					continue;
				}
			}
			return match_docno;
		}
		else if (match_docno)
		{
			docno_iter = match_docno+1;
		}
		else
		{
			break;
		}
	}
	return 0;
}

// Window iterator class:
class WindowPostingIterator
	:public PostingIteratorInterface
{
public:
	// Constructor:
	WindowPostingIterator(
			const std::vector<Reference< PostingIteratorInterface> >& args,
			unsigned int range_,
			unsigned int cardinality_,
			ErrorBufferInterface* errorhnd_)
		:m_docno(0)
		,m_posno(0)
		,m_argar(args)
		,m_documentFrequency(-1)
		,m_range(range_)
		,m_cardinality(cardinality_)
		,m_errorhnd(errorhnd_)
	{
		// Initializing the featureid:
		std::vector<Reference< PostingIteratorInterface> >::const_iterator
			ai = m_argar.begin(), ae = m_argar.end();
		for (int aidx=0; ai != ae; ++ai,++aidx)
		{
			if (aidx) m_featureid.push_back('=');
			m_featureid.append( (*ai)->featureid());
		}
		m_featureid.append( ".win");
	}

	// Destructor:
	virtual ~WindowPostingIterator(){}

	// The skip document method uses the getFirstAllMatchDocnoSubset introduced 
	// and assures with a skip position call, that the result is a real match:
	virtual Index skipDoc( const Index& docno_)
	{
		try
		{
			m_docno = getFirstAllMatchDocnoSubset(
					m_argar, docno_, false,
					m_cardinality, m_candidate_set);
			while (m_docno && skipPos(0) == 0)
			{
				m_docno = getFirstAllMatchDocnoSubset(
						m_argar, m_docno+1, false,
						m_cardinality, m_candidate_set);
			}
			return m_docno;
		}
		CATCH_ERROR_MAP_RETURN( *m_errorhnd, (m_docno=0))
	}

	// The skip document candidate method uses the getFirstAllMatchDocnoSubset introduced:
	virtual Index skipDocCandidate( const Index& docno_)
	{
		try
		{
			return m_docno=getFirstAllMatchDocnoSubset(
						m_argar, docno_, true,
						m_cardinality, m_candidate_set);
		}
		CATCH_ERROR_MAP_RETURN( *m_errorhnd, (m_docno=0))
	}

	// The skip position method uses a sliding window for finding the next match fulfilling
	// the condition of having a set with size cardinality of element withing a proximity range,
	// at a position bigger or equal to the position index passed as argument:
	virtual Index skipPos( const Index& posno_)
	{
		try
		{
			PositionWindow win( m_candidate_set, m_range, m_cardinality, posno_);
			if (!win.first()) return m_posno=0;
			return m_posno=win.pos();
		}
		CATCH_ERROR_MAP_RETURN( *m_errorhnd, (m_posno=0))
	}

	// Return a reference to the identifier built in the constructor:
	virtual const char* featureid() const
	{
		return m_featureid.c_str();
	}

	// The document frequency cannot be calculated without a fullscan of the 
	// index for all matching documents also inspecting position.
	// This is considered as too expensive. So is estimating.
	// So we return the minimum document frequency of the arguments.:
	virtual Index documentFrequency() const
	{
		try
		{
			if (m_documentFrequency < 0)
			{
				std::vector<Reference< PostingIteratorInterface> >::const_iterator
					ai = m_argar.begin(), ae = m_argar.end();
				if (ai == ae) return 0;
		
				m_documentFrequency = (*ai)->documentFrequency();
				for (++ai; ai != ae; ++ai)
				{
					Index df = (*ai)->documentFrequency();
					if (df < m_documentFrequency)
					{
						m_documentFrequency = df;
					}
				}
			}
			return m_documentFrequency;
		}
		CATCH_ERROR_MAP_RETURN( *m_errorhnd, 0)
	}

	// The frequency is calculated by iterating on all
	// elements of the current document and counting the number of positions:
	virtual unsigned int frequency()
	{
		Index idx=0;
		unsigned int rt = 0;
		for (;0!=(idx=skipPos( idx)); ++idx,++rt){}
		return rt;
	}

	// Return the current document number:
	virtual Index docno() const
	{
		return m_docno;
	}

	// Return the current position number:
	virtual Index posno() const
	{
		return m_posno;
	}

private:
	// Current document:
	Index m_docno;
	// Current position:
	Index m_posno;
	// Argument iterators:
	std::vector<Reference< PostingIteratorInterface> > m_argar;
	// Unique id of the feature expression for debugging and tracing:
	std::string m_featureid;
	// Cached Document frequency (of the rarest subexpression):
	mutable Index m_documentFrequency;
	// Required proximity range of the candicate windows:
	unsigned int m_range;
	// Required cardinality of the result:
	unsigned int m_cardinality;
	// Current set of matching document candidates:
	std::vector<PostingIteratorInterface*> m_candidate_set;
	// Buffer for error messages (for exception free interfaces):
	ErrorBufferInterface* m_errorhnd;
};

// Window iterator constructor class:
class WindowPostingJoinOperator
	:public PostingJoinOperatorInterface
{
public:
	// Constructor:
	WindowPostingJoinOperator( ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_){}

	// Destructor:
	virtual ~WindowPostingJoinOperator(){}

	// Virtual constructor of the resulting posting iterator
	virtual PostingIteratorInterface* createResultIterator(
			const std::vector<Reference<PostingIteratorInterface> >& argitrs,
			int range,
			unsigned int cardinality) const
	{
		try
		{
			if (range < 0)
			{
				throw std::runtime_error(
					"no negative range allowed for window iterator");
			}
			return new WindowPostingIterator(
					argitrs, (unsigned int)range,
					cardinality, m_errorhnd);
		}
		CATCH_ERROR_MAP_RETURN( *m_errorhnd, 0)
	}


	// Return the Description of the operator for user help and introspection:
	virtual Description getDescription() const
	{
		try
		{
			return Description( 
				"iterator on windows of a maximum size (range) "
				"containing a defined subset of features (cardinality)");
		}
		CATCH_ERROR_MAP_RETURN( *m_errorhnd, Description())
	}

private:
	ErrorBufferInterface* m_errorhnd;
};

// Exported function constructing the PostingJoinOperatorInterface realizing an 
// iterator on windows of a given maximum size:
PostingJoinOperatorInterface* strus::createWindowJoinOperator( ErrorBufferInterface* errorhnd)
{
	return new WindowPostingJoinOperator( errorhnd);
}


