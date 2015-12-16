#!/usr/bin/python
import tornado.ioloop
import tornado.web
import tornado.websocket
import tornado.gen
import os
import sys
import struct
import strusIR
import time
import collections
import optparse
import websocket
import trollius

# [0] Globals:
# Information retrieval engine:
backend = strusIR.Backend( "path=storage; cache=512M")
# Port of the global statistics server:
statserver = "localhost:7183"
# Port of this storage server:
myport = 7184

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

# Remove own statistics from global statistics (unsubscribe):
class UnsubscribeHandler( tornado.web.RequestHandler ):
    def get(self):
        try:
            # Publish negative statistics:
            publishStatistics( backend.getDoneStatisticsIterator())
            self.write( "OK\n")
        except Exception as e:
            self.write( "ERR %s\n" % (e))

# Remove own statistics from global statistics (unsubscribe):
class SubscribeHandler( tornado.web.RequestHandler ):
    def get(self):
        try:
            # Publish negative statistics:
            publishStatistics( backend.getInitStatisticsIterator())
            self.write( "OK\n")
        except Exception as e:
            self.write( "ERR %s\n" % (e))

# Answer a query (binary protocol):
class QueryHandlerBin( tornado.websocket.WebSocketHandler ):
    def errormsg( self, msg):
        return struct.pack( ">H%ds" % len(msg), len(msg), msg)

    def packedstr( self, str):
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
                if (message[ messageofs] == 'I'):
                    (firstrank,) = struct.unpack_from( ">H", message, messageofs+1)
                    messageofs += struct.calcsize( ">H") + 1
                elif (message[ messageofs] == 'N'):
                    (nofranks,) = struct.unpack_from( ">H", message, messageofs+1)
                    messageofs += struct.calcsize( ">H") + 1
                elif (message[ messageofs] == 'S'):
                    (collectionsize,) = struct.unpack_from( ">q", message, messageofs+1)
                    messageofs += struct.calcsize( ">q") + 1
                elif (message[ messageofs] == 'T'):
                    (df,typesize,valuesize) = struct.unpack_from( ">qHH", message, messageofs+1)
                    messageofs += struct.calcsize( ">qHH") + 1
                    (type,value) = struct.unpack_from( "%ds%ds" % (typesize,valuesize), message, messageofs)
                    messageofs += typesize + valuesize
                    terms.append( Term( type, value, df))
                else:
                    rt = b"E" + self.errormsg( b"unknown parameter")
                    self.write_message( rt, binary=True)
                    return
            # Evaluate query with BM25 (Okapi):
            results = backend.evaluateQueryText( terms, collectionsize, firstrank, nofranks)
            # Build the result:
            rt = bytearray("Y")
            for result in results:
                rt.append( '_')
                rt.append( 'D')
                rt.append( struct.pack( ">I", result['docno']))
                rt.append( 'W')
                rt.append( struct.pack( ">f", result['weight']))
                rt.append( 'I')
                rt.append( self.packedstr( result['docid']))
                rt.append( 'T')
                rt.append( self.packedstr( result['title']))
                rt.append( 'A')
                rt.append( self.packedstr( result['abstract']))
            # Send the result back:
            self.write_message( rt, binary=True)

        except Exception as e:
            rt = b"E" + self.errormsg( bytearray( e))
            self.write_message( rt, binary=True)

# [2] Dispatcher:
application = tornado.web.Application([
    # /subscribe in the URL triggers the handler for sending the global statistics
    # to the strusStatisticsServer:
    (r"/subscribe", SubscribeHandler),
    # /unsubscribe in the URL triggers the handler for sending the negative global statistics
    # to the strusStatisticsServer:
    (r"/unsubscribe", UnsubscribeHandler),
    # /insert in the URL triggers the handler for inserting documents:
    (r"/insert", InsertHandler),
    # /binquery in the URL triggers the handler for answering queries with a binary protocol:
    (r"/binquery", QueryHandlerBin),
])

# [3] Publish statistics:
@trollius.coroutine
def publishStatisticsCallback( itr):
    # Open connection to statistics server:
    nofTries = 10
    conn = None
    errmsg = ""
    while (nofTries > 0 and conn == None):
        try:
            conn = yield From( websocket.create_connection( 'ws://{}/publish'.format( statserver)))
        except IOError as e:
            print "connection so statistics server %s failed (%s) ... trying again (%u)" % (statserver, e,nofTries)
            nofTries -= 1
            errmsg = e
            time.sleep(3) # delays for 3 seconds

    # Iterate on statistics an publish them:
    if (conn == None):
        raise Exception( "connection error publishing statistics: %s" % errmsg)
    msg = itr.getNext()
    while (len(msg) > 0):
        yield From( conn.send( msg ))
        reply = yield From( conn.recv())
        if (reply[0] == 'E'):
            raise Exception( "error in statistis server: %s" % reply[ 1:])
        elif (reply[0] != 'Y'):
            raise Exception( "protocol error publishing statistics")
        msg = itr.getNext()
    conn.close()

def publishStatistics( itr):
    # Initiate the publishing of statistics as task in the eventloop
    tornado.ioloop.IOLoop.current().add_callback(
            callback=lambda: publishStatisticsCallback( itr))


# [4] Server main:
if __name__ == "__main__":
    try:
        parser = optparse.OptionParser()
        parser.add_option("-p", "--port", dest="port", default=myport,
                          help="Specify the port of this server as PORT (default %u" % myport,
                          metavar="PORT")
        parser.add_option("-s", "--statserver", dest="statserver", default=statserver,
                          help="Specify the port of the statistics server as ADDR (default %s" % statserver,
                          metavar="ADDR")
        parser.add_option("-P", "--publish-stats", action="store_true", dest="do_publish_stats", default=False,
                          help="Tell the node to publish the own storage statistics to the statistics server at startup")

        (options, args) = parser.parse_args()
        if len(args) > 0:
            parser.error("no arguments expected")
            parser.print_help()
        myport = options.port
        statserver = options.statserver
        if (statserver[0:].isdigit()):
            statserver = '{}:{}'.format( 'localhost', statserver)

        if (options.do_publish_stats):
            # Start publish local statistics:
            print( "Load local statistics to publish ...\n")
            publishStatistics( backend.getInitStatisticsIterator())

        # Start server:
        print( "Starting server ...\n")
        application.listen( myport )
        print( "Listening on port %u\n" % myport )
        tornado.ioloop.IOLoop.current().start()
        print( "Terminated\n")
    except Exception as e:
        print( e)


