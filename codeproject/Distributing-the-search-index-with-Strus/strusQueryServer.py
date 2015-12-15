#!/usr/bin/python
import tornado.ioloop
import tornado.web
import tornado.websocket
import os
import sys
import gen
import struct
import collections

# [0] Globals and helper classes:
# Port of this query server (set to default):
myport = 7184
# Port of the global statistics server:
globstatsport = 7183
# Strus storage server ports:
storageports = []

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
    @gen.coroutine
    def queryStats( self, terms):
        statquery = bytearray()
        for term in terms:
            statquery.append('T')
            typesize = len( term.type())
            valuesize = len( term.value())
            statquery.append( struct.pack( ">HH", typesize, valuesize)
            statquery.append( struct.pack( "%ds%ds" % (typesize,valuesize), term.type(), term.value())
        statquery.append('N')
        conn = yield from websocket.create_connection(
                      'ws://{}:{}/stats'.format( 'localhost', globstatsport)))
        yield from conn.send( msg )
        stats = yield from websocket.recv()
        if (stats[0] == 'E'):
            raise Exception( "failed to query global statistics: %s" % stats[1:])
        rt = []
        statsofs = 1
        statslen = len(stats)
        while (statsofs < statslen):
            rt.append( struct.unpack_from( ">q", stats, statsofs)
            statsofs += struct.calcsize( ">q")
        if (len(rt) != len(terms)+1):
            raise Exception("statistic server result does not match query")
        return rt

    @gen.coroutine
    def queryExecute( port, qry):
        result = None
        conn = None
        try:
            conn = yield from websocket.websocket_connect(
                  'ws://{}:{}/binquery'.format( 'localhost', port))

            yield from conn.send( qry )
            reply = yield from websocket.recv()
            if (reply[0] == 'E'):
                conn.close()
                return (None, "storage server %u returned error: %s" % (port, reply[1:]))
            elif if (reply[0] == 'Y'):
                result = []
                row = None
                replyofs = 1
                replysize = len(reply)-1
                while (replyofs < replysize):
                    if (reply[ replyofs] == '_')
                        if (row != None):
                            result.append( row)
                        row = ResultRow()
                        replyofs += 1
                    elif (reply[ replyofs] == 'D')
                        (row.docno,) = struct.unpack_from( ">I", reply, replyofs+1)
                        replyofs += struct.calcsize( ">I") + 1
                    elif (reply[ replyofs] == 'W')
                        (row.weight,) = struct.unpack_from( ">f", reply, replyofs+1)
                        replyofs += struct.calcsize( ">f") + 1
                    elif (reply[ replyofs] == 'I')
                        docidlen = struct.unpack_from( ">H", reply, replyofs+1)
                        replyofs += struct.calcsize( ">H") + 1
                        (row.docid,) = struct.unpack_from( "%us" % docidlen, reply, replyofs)
                        replyofs += docidlen
                    elif (reply[ replyofs] == 'T')
                        titlelen = struct.unpack_from( ">H", reply, replyofs+1)
                        replyofs += struct.calcsize( ">H") + 1
                        (row.title,) = struct.unpack_from( "%us" % titlelen, reply, replyofs)
                        replyofs += titlelen
                    elif (reply[ replyofs] == 'A')
                        abstractlen = struct.unpack_from( ">H", reply, replyofs+1)
                        replyofs += struct.calcsize( ">H") + 1
                        (row.abstract,) = struct.unpack_from( "%us" % abstractlen, reply, replyofs)
                        replyofs += abstractlen
                    else:
                        conn.close()
                        return (None, "storage server %u protocol error: unknown result column name", (port))
                if (row != None):
                    result.append( row)
                conn.close()
                return (result, None)
            else:
                conn.close()
                return (None, "protocol error storage %u query: unknown header" % (port))
        except tornado.httpclient.HTTPError as e:
            if (conn):
                conn.close()
            return (None, "storage server %u connection error: %s" % (port, e))

    @gen.coroutine
    def queryCallback( port, qry):
        return queryExecute( port, qry), gen.Callback( port)

    def issueQuery( port, qry):
        # Initiate the publishing of statistics as task in the eventloop
        tornado.ioloop.IOLoop.current().add_callback(
                callback=lambda: queryCallback( port, qry))

    def rowMergeIter( itrs):
        minweight = sys.float_info.max
        minitr = None
        for itr in itrs:
            if (itr.weight < minweight):
                minweight = itr.weight
                minitr = itr
                itr.next()
        if (minitr != None):
            yield minitr

    def mergeQueryResults( results):
        merged = []
        errors = []
        itrs = []
        for result in results:
            if (result[0] == None):
                errors.append( result[1])
            else:
                itrs.append( iter( result[0]))

        
        while (len( results) > 0):
            

    @gen.coroutine
    def evaluateQueryText( self, querystr, firstrank, nofranks):
        terms = analyzer.analyzePhrase( "text", querystr)
        if len( terms) == 0:
            # Return empty result for empty query:
            return []
        # Get the global statistics:
        stats = self.queryStats( terms)
        # Assemble the query:
        qry = bytearray()
        collectionsize = stats[ len( terms)]
        qry.append( b"S" + struct.pack( ">q", collectionsize))
        qry.append( b"I" + struct.pack( ">H", firstrank))
        qry.append( b"N" + struct.pack( ">H", nofranks))
        for ii in range( 0, len( terms)):
            qry.append( b"T")
            typesize = len(terms[ii].type())
            valuesize = len(terms[ii].value())
            qry.append( struct.pack( ">qHH", stats[ii], typesize, valuesize))
            qry.append( struct.pack( "%ds%ds" % (typesize,valuesize), terms[ii].type(), terms[ii].value())
        # Query all storage servers:
        for port in storageports:
            issueQuery( port, qry)
        # Merge the results:
        return mergeQueryResults( yield gen.WaitAll( storageports))

    def get(self):
        try:
            # q = query terms:
            querystr = self.get_argument( "q", None)
            # Evaluate query with BM25 (Okapi):
            result = self.evaluateQueryText( querystr, firstrank, nofranks)
            # Render the results:
            self.render( "search_bm25_html.tpl",
                             scheme=scheme, querystr=querystr,
                             firstrank=firstrank, nofranks=nofranks,
                             results=result[0], messages=result[1])
        except Exception as e:
            self.render( "search_error_html.tpl", 
                         message=e, scheme=scheme, querystr=querystr,
                         firstrank=firstrank, nofranks=nofranks)

# Query a list of statistics (web socket request with a binary blob, proprietary protocol):
class QueryHandler( tornado.websocket.WebSocketHandler ):
    def errormsg( msg)
        return struct.pack( ">H%ds" % len(msg), len(msg), msg)

    def on_message(self, message):
        try:
            rt = bytearray()
            messagesize = len(message)
            messageofs = 0
            while (messageofs < messagesize):
                if (message[ messageofs] == 'T')
                    # Fetch df of term, message format [T][typesize][valuesize][type string][value string]:
                    (typesize,valuesize) = struct.unpack_from( ">HH", message, messageofs+1)
                    messageofs += struct.calcsize( ">HH") + 1
                    (type,value) = struct.unpack_from( "%ds%ds" % (typesize,valuesize), message, messageofs)
                    messageofs += typesize + valuesize
                    df = 0
                    key = struct.pack( 'ps', type, value)
                    if key in termDfMap:
                        df = termDfMap[ key]
                    rt.append( struct.pack( ">q", df)
                elif (message[ messageofs] == 'N')
                    # Fetch N (nof documents), message format [N]:
                    messageofs += 1
                    rt.append( struct.pack( ">q", nofDocs)
                else:
                    rt.append( b"E" + errormsg( b"unknown command"))
                    messageofs = messagesize    # ... stop parsing on error
        except Exception as e:
            rt.append( b"E" + errormsg( bytearray( e)))
        self.write_message( rt, binary=True)

# [3] Dispatcher:
application = tornado.web.Application([
    # /query in the URL triggers the handler for answering queries:
    (r"/query", QueryHandler),
    # /static in the URL triggers the handler for accessing static 
    # files like images referenced in tornado templates:
    (r"/static/(.*)",tornado.web.StaticFileHandler,
        {"path": os.path.dirname(os.path.realpath(sys.argv[0]))},)
])


# [5] Server main:
if __name__ == "__main__":
    try:
        if (len(sys.argv) >= 2):
            # First argument port, if not use default port
            myport = int(sys.argv[1])

        # Other arguments are storage ports, if empty use default port
        storageports.append( sys.argv[2:])
        if (len( storageports) == 0):
            storageports.append( 7182)

        # Start server:
        print( "Starting server ...\n")
        application.listen( myport )
        print( "Listening on port %u\n" % myport )
        tornado.ioloop.IOLoop.current().start()
        print( "Terminated\n")
    except Exception as e:
        print( e)


