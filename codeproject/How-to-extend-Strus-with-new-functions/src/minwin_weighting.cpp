#include "minwin_weighting.hpp"
#include "strus/weightingFunctionInterface.hpp"
#include "strus/weightingFunctionInstanceInterface.hpp"
#include "strus/weightingFunctionContextInterface.hpp"
#include "strus/arithmeticVariant.hpp"
#include "strus/storageClientInterface.hpp"
#include "strus/metaDataReaderInterface.hpp"
#include "strus/postingIteratorInterface.hpp"
#include "strus/errorBufferInterface.hpp"

using namespace strus;

// Helper for boilerplate code catching exceptions and reporting them
// via an error buffer interface, returning an undefined value.
// The caller of the function can check for a failed operation by inspecting
// the ErrorBufferInterface passed to the object (ErrorBufferInterface::hasError()):
#define CATCH_ERROR_MAP_RETURN( HND, VALUE, MSG)\
	catch( const std::bad_alloc&)\
	{\
		(HND).report( "out of memory in window weighting function");\
		return VALUE;\
	}\
	catch( const std::runtime_error& err)\
	{\
		(HND).report( "%s (minwin window weighting function): %s", MSG, err.what());\
		return VALUE;\
	}
#define CATCH_ERROR_MAP( HND, MSG)\
	catch( const std::bad_alloc&)\
	{\
		(HND).report( "out of memory in window weighting function");\
	}\
	catch( const std::runtime_error& err)\
	{\
		(HND).report( "%s (minwin window weighting function): %s", MSG, err.what());\
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
			float /*weight_*/,
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
		CATCH_ERROR_MAP( *m_errhnd);
	}

	virtual double call( const Index& docno)
	{
		try
		{
			std::vector<PostingIteratorInterface*>::const_iterator ai = m_arg.begin(), ae = m_arg.end();
			std::vector<PostingIteratorInterface*> matches;
			matches.reserve( m_arg.size());
			for (; ai != ae; ++ai)
			{
				if (docno == ai->skipDoc( docno))
				{
					matches.push_back( *ai);
				}
			}
			PositionWindow win( matches, m_range, m_cardinality, 0);
			unsigned int maxwinsize = m_range+1;
			bool more = win.first();
			for (;more; more = win.next())
			{
				unsigned int winsize = win.size();
				if (winsize < maxwinsize)
				{
					maxwinsize = winsize;
				}
			}
			return 1.0/(maxwinsize+1);
		}
		CATCH_ERROR_MAP_RETURN( *m_errhnd, 0.0);
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
		CATCH_ERROR_MAP( *m_errhnd);
	}

	virtual void addNumericParameter( const std::string& name, const ArithmeticVariant& value)
	{
		try
		{
			if (name_ == "maxwinsize")
			{
				m_range = value.toint();
				if (m_range <= 0) throw std::runtime_error("illegal proximity range parameter (negative or null)");
			}
			else if (name_ == "cardinality")
			{
				if (value.type != ArithmeticVariant::UInt && value.type != ArithmeticVariant::Int)
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
		CATCH_ERROR_MAP( *m_errhnd);
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
		CATCH_ERROR_MAP_RETURN( *m_errhnd, 0);
	}

	virtual std::string tostring() const
	{
		try
		{
			std::ostringstream rt;
			rt << "maxwinsize=" << m_maxwinsize << ", cardinality=" << m_cardinality;
			return rt.str();
		}
		CATCH_ERROR_MAP_RETURN( *m_errorhnd, );
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

	virtual WeightingFunctionInstanceInterface* createInstance() const
	{
		try
		{
			return new MinWinWeightingFunctionInstance( m_errhnd);
		}
		CATCH_ERROR_MAP_RETURN( *m_errorhnd, 0);
	}

	virtual Description getDescription() const
	{
		
	}

private:
	ErrorBufferInterface* m_errhnd;
};

// Exported function for creating a weighting function object for calculating the inverse of the minimal window size:
WeightingFunctionInterface* createWeightingFunction(
	ErrorBufferInterface* errorhnd)
{
	try
	{
		return new MinWinWeightingFunction( errorhnd);
	}
	CATCH_ERROR_MAP_RETURN( *errorhnd, 0);
}

#endif

