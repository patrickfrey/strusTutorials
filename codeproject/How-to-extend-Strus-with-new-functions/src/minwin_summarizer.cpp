#include "minwin_summarizer.hpp"
#include "positionWindow.hpp"
#include "strus/summarizerFunctionInterface.hpp"
#include "strus/summarizerFunctionInstanceInterface.hpp"
#include "strus/summarizerFunctionContextInterface.hpp"
#include "strus/numericVariant.hpp"
#include "strus/storageClientInterface.hpp"
#include "strus/metaDataReaderInterface.hpp"
#include "strus/postingIteratorInterface.hpp"
#include "strus/forwardIteratorInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/reference.hpp"
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
		(HND).report( "out of memory in minimal window summarizer function");\
		return VALUE;\
	}\
	catch( const std::runtime_error& err)\
	{\
		(HND).report( "%s (minimal window summarizer function): %s", MSG, err.what());\
		return VALUE;\
	}
#define CATCH_ERROR_MAP( HND, MSG)\
	catch( const std::bad_alloc&)\
	{\
		(HND).report( "out of memory in minimal window summarizer function");\
	}\
	catch( const std::runtime_error& err)\
	{\
		(HND).report( "%s (minimal window summarizer function): %s", MSG, err.what());\
	}


class MinWinSummarizerFunctionContext
	:public SummarizerFunctionContextInterface
{
public:
	MinWinSummarizerFunctionContext(
			ErrorBufferInterface* errorhnd_,
			const StorageClientInterface* storage_,
			int range_,
			unsigned int cardinality_,
			const std::string& type_)
		:m_errhnd(errorhnd_)
		,m_forwardindex(storage_->createForwardIterator( type_))
		,m_range(range_)
		,m_cardinality(cardinality_)
	{}

	virtual ~MinWinSummarizerFunctionContext(){}

	virtual void addSummarizationFeature(
			const std::string& name_,
			PostingIteratorInterface* postingIterator_,
			const std::vector<SummarizationVariable>& /*variables_*/,
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
				throw std::runtime_error( "unknown summarization feature name");
			}
		}
		CATCH_ERROR_MAP( *m_errhnd, "in add summarization feature");
	}

	virtual std::vector<SummaryElement> getSummary( const Index& docno)
	{
		try
		{
			// Initialize the features to weight and the forward index:
			m_forwardindex->skipDoc( docno);
			std::vector<SummaryElement> rt;
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
			// Calculate the minimal window size and position:
			PositionWindow win( matches, m_range, m_cardinality, 0);
			unsigned int minwinsize = m_range+1;
			Index minwinpos = 0;
			bool more = win.first();
			for (;more; more = win.next())
			{
				unsigned int winsize = win.size();
				if (winsize < minwinsize)
				{
					minwinsize = winsize;
					minwinpos = win.pos();
				}
			}
			// Build the summary phrase and append it to the result:
			if (minwinsize < (unsigned int)m_range)
			{
				std::string text;
				Index pos = minwinpos;
				while (minwinsize)
				{
					if (pos == m_forwardindex->skipPos( pos))
					{
						if (!text.empty()) text.push_back( ' ');
						text.append( m_forwardindex->fetch());
					}
					else
					{
						text.append( "..");
					}
					--minwinsize;
					++pos;
				}
				rt.push_back( SummaryElement( "minwin", text));
			}
			return rt;
		}
		CATCH_ERROR_MAP_RETURN( *m_errhnd, std::vector<SummaryElement>(), "in call");
	}

private:
	ErrorBufferInterface* m_errhnd;
	Reference<ForwardIteratorInterface> m_forwardindex;
	std::vector<PostingIteratorInterface*> m_arg;
	int m_range;
	unsigned int m_cardinality;
};


class MinWinSummarizerFunctionInstance
	:public SummarizerFunctionInstanceInterface
{
public:
	MinWinSummarizerFunctionInstance( ErrorBufferInterface* errhnd_)
		:m_errhnd(errhnd_),m_range(1000),m_cardinality(0),m_type(){}

	virtual ~MinWinSummarizerFunctionInstance(){}

	virtual void addStringParameter( const std::string& name, const std::string& value)
	{
		try
		{
			if (name == "type")
			{
				m_type = value;
			}
			else
			{
				throw std::runtime_error( std::string( "unknown string parameter ") + name);
			}
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
				if (m_range <= 0)
				{
					throw std::runtime_error("proximity range parameter negative or null");
				}
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

	virtual SummarizerFunctionContextInterface* createFunctionContext(
			const StorageClientInterface* storage_,
			MetaDataReaderInterface* /*metadata_*/,
			const GlobalStatistics& /*stats*/) const
	{
		try
		{
			return new MinWinSummarizerFunctionContext(
					m_errhnd, storage_, m_range, m_cardinality, m_type);
		}
		CATCH_ERROR_MAP_RETURN( *m_errhnd, 0, "in create function context");
	}

	virtual std::string tostring() const
	{
		try
		{
			std::ostringstream rt;
			rt << "type=" << m_type << ", maxwinsize=" << m_range << ", cardinality=" << m_cardinality;
			return rt.str();
		}
		CATCH_ERROR_MAP_RETURN( *m_errhnd, std::string(), "in tostring");
	}

private:
	ErrorBufferInterface* m_errhnd;
	std::vector<PostingIteratorInterface*> m_arg;
	int m_range;
	unsigned int m_cardinality;
	std::string m_type;
};


class MinWinSummarizerFunction
	:public SummarizerFunctionInterface
{
public:
	MinWinSummarizerFunction( ErrorBufferInterface* errhnd_)
		:m_errhnd(errhnd_)
	{}

	virtual ~MinWinSummarizerFunction(){}

	virtual SummarizerFunctionInstanceInterface* createInstance(
			const QueryProcessorInterface* /*processor*/) const
	{
		try
		{
			return new MinWinSummarizerFunctionInstance( m_errhnd);
		}
		CATCH_ERROR_MAP_RETURN( *m_errhnd, 0, "in create instance");
	}

	virtual FunctionDescription getDescription() const
	{
		typedef FunctionDescription::Parameter P;
		FunctionDescription rt("Get the passage of the forward index inside the "
				"minimal window containing a subset of the document features");
		rt( P::Feature, "match", "defines the query features to find in a window");
		rt( P::Numeric, "maxwinsize", "the maximum size of a window to search for");
		rt( P::Numeric, "cardinality", "the number of features to find at least in a window");
		rt( P::String, "type", "forward index feature type for building the result");
		return rt;
	}

private:
	ErrorBufferInterface* m_errhnd;
};


// Exported function for creating a weighting function object for calculating the inverse of the minimal window size:
SummarizerFunctionInterface* createMinWinSummarizerFunction(
	ErrorBufferInterface* errorhnd)
{
	try
	{
		return new MinWinSummarizerFunction( errorhnd);
	}
	CATCH_ERROR_MAP_RETURN( *errorhnd, 0, "in create function");
}

