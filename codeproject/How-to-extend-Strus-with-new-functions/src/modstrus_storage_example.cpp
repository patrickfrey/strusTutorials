#include "strus/base/dll_tags.hpp"
#include "strus/storageModule.hpp"
#include "window_joinop.hpp"
#include "minwin_weighting.hpp"
#include "minwin_summarizer.hpp"

using namespace strus;

static const PostingIteratorJoinConstructor postingJoinOperators[] =
{
	{"window", createWindowJoinOperator},
	{0,0}
};

static const WeightingFunctionConstructor weightingFunctions[] =
{
	{"minwin", createMinWinWeightingFunction},
	{0,0}
};

static const SummarizerFunctionConstructor summarizers[] =
{
	{"minwin", createMinWinSummarizerFunction},
	{0,0}
};

extern "C" DLL_PUBLIC strus::StorageModule entryPoint;

strus::StorageModule entryPoint( postingJoinOperators, weightingFunctions, summarizers);




