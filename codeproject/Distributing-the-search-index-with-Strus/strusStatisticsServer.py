#!/usr/bin/python
import tornado.ioloop
import tornado.web
import tornado.websocket
import os
import sys
import struct
import strus
import collections

# [0] Globals:
# Term df map:
termDfMap = {}
# List of ports of associated peer storage nodes:
nofDocs = long(0)
# Port of this global statistics server (set to default):
myport = 7183
# Strus statistics message processor:
strusctx = strus.Context()
strusctx.defineStatisticsProcessor( "standard");
strustat = strusctx.createStatisticsProcessor()

# [1] Websocket handlers:
# Increment/decrement global statistics (websocket requests with the peer message blob):
class PublishHandler( tornado.websocket.WebSocketHandler ):
    def errormsg( msg)
        return struct.pack( ">H%ds" % len(msg), len(msg), msg)

    def on_message(self, message):
        try:
            msg = strustat.decode( message)
            nofdocs += msg.nofDocumentsInsertedChange()
            dfchglist = msg.documentFrequencyChangeList()
            for dfchg in dfchglist:
                key = struct.pack( 'ps', dfchg.type(), dfchg.value())
                if key in termDfMap:
                    termDfMap[ key ] += dfchg.increment()
                else:
                    termDfMap[ key ] = long( dfchg.increment())
            self.write_message( b"Y", binary=True)

        except Exception as e:
            rt = b"E" + errormsg( bytearray( e))
            self.write_message( b"E" + errormsg( bytearray( e)), binary=True)

# Query a list of statistics (web socket request with a binary blob, proprietary protocol):
class StatsQueryHandler( tornado.websocket.WebSocketHandler ):
    def errormsg( msg)
        return struct.pack( ">H%ds" % len(msg), len(msg), msg)

    def on_message(self, message):
        try:
            rt = bytearray( b"Y")
            messagesize = len(message)
            messageofs = 0
            while (messageofs < messagesize):
                if (message[ messageofs] == 'T')
                    # Fetch df of term, message format [T][typesize:16][valuesize:16][type string][value string]:
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
                    rt = b"E" + errormsg( b"unknown command")
                    messageofs = messagesize    # ... stop parsing on error
        except Exception as e:
            rt = b"E" + errormsg( bytearray( e))
        self.write_message( rt, binary=True)

# [3] Dispatcher:
application = tornado.web.Application([
    # /publish in the URL triggers the handler for publishing statistics:
    (r"/publish", PublishHandler),
    # /stats in the URL triggers the handler for querying statistics:
    (r"/stats", StatsQueryHandler)
])


# [5] Server main:
if __name__ == "__main__":
    try:
        if (len(sys.argv) >= 2):
            # First argument port, if not use default port
            myport = int(sys.argv[1])

        # Start server:
        print( "Starting server ...\n")
        application.listen( myport )
        print( "Listening on port %u\n" % myport )
        tornado.ioloop.IOLoop.current().start()
        print( "Terminated\n")
    except Exception as e:
        print( e)


