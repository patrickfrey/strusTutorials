#include "window_joinop.hpp"
#include "positionWindow.hpp"
#include "strus/lib/error.hpp"
#include "strus/postingJoinOperatorInterface.hpp"
#include "strus/postingIteratorInterface.hpp"
#include <cstdio>
#include <iostream>
#include <memory>
#include <vector>
#include <stdexcept>
#include <ctime>
#include <algorithm>
#include <limits>
#include <cstdarg>
#include <memory>
#include "strus/base/stdint.h"

#undef STRUS_LOWLEVEL_DEBUG

uint32_t uint32_hash( uint32_t a)
{
	a += ~(a << 15);
	a ^=  (a >> 10);
	a +=  (a << 3);
	a ^=  (a >> 6);
	a += ~(a << 11);
	a ^=  (a >> 16);
	return a;
}

class Random
{
public:
	Random()
	{
		time_t nowtime;
		struct tm* now;

		::time( &nowtime);
		now = ::localtime( &nowtime);

		m_value = uint32_hash( ((now->tm_year+1)
					* (now->tm_mon+100)
					* (now->tm_mday+1)));
	}

	unsigned int get( unsigned int min_, unsigned int max_)
	{
		if (min_ >= max_)
		{
			throw std::runtime_error("illegal range passed to pseudo random number generator");
		}
		m_value = uint32_hash( m_value + 1);
		unsigned int iv = max_ - min_;
		if (iv)
		{
			return (m_value % iv) + min_;
		}
		else
		{
			return min_;
		}
	}

private:
	unsigned int m_value;
};

static Random g_random;
static strus::ErrorBufferInterface* g_errorbuf = 0;

class TestDocument
{
public:
	TestDocument()
		:m_terms(){}
	TestDocument( unsigned int size, unsigned int nofterms)
		:m_terms()
	{
		unsigned int ii=0;
		for (; ii<size; ++ii)
		{
			m_terms.push_back( g_random.get( 1, nofterms));
		}
	}
	TestDocument( const TestDocument& o)
		:m_terms(o.m_terms){}

	const std::vector<unsigned int>& terms() const		{return m_terms;}

private:
	std::vector<unsigned int> m_terms;
};

class TestCollection
{
public:
	TestCollection()
		:m_docs(){}
	TestCollection( unsigned int size, unsigned int maxdocsize, unsigned int nofterms)
		:m_docs()
	{
		unsigned int ii=0;
		for (; ii<size; ++ii)
		{
			m_docs.push_back( TestDocument( g_random.get( 1, maxdocsize), nofterms));
		}
	}
	TestCollection( const TestCollection& o)
		:m_docs(o.m_docs){}

	const std::vector<TestDocument>& docs() const		{return m_docs;}

	void print( std::ostream& out) const
	{
		std::vector<TestDocument>::const_iterator
			di = docs().begin(), de = docs().end();
		for (unsigned int didx=1; di != de; ++di,++didx)
		{
			out << didx << ":";
			std::vector<unsigned int>::const_iterator
				ti = di->terms().begin(), te = di->terms().end();
			for (; ti != te; ++ti)
			{
				out << " " << *ti;
			}
			out << std::endl;
		}
	}

private:
	std::vector<TestDocument> m_docs;
};


class TestPostingIterator
	:public strus::PostingIteratorInterface
{
public:
	TestPostingIterator( const TestCollection& coll, unsigned int termno_)
		:m_dbegin(coll.docs().begin()),m_ditr(coll.docs().begin()),m_dend(coll.docs().end())
		,m_posno(0),m_termno(termno_)
	{
		snprintf( m_featureid, sizeof(m_featureid), "test(%u)", m_termno);
	}

	virtual strus::Index skipDoc( const strus::Index& docno)
	{
		m_posno = 0;
		if (!skipDocCandidate( docno)) return 0;
		m_ditr = m_dbegin + docno-1;
		for (; m_ditr <= m_dend; ++m_ditr)
		{
			std::vector<unsigned int>::const_iterator
				ti = m_ditr->terms().begin(), te = m_ditr->terms().end();
			for (; ti != te && *ti != m_termno; ++ti){}
			if (ti != te) return (m_ditr-m_dbegin)+1;
		}
		return 0;
	}

	virtual strus::Index skipDocCandidate( const strus::Index& docno)
	{
		m_posno = 0;
		if (m_dend - m_dbegin < docno)
		{
			m_ditr = m_dend;
			return 0;
		}
		if (docno == 0)
		{
			m_ditr = m_dbegin;
		}
		else
		{
			m_ditr = m_dbegin + docno-1;
		}
		return (m_ditr-m_dbegin)+1;
	}

	virtual strus::Index skipPos( const strus::Index& firstpos)
	{
		if (m_ditr == m_dend) return 0;
		std::size_t ofs = firstpos?(std::size_t)(firstpos-1):0;
		std::vector<unsigned int>::const_iterator
			ti = m_ditr->terms().begin() + ofs, te = m_ditr->terms().end();
		for (; ti < te && *ti != m_termno; ++ti){}
		return m_posno = (ti < te)?(ti - m_ditr->terms().begin() + 1):0;
	}

	virtual const char* featureid() const
	{
		return m_featureid;
	}

	virtual strus::Index documentFrequency() const
	{
		strus::Index rt = 0;
		std::vector<TestDocument>::const_iterator di = m_dbegin, de = m_dend;
		for (; di != de; ++di)
		{
			std::vector<unsigned int>::const_iterator
				ti = m_ditr->terms().begin(), te = m_ditr->terms().end();
			for (; ti != te && *ti != m_termno; ++ti){}
			rt += (ti != te)?1:0;
		}
		return rt;
	}

	virtual unsigned int frequency()
	{
		strus::Index rt = 0;
		if (m_ditr == m_dend) return 0;
		std::vector<unsigned int>::const_iterator
			ti = m_ditr->terms().begin(), te = m_ditr->terms().end();
		for (; ti != te; ++ti)
		{
			if (*ti == m_termno) ++rt;
		}
		return rt;
	}

	virtual strus::Index docno() const
	{
		return (m_ditr == m_dend)?0:(m_ditr - m_dbegin + 1);
	}

	virtual strus::Index posno() const
	{
		return m_posno;
	}

private:
	char m_featureid[ 32];
	std::vector<TestDocument>::const_iterator m_dbegin;
	std::vector<TestDocument>::const_iterator m_ditr;
	std::vector<TestDocument>::const_iterator m_dend;
	std::size_t m_posno;
	unsigned int m_termno;
};

class SimplePostingIterator
	:public strus::PostingIteratorInterface
{
public:
	enum {MaxPosArraySize=64};
	SimplePostingIterator( const unsigned int* posar_, unsigned int id)
		:m_posarsize(0),m_posidx(0),m_docno(0),m_posno(0)
	{
		for (;m_posarsize < MaxPosArraySize && posar_[m_posarsize]; ++m_posarsize)
		{
			m_posar[ m_posarsize] = posar_[m_posarsize];
		}
		snprintf( m_featureid, sizeof(m_featureid), "simple(%u)", id);
	}

	virtual strus::Index skipDoc( const strus::Index& docno_)
	{
		return m_docno = docno_?docno_:docno_+1;
	}

	virtual strus::Index skipDocCandidate( const strus::Index& docno_)
	{
		return m_docno = docno_?docno_:docno_+1;
	}

	virtual strus::Index skipPos( const strus::Index& firstpos)
	{
		if (m_posarsize == 0) return 0;
		while (m_posidx < m_posarsize && m_posar[m_posidx] < firstpos)
		{
			++m_posidx;
		}
		if (m_posidx == m_posarsize)
		{
			m_posidx = 0;
			return m_posno = 0;
		}
		while (m_posidx > 0 && m_posar[m_posidx-1] >= firstpos)
		{
			--m_posidx;
		}
		return m_posno = m_posar[m_posidx];
	}

	virtual const char* featureid() const
	{
		return m_featureid;
	}

	virtual strus::Index documentFrequency() const
	{
		return 0;
	}

	virtual unsigned int frequency()
	{
		return m_posarsize;
	}

	virtual strus::Index docno() const
	{
		return m_docno;
	}

	virtual strus::Index posno() const
	{
		return m_posno;
	}

private:
	char m_featureid[ 32];
	strus::Index m_posar[ MaxPosArraySize];
	std::size_t m_posarsize;
	std::size_t m_posidx;
	strus::Index m_docno;
	strus::Index m_posno;
};



static void testIterator( const TestCollection& testCollection, unsigned int exprsize)
{
	std::auto_ptr<strus::PostingJoinOperatorInterface> joinop( strus::createWindowJoinOperator( g_errorbuf));
AGAIN:
	strus::Index docno = g_random.get( 0, testCollection.docs().size())+1;
	std::vector<TestDocument>::const_iterator di = testCollection.docs().begin() + docno -1;
	if (di->terms().size() < 2) goto AGAIN;
	unsigned int tidx = g_random.get( 0, di->terms().size()/2);
	strus::Index posno = tidx+1;
	std::vector<unsigned int>::const_iterator ti = di->terms().begin()+tidx, te = di->terms().end();
	std::vector<strus::Reference<strus::PostingIteratorInterface> > argitrs;
	unsigned int cardinality = exprsize;
	bool isfirst = true;
	unsigned int ei = 0;
	for (; ei<exprsize && ti < te; ++ei,++ti)
	{
		if (g_random.get( 0,5) == 1)
		{
			--cardinality;
			if (isfirst)
			{
				//... is first
				posno += 1;
			}
		}
		else
		{
			isfirst = false;
#ifdef STRUS_LOWLEVEL_DEBUG
			std::cerr << "add sub term " << *ti << std::endl;
#endif
			strus::Reference<strus::PostingIteratorInterface> piref( new TestPostingIterator( testCollection, *ti));
			argitrs.push_back( piref);
		}
	}
#ifdef STRUS_LOWLEVEL_DEBUG
	std::cerr << "create expression size " << exprsize << " cardinality " << cardinality << std::endl;
#endif
	if(!cardinality || (ti == te && ei < exprsize)) goto AGAIN;
	std::auto_ptr<strus::PostingIteratorInterface> pitr( joinop->createResultIterator( argitrs, 1+exprsize, cardinality));
	strus::Index test_docno = pitr->skipDoc( docno);
	strus::Index test_posno = pitr->skipPos( posno);
#ifdef STRUS_LOWLEVEL_DEBUG
	std::cerr << "check doc " << test_docno << "=" << docno << ", pos " << test_posno << "=" << posno << std::endl;
#endif
	if (test_docno != docno || test_posno != posno)
	{
		throw std::runtime_error( "test failed");
	}
}


static void testWinWindow()
{
	struct Result
	{
		unsigned int pos;
		unsigned int size;
	};
	static const unsigned int ar[3][32] = {{1,5,9,15,0},{2,6,10,16,0},{5,11,18,0}};
	static const Result res[32] = {{1,4},{2,3},{5,1},{5,4},{6,5},{9,2},{10,5},{11,5},{15,3},{0,0}};

	std::vector<strus::Reference<SimplePostingIterator> > argbufs;
	std::vector<strus::PostingIteratorInterface*> args;
	for (std::size_t ii=0; ii<3; ++ii)
	{
		argbufs.push_back( new SimplePostingIterator( ar[ii], ii+1));
		args.push_back( argbufs.back().get());
	}
	Result const* ri = res;
	strus::PositionWindow win( args, 10, 0, 0);
	bool more=win.first();
	for (; more && ri->pos; more=win.next(),++ri)
	{
		if (win.pos() != ri->pos || win.size() != ri->size)
		{
			std::cerr << "error window position " << win.pos() << " size " << win.size() << ", expected position " << ri->pos << " size " << ri->size << std::endl;
			throw std::runtime_error( "test failed");
		}
#ifndef STRUS_LOWLEVEL_DEBUG
		std::cerr << "window position " << win.pos() << " size " << win.size() << std::endl;
#endif
	}
	if (more) throw std::runtime_error( "test failed: more matches than expected");
	if (ri->pos) throw std::runtime_error( "test failed: not all matches found");
}

int main( int argc, char** argv)
{
	try
	{
		std::auto_ptr<strus::ErrorBufferInterface> errorbuf( strus::createErrorBuffer_standard( stderr, 2));
		g_errorbuf = errorbuf.get();

		testWinWindow();

		unsigned int collsize = 100;
		unsigned int maxdocsize = 100;
		unsigned int nofterms = 10;
		unsigned int noftests = 1000;

		TestCollection testCollection( collsize, maxdocsize, nofterms);
#ifdef STRUS_LOWLEVEL_DEBUG
		testCollection.print( std::cerr);
#endif
		for (unsigned int ti=0; ti<noftests; ++ti)
		{
#ifdef STRUS_LOWLEVEL_DEBUG
			std::cerr << "[test " << (ti+1) << "]" << std::endl;
#endif
			testIterator( testCollection, 4);
		}
		std::cerr << "OK " << noftests << std::endl;
	}
	catch (const std::exception& err)
	{
		std::cerr << "ERROR " << err.what() << std::endl;
		return -1;
	}
	return 0;
}

