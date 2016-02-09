#ifndef _STRUS_WEIGHTING_FUNCTION_MINWIN_HPP_INCLUDED
#define _STRUS_WEIGHTING_FUNCTION_MINWIN_HPP_INCLUDED

namespace strus {

// Forward declarations:
class WeightingFunctionInterface;
class ErrorBufferInterface;

// Weighting function constructor:
WeightingFunctionInterface* createMinWinWeightingFunction(
	ErrorBufferInterface* errorhnd);

}//namespace
#endif

