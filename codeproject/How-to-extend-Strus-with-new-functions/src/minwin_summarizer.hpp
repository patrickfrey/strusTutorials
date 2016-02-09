#ifndef _STRUS_SUMMARIZER_FUNCTION_MINWIN_HPP_INCLUDED
#define _STRUS_SUMMARIZER_FUNCTION_MINWIN_HPP_INCLUDED

namespace strus {

// Forward declarations:
class SummarizerFunctionInterface;
class ErrorBufferInterface;

// Weighting function constructor:
SummarizerFunctionInterface* createMinWinSummarizerFunction(
	ErrorBufferInterface* errorhnd);

}//namespace
#endif
