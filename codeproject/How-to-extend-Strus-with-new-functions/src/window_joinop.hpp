#ifndef _STRUS_POSTING_JOIN_OPERATOR_WINDOW_HPP_INCLUDED
#define _STRUS_POSTING_JOIN_OPERATOR_WINDOW_HPP_INCLUDED

namespace strus {

// Forward declarations:
class PostingJoinOperatorInterface;
class ErrorBufferInterface;

// Window iterator constructor:
PostingJoinOperatorInterface* createWindowJoinOperator(
	ErrorBufferInterface* errorhnd);

}//namespace
#endif

