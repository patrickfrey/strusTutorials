/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "strus/base/dll_tags.hpp"
#include "strus/traceModule.hpp"
#include "strus/traceLoggerInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include <string>
#include <vector>
#include <map>
#include <iostream>

class TraceLoggerExample
	:public strus::TraceLoggerInterface
{
public:
	explicit TraceLoggerExample( strus::ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_),m_logcnt(0){}
	virtual ~TraceLoggerExample()
	{
		close();
	}

	virtual strus::TraceLogRecordHandle
		logMethodCall(
			const char* className,
			const char* methodName,
			const strus::TraceObjectId&)
	{
		char methodid[ 256];
		snprintf( methodid, sizeof(methodid), "%s::%s", className, methodName);
		try
		{
			++m_callcount[ methodid];
			return ++m_logcnt;
		}
		catch (const std::bad_alloc&)
		{
			m_errorhnd->report( "out of memory");
			return 0;
		}
	}

	virtual void logMethodTermination(
			const strus::TraceLogRecordHandle& ,
			const std::vector<strus::TraceElement>& )
	{}

	virtual bool close()
	{
		try
		{
			std::map<std::string,int>::const_iterator ci = m_callcount.begin(), ce = m_callcount.end();
			for (; ci != ce; ++ci)
			{
				std::cout << ci->first << " called " << ci->second << " times" << std::endl;
			}
			return true;
		}
		catch (const std::bad_alloc&)
		{
			m_errorhnd->report( "out of memory");
			return false;
		}
	}

private:
	strus::ErrorBufferInterface* m_errorhnd;
	std::map<std::string,int> m_callcount;
	strus::TraceLogRecordHandle m_logcnt;
};


static strus::TraceLoggerInterface* createTraceLogger_example( const std::string& , strus::ErrorBufferInterface* errorhnd)
{
	try
	{
		return new TraceLoggerExample( errorhnd);
	}
	catch (const std::bad_alloc&)
	{
		errorhnd->report( "out of memory");
		return 0;
	}
}

static const strus::TraceLoggerConstructor tracelogger[] =
{
	{"traceexample", &createTraceLogger_example},
	{0,0}		
};

extern "C" DLL_PUBLIC strus::TraceModule entryPoint;

strus::TraceModule entryPoint( tracelogger);






