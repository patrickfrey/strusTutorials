#!/usr/bin/python
import tornado.ioloop
import tornado.web
import tornado.websocket
import tornado.gen
import os
import sys
import struct
import binascii
import collections
import heapq
import optparse
import trollius
import signal
import strus
import strusMessage

# [0] Globals and helper classes:
# The address of the global statistics server:
statserver = "localhost:7183"
# Strus storage server addresses:
storageservers = []
# Strus client connection factory:
msgclient = strusMessage.RequestClient()

# Query analyzer structures:
strusctx = strus.Context()
analyzer = strusctx.createQueryAnalyzer()
analyzer.definePhraseType(
        "text", "word", "word", 
        ["lc", ["stem", "en"], ["convdia", "en"]]
    )
# Query evaluation structures:
ResultRow = collections.namedtuple('ResultRow', ['docno', 'docid', 'weight', 'title', 'abstract'])


# [1] HTTP handlers:
# Answer a query (issue a query to all storage servers and merge it to one result):
class QueryHandler( tornado.web.RequestHandler ):
    @tornado.gen.coroutine
    def queryStats( self, terms):
        rt = ([],0)
        try:
            print ("+++ queryStats 1")
            statquery = bytearray("Q")
            for term in terms:
                statquery.append('T')
                typesize = len( term.type())
                valuesize = len( term.value())
                print ("+++ queryStats 1.1 %u %u" % (typesize, valuesize))
                statquery += struct.pack( ">HH", typesize, valuesize)
                print ("+++ queryStats 1.2")
                statquery += struct.pack( "%ds%ds" % (typesize,valuesize), term.type(), term.value())
                print ("+++ queryStats 1.3")
            statquery.append('N')
            print ("+++ queryStats 2")
            ri = statserver.rindex(':')
            host,port = statserver[:ri],int( statserver[ri+1:])
            conn = yield msgclient.connect( host, port)
            statreply = yield msgclient.issueRequest( conn, statquery)

            print ("+++ queryStats 3")
            print binascii.hexlify(statreply)
            if (statreply[0] == 'E'):
                raise Exception( "failed to query global statistics: %s" % stats[1:])
            elif (statreply[0] != 'Y'):
                raise Exception( "protocol error loading global statistics")
            dflist = []
            collsize = 0
            statsofs = 1
            statslen = len(statreply)
            while (statsofs < statslen):
                print ("+++ queryStats 4")
                (statsval,) = struct.unpack_from( ">q", statreply, statsofs)
                print ("+++ queryStats 5 %u" % statsval)
                statsofs += struct.calcsize( ">q")
                print ("+++ queryStats 6")
                if (len(dflist) < len(terms)):
                    print ("+++ queryStats 6.1")
                    dflist.append( statsval)
                elif (len(dflist) == len(terms)):
                    print ("+++ queryStats 6.2")
                    collsize = statsval
                else:
                    print ("+++ queryStats 6.3")
                    break
            print ("+++ queryStats 9")
            if (statsofs != statslen):
                raise Exception("statistic server result does not match query")
            print ("+++ queryStats 10")
            rt = (dflist, collsize)
        except IOError as e:
            raise Exception("query statistic server failed: %s" % e)
        raise tornado.gen.Return( rt)

    @tornado.gen.coroutine
    def issueQuery( self, serveraddr, qryblob):
        ri = serveraddr.rindex(':')
        host,port = serveraddr[:ri],int( serveraddr[ri+1:])
        result = None
        conn = None
        try:
            conn = yield msgclient.connect( host, port)
            reply = yield msgclient.issueRequest( conn, qryblob)
            if (reply[0] == 'E'):
                yield conn.close()
                raise tornado.gen.Return( (None, "storage server %s:%d returned error: %s" % (host, port, reply[1:])) )
            elif (reply[0] == 'Y'):
                result = []
                row = None
                replyofs = 1
                replysize = len(reply)-1
                while (replyofs < replysize):
                    if (reply[ replyofs] == '_'):
                        if (row != None):
                            result.append( row)
                        row = ResultRow()
                        replyofs += 1
                    elif (reply[ replyofs] == 'D'):
                        (row.docno,) = struct.unpack_from( ">I", reply, replyofs+1)
                        replyofs += struct.calcsize( ">I") + 1
                    elif (reply[ replyofs] == 'W'):
                        (row.weight,) = struct.unpack_from( ">f", reply, replyofs+1)
                        replyofs += struct.calcsize( ">f") + 1
                    elif (reply[ replyofs] == 'I'):
                        (docidlen,) = struct.unpack_from( ">H", reply, replyofs+1)
                        replyofs += struct.calcsize( ">H") + 1
                        (row.docid,) = struct.unpack_from( "%us" % docidlen, reply, replyofs)
                        replyofs += docidlen
                    elif (reply[ replyofs] == 'T'):
                        (titlelen,) = struct.unpack_from( ">H", reply, replyofs+1)
                        replyofs += struct.calcsize( ">H") + 1
                        (row.title,) = struct.unpack_from( "%us" % titlelen, reply, replyofs)
                        replyofs += titlelen
                    elif (reply[ replyofs] == 'A'):
                        (abstractlen,) = struct.unpack_from( ">H", reply, replyofs+1)
                        replyofs += struct.calcsize( ">H") + 1
                        (row.abstract,) = struct.unpack_from( "%us" % abstractlen, reply, replyofs)
                        replyofs += abstractlen
                    else:
                        yield conn.close()
                        raise tornado.gen.Return( (None, "storage server %s:%u protocol error: unknown result column name" % (host,port)) )
                if (row != None):
                    result.append( row)
                yield conn.close()
                raise tornado.gen.Return( (result, None) )
            else:
                yield conn.close()
                raise tornado.gen.Return( (None, "protocol error storage %s:%u query: unknown header" % (host,port)) )
        except IOError as e:
            if (conn):
                yield conn.close()
            raise tornado.gen.Return( (None, "storage server %s:%u connection error: %s" % (host, port, str(e))))

    @tornado.gen.coroutine
    def issueQueries( self, servers, qryblob):
        try:
            results = yield tornado.gen.multi_future( [ self.issueQuery( addr, qryblob) for addr in servers ] )
            error = results.exception()
            if (error != None):
                raise error
            raise tornado.gen.Return( results.result())
        except Exception as e:
            print "+++ ERROR %s" % str(e)
            raise Exception( "error issueing query: %s" % str(e))

    # Merge code derived from Python Cookbook (Sebastien Keim, Raymond Hettinger and Danny Yoo)
    # referenced in from http://wordaligned.org/articles/merging-sorted-streams-in-python:
    def mergeResultIter( self, resultlists):
        # prepare a priority queue whose items are pairs of the form (-weight, resultlistiter):
        heap = [  ]
        for resultlist in resultlists:
            resultlistiter = iter(resultlist)
            for result in resultlistiter:
                # subseq is not empty, therefore add this subseq's pair
                # (current-value, iterator) to the list
                heap.append((-result.weight, result, resultlistiter))
                break
        # make the priority queue into a heap
        heapq.heapify(heap)
        while heap:
            # get and yield the result with the highest weight (minus lowest negative weight):
            negative_weight, result, resultlistiter = heap[0]
            yield result
            for result in resultlistiter:
                # resultlists is not finished, replace best pair in the priority queue
                heapq.heapreplace( heap, (-result.weight, result, resultlistiter))
                break
            else:
                # subseq has been exhausted, therefore remove it from the queue
                heapq.heappop( heap)

    def mergeQueryResults( results, firstrank, nofranks):
        merged = []
        errors = []
        itrs = []
        maxnofresults = firstrank + nofranks
        for result in results:
            if (result[0] == None):
                errors.append( result[1])
            else:
                itrs.append( iter( result[0]))
        ri = 0
        for result in self.mergeResultIter( itrs):
            if (ri == maxnofresults):
                break
            merged.append( result)
        return (merged[ firstrank:maxnofresults], errors)

    @tornado.gen.coroutine
    def evaluateQueryText( self, querystr, firstrank, nofranks):
        maxnofresults = firstrank + nofranks
        terms = analyzer.analyzePhrase( "text", querystr)
        if len( terms) == 0:
            # Return empty result for empty query:
            raise tornado.gen.Return( [] )
        print ("+++ evaluateQueryText 1")
        # Get the global statistics:
        dflist,collectionsize = yield self.queryStats( terms)
        # Assemble the query:
        print ("+++ evaluateQueryText 2")
        qry = bytearray()
        print ("+++ evaluateQueryText 2.1 %u" % collectionsize)
        qry += bytearray( b"S") + struct.pack( ">q", collectionsize)
        print ("+++ evaluateQueryText 2.2")
        qry += bytearray( b"I") + struct.pack( ">H", 0)
        print ("+++ evaluateQueryText 2.3")
        qry += bytearray( b"N") + struct.pack( ">H", maxnofresults)
        print ("+++ evaluateQueryText 3")
        for ii in range( 0, len( terms)):
            qry += bytearray( b"T")
            print ("+++ 3.1")
            typesize = len(terms[ii].type())
            print ("+++ 3.2")
            valuesize = len(terms[ii].value())
            print ("+++ 3.3")
            qry += struct.pack( ">qHH", dflist[ii], typesize, valuesize)
            print ("+++ 3.4")
            qry += struct.pack( "%ds%ds" % (typesize,valuesize), terms[ii].type(), terms[ii].value())
            print ("+++ 3.5")
        # Query all storage servers:
        print ("+++ evaluateQueryText 4")
        results = yield self.issueQueries( storageservers, qry)
        raise tornado.gen.Return( mergeQueryResults( results, firstrank, nofranks))

    def get(self):
        try:
            # q = query terms:
            querystr = self.get_argument( "q", None)
            # i = firstrank:
            firstrank = int( self.get_argument( "i", 0))
            # n = nofranks:
            nofranks = int( self.get_argument( "n", 20))
            # Evaluate query with BM25 (Okapi):
            result = self.evaluateQueryText( querystr, firstrank, nofranks)
            # Render the results:
            self.render( "search_bm25_html.tpl", results=result[0], messages=result[1])
        except Exception as e:
            self.render( "search_error_html.tpl", message=e)

# Insert a multipart document (POST request):
class InsertHandler( tornado.web.RequestHandler ):
    @tornado.gen.coroutine
    def post(self, port):
        try:
            print "+++ InsertHandler post 1"
            # Insert documents:
            conn = yield msgclient.connect( 'localhost', int(port))

            print "+++ InsertHandler post 2"
            reply = yield msgclient.issueRequest( conn, b"I" + bytearray( self.request.body))
            if (reply[0] == 'E'):
                print "+++ InsertHandler post 3 %s" % reply
                raise Exception( bytes( reply[1:]))
            elif (reply[0] != 'Y'):
                print "+++ InsertHandler post 4"
                raise Exception( "protocol error server reply on insert")

            print "+++ InsertHandler post 7"
            (nofDocuments,) = struct.unpack( ">I", reply[1:])
            self.write( "OK %u\n" % (nofDocuments))
        except Exception as e:
            self.write( "ERR %s\n" % str(e))

# [3] Dispatcher:
application = tornado.web.Application([
    # /query in the URL triggers the handler for answering queries:
    (r"/query", QueryHandler),
    # /insert in the URL triggers the post handler for insert requests:
    (r"/insert/([0-9]+)", InsertHandler),
    # /static in the URL triggers the handler for accessing static 
    # files like images referenced in tornado templates:
    (r"/static/(.*)",tornado.web.StaticFileHandler,
        {"path": os.path.dirname(os.path.realpath(sys.argv[0]))},)
])

def on_shutdown():
    print('Shutting down')
    tornado.ioloop.IOLoop.current().stop()

# [5] Server main:
if __name__ == "__main__":
    try:
        # Port of this query server (set to default):
        parser = optparse.OptionParser()
        parser.add_option("-p", "--port", dest="port", default=8080,
                          help="Specify the port of this server as PORT (default %u)" % 8080,
                          metavar="PORT")
        parser.add_option("-s", "--statserver", dest="statserver", default=statserver,
                          help="Specify the address of the statistics server as ADDR (default %s" % statserver,
                          metavar="ADDR")

        (options, args) = parser.parse_args()
        if len(args) > 0:
            parser.error("no arguments expected")
            parser.print_help()
        myport = options.port
        statserver = options.statserver
        if (statserver[0:].isdigit()):
            statserver = '{}:{}'.format( 'localhost', statserver)

        # Positional arguments are storage server addresses, if empty use default at localhost:7184
        for arg in args:
            if (arg[0:].isdigit()):
                storageservers.append( '{}:{}'.format( 'localhost', arg))
            else:
                storageservers.append( arg)
        if (len( storageservers) == 0):
            storageservers.append( "localhost:7184")

        # Start server:
        print( "Starting server ...\n")
        application.listen( myport )
        print( "Listening on port %u\n" % myport )
        ioloop = tornado.ioloop.IOLoop.current()
        signal.signal( signal.SIGINT, lambda sig, frame: ioloop.add_callback_from_signal(on_shutdown))
        ioloop.start()
        print( "Terminated\n")
    except Exception as e:
        print( e)


