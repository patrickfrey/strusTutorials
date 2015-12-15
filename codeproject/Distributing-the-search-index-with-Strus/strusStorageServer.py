#!/usr/bin/python
import tornado.ioloop
import tornado.web
import tornado.websocket
import os
import sys
import struct
import strusIR
import time
import collections

# [0] Globals:
# Information retrieval engine:
backend = strusIR.Backend( "path=storage; cache=512M")
# Port of the global statistics server:
globstatsport = 7183
# Port of this storage server:
myport = 7182

# [1] 
# Insert a document (POST request with the multipart document as body):
class InsertHandler( tornado.web.RequestHandler ):
    def post(self):
        try:
            # Insert documents:
            content = self.request.body.decode('utf-8')
            nofDocuments = backend.insertDocuments( content)
            # Publish statistic updates:
            publishStatistics( backend.getUpdateStatisticsIterator())
            self.write( "OK %u\n" % (nofDocuments))
        except Exception as e:
            self.write( "ERR %s\n" % (e))


# Answer a query (binary protocol):
class QueryHandlerBin( tornado.websocket.WebSocketHandler ):
    def errormsg( msg)
        return struct.pack( ">H%ds" % len(msg), len(msg), msg)

    def packedstr( str)
        return struct.pack( ">H%ds" % len(str), len(str), str)

    def get(self):
        try:
            Term = collections.namedtuple('Term', ['type', 'value', 'df'])
            rt = bytearray()
            nofranks = 20
            collectionsize = 0
            firstrank = 0
            terms = []
            messagesize = len(message)
            messageofs = 0
            while (messageofs < messagesize):
                if (message[ messageofs] == 'I')
                    (firstrank,) = struct.unpack_from( ">H", message, messageofs+1)
                    messageofs += struct.calcsize( ">H") + 1
                elif (message[ messageofs] == 'N')
                    (nofranks,) = struct.unpack_from( ">H", message, messageofs+1)
                    messageofs += struct.calcsize( ">H") + 1
                elif (message[ messageofs] == 'S')
                    (collectionsize,) = struct.unpack_from( ">q", message, messageofs+1)
                    messageofs += struct.calcsize( ">q") + 1
                elif (message[ messageofs] == 'T')
                    (df,typesize,valuesize) = struct.unpack_from( ">qHH", message, messageofs+1)
                    messageofs += struct.calcsize( ">qHH") + 1
                    (type,value) = struct.unpack_from( "%ds%ds" % (typesize,valuesize), message, messageofs)
                    messageofs += typesize + valuesize
                    terms.append( Term( type, value, df)
                else:
                    rt = b"E" + errormsg( b"unknown parameter")
                    self.write_message( rt, binary=True)
                    return
            # Evaluate query with BM25 (Okapi):
            results = backend.evaluateQueryText( terms, collectionsize, firstrank, nofranks)
            # Build the result:
            rt = bytearray("Y")
            for result in results:
                rt.append( '_')
                rt.append( 'D')
                rt.append( struct.pack( ">I", result['docno'])
                rt.append( 'W')
                rt.append( struct.pack( ">f", result['weight'])
                rt.append( 'I')
                rt.append( packedstr( result['docid']))
                rt.append( 'T')
                rt.append( packedstr( result['title']))
                rt.append( 'A')
                rt.append( packedstr( result['abstract']))
            # Send the result back:
            self.write_message( rt, binary=True)

        except Exception as e:
            rt = b"E" + errormsg( bytearray( e))
            self.write_message( rt, binary=True)

# [2] Dispatcher:
application = tornado.web.Application([
    # /insert in the URL triggers the handler for inserting documents:
    (r"/insert", InsertHandler),
    # /binquery in the URL triggers the handler for answering queries with a binary protocol:
    (r"/binquery", QueryHandlerBin),
])

# [3] Publish statistics:
@gen.coroutine
def publishStatisticsCallback( itr):
    # Open connection to server:
    nofTries = 20
    conn = None
    errmsg = ""
    while (nofTries > 0 and conn == None):
        try:
            conn = yield from websocket.create_connection(
                      'ws://{}:{}/publish'.format( 'localhost', globstatsport)))
        except tornado.httpclient.HTTPError as e:
            nofTries -= 1
            errmsg = e
            time.sleep(3) # delays for 5 seconds

    if (conn != None):
        msg = itr.getNext()
        while (len(msg) > 0):
            yield from conn.send( msg )
            reply = yield from websocket.recv()
            if (reply[0] == 'E'):
                raise Exception( "error in global statistis server: %s" % reply[ 1:])
            elif if (reply[0] != 'Y'):
                raise Exception( "protocol error publishing statistics")
            msg = itr.getNext()
        conn.close()
    else:
        raise Exception( "connection error publishing statistics: %s" % errmsg)
    print "publishing initial statistics finished"

def publishStatistics( itr):
    # Initiate the publishing of statistics as task in the eventloop
    tornado.ioloop.IOLoop.current().add_callback(
            callback=lambda: publishStatisticsCallback( itr))


# [4] Server main:
if __name__ == "__main__":
    try:
        if (len(sys.argv) >= 2):
            # First argument own port, if not use default port
            myport = int(sys.argv[1])

        if (len(sys.argv) >= 3):
            # Second argument port of global statistics server, if not use default port
            globstatsport = int(sys.argv[2])

        # Start publish local statistics:
        print( "Load local statistics to publish ...\n")
        publishStatistics( backend.getInitPeerMessageIterator())

        # Start server:
        print( "Starting server ...\n")
        application.listen( myport )
        print( "Listening on port %u\n" % myport )
        tornado.ioloop.IOLoop.current().start()
        print( "Terminated\n")
    except Exception as e:
        print( e)


