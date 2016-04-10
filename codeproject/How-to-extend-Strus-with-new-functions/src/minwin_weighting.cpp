#include "minwin_weighting.hpp"
#include "positionWindow.hpp"
#include "strus/weightingFunctionInterface.hpp"
#include "strus/weightingFunctionInstanceInterface.hpp"
#include "strus/weightingFunctionContextInterface.hpp"
#include "strus/numericVariant.hpp"
#include "strus/storageClientInterface.hpp"
#include "strus/metaDataReaderInterface.hpp"
#include "strus/postingIteratorInterface.hpp"
#include "strus/queryProcessorInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include <sstream>
#include <iostream>
#include <stdexcept>

using namespace strus;

// Helper for boilerplate code catching exceptions and reporting them
// via an error buffer interface, returning an undefined value.
// The caller of the function can check for a failed operation by inspecting
// the ErrorBufferInterface passed to the object (ErrorBufferInterface::hasError()):
#define CATCH_ERROR_MAP_RETURN( HND, VALUE, MSG)\
	catch( const std::bad_alloc&)\
	{\
		(HND).report( "out of memory in minimal weighting function");\
		return VALUE;\
	}\
	catch( const std::runtime_error& err)\
	{\
		(HND).report( "%s (minimal window weighting function): %s", MSG, err.what());\
		return VALUE;\
	}
#define CATCH_ERROR_MAP( HND, MSG)\
	catch( const std::bad_alloc&)\
	{\
		(HND).report( "out of memory in minimal window weighting function");\
	}\
	catch( const std::runtime_error& err)\
	{\
		(HND).report( "%s (minimal window weighting function): %s", MSG, err.what());\
	}

class MinWinWeightingFunctionContext
	:public WeightingFunctionContextInterface
{
public:
	MinWinWeightingFunctionContext(
			ErrorBufferInterface* errhnd_, int range_, unsigned int cardinality_)
		:m_errhnd(errhnd_),m_range(range_),m_cardinality(cardinality_)
	{}

	virtual ~MinWinWeightingFunctionContext(){}

	virtual void addWeightingFeature(
			const std::string& name_,
			PostingIteratorInterface* postingIterator_,
			double /*weight_*/,
			const TermStatistics& /*stats_*/)
	{
		try
		{
			if (name_ == "match")
			{
				m_arg.push_back( postingIterator_);
			}
			else
			{
				throw std::runtime_error( "unknown weighting feature name");
			}
		}
		CATCH_ERROR_MAP( *m_errhnd, "in add weighting feature");
	}

	virtual double call( const Index& docno)
	{
		try
		{
			// Initialize the features to weight:
			std::vector<PostingIteratorInterface*>::const_iterator
				ai = m_arg.begin(), ae = m_arg.end();
			std::vector<PostingIteratorInterface*> matches;
			matches.reserve( m_arg.size());
			for (; ai != ae; ++ai)
			{
				if (docno == (*ai)->skipDoc( docno))
				{
					matches.push_back( *ai);
				}
			}
			// Calculate the minimal window size:
			PositionWindow win( matches, m_range, m_cardinality, 0);
			unsigned int minwinsize = m_range+1;
			bool more = win.first();
			for (;more; more = win.next())
			{
				unsigned int winsize = win.size();
				if (winsize < minwinsize)
				{
					minwinsize = winsize;
				}
			}
			// Return the weight depending on the minimal window size:
			if (minwinsize < (unsigned int)m_range)
			{
				return 1.0/(minwinsize+1);
			}
			else
			{
				return 0.0;
			}
		}
		CATCH_ERROR_MAP_RETURN( *m_errhnd, 0.0, "in call")
	}

private:
	ErrorBufferInterface* m_errhnd;
	std::vector<PostingIteratorInterface*> m_arg;
	int m_range;
	unsigned int m_cardinality;
};

class MinWinWeightingFunctionInstance
	:public WeightingFunctionInstanceInterface
{
public:
	explicit MinWinWeightingFunctionInstance( ErrorBufferInterface* errhnd_)
		:m_errhnd(errhnd_),m_range(1000),m_cardinality(0){}
	virtual ~MinWinWeightingFunctionInstance(){}

	virtual void addStringParameter( const std::string& name, const std::string&)
	{
		try
		{
			throw std::runtime_error( std::string( "unknown numeric parameter ") + name);
		}
		CATCH_ERROR_MAP( *m_errhnd, "in add string parameter");
	}

	virtual void addNumericParameter( const std::string& name, const NumericVariant& value)
	{
		try
		{
			if (name == "maxwinsize")
			{
				m_range = value.toint();
				if (m_range <= 0) throw std::runtime_error("illegal proximity range parameter (negative or null)");
			}
			else if (name == "cardinality")
			{
				if (value.type != NumericVariant::UInt && value.type != NumericVariant::Int)
				{
					throw std::runtime_error("illegal cardinality parameter");
				}
				m_cardinality = value.touint();
			}
			else
			{
				throw std::runtime_error( std::string( "unknown numeric parameter ") + name);
			}
		}
		CATCH_ERROR_MAP( *m_errhnd, "in add numeric parameter");
	}

	virtual WeightingFunctionContextInterface* createFunctionContext(
			const StorageClientInterface* /*storage_*/,
			MetaDataReaderInterface* /*metadata_*/,
			const GlobalStatistics& /*stats_*/) const
	{
		try
		{
			return new MinWinWeightingFunctionContext( m_errhnd, m_range, m_cardinality);
		}
		CATCH_ERROR_MAP_RETURN( *m_errhnd, 0, "in create function context");
	}

	virtual std::string tostring() const
	{
		try
		{
			std::ostringstream rt;
			rt << "maxwinsize=" << m_range << ", cardinality=" << m_cardinality;
			return rt.str();
		}
		CATCH_ERROR_MAP_RETURN( *m_errhnd, std::string(), "in tostring");
	}

private:
	ErrorBufferInterface* m_errhnd;
	std::vector<PostingIteratorInterface*> m_arg;
	int m_range;
	unsigned int m_cardinality;
};


class MinWinWeightingFunction
	:public WeightingFunctionInterface
{
public:
	MinWinWeightingFunction( ErrorBufferInterface* errhnd_)
		:m_errhnd(errhnd_)
	{}

	virtual ~MinWinWeightingFunction(){}

	virtual WeightingFunctionInstanceInterface* createInstance( const QueryProcessorInterface*) const
	{
		try
		{
			return new MinWinWeightingFunctionInstance( m_errhnd);
		}
		CATCH_ERROR_MAP_RETURN( *m_errhnd, 0, "in create instance");
	}

	virtual FunctionDescription getDescription() const
	{
		typedef FunctionDescription::Parameter P;
		FunctionDescription rt("Calculate the document weight as the inverse of the minimal window size containing a subset of the document features");
		rt( P::Feature, "match", "defines the query features to find in a window");
		rt( P::Numeric, "maxwinsize", "the maximum size of a window to search for");
		rt( P::Numeric, "cardinality", "the number of features to find at least in a window");
		return rt;
	}

private:
	ErrorBufferInterface* m_errhnd;
};

// Exported function for creating a weighting function object for calculating the inverse of the minimal window size:
WeightingFunctionInterface* createMinWinWeightingFunction(
	ErrorBufferInterface* errorhnd)
{
	try
	{
		return new MinWinWeightingFunction( errorhnd);
	}
	CATCH_ERROR_MAP_RETURN( *errorhnd, 0, "in create function");
}



