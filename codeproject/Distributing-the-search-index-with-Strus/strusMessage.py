#!/usr/bin/python
import tornado.ioloop
import tornado.gen
import tornado.tcpclient
import tornado.tcpserver
import signal
import os
import sys
import struct
import binascii

class TcpConnection( object):
    def __init__(self, stream, command_callback):
        self.stream = stream
        self.command_callback = command_callback

    @tornado.gen.coroutine
    def on_connect(self):
        try:
            while (True):
                print('+++ TcpConnection on_connect 1')
                msgsizemsg = yield self.stream.read_bytes( struct.calcsize(">I"))
                print( binascii.hexlify( bytes( msgsizemsg)))
                print('+++ TcpConnection on_connect 2')
                (msgsize,) = struct.unpack( ">I", msgsizemsg)
                print('+++ TcpConnection on_connect 3 %u' % msgsize)
                msg = yield self.stream.read_bytes( msgsize)
                print('+++ TcpConnection on_connect 4')
                reply = yield self.command_callback( msg)
                print('+++ TcpConnection on_connect 5')
                yield self.stream.write( struct.pack( ">I", len(reply)) + bytes(reply));
                print('+++ TcpConnection on_connect 6')
        except tornado.iostream.StreamClosedError:
            pass

class RequestServer( tornado.tcpserver.TCPServer):
    def __init__(self, command_callback, shutdown_callback):
        tornado.tcpserver.TCPServer.__init__(self)
        self.command_callback = command_callback
        self.shutdown_callback = shutdown_callback
        self.io_loop = tornado.ioloop.IOLoop.current()

    def do_shutdown( self, signum, frame):
        print('Shutting down')
        self.shutdown_callback()
        self.io_loop.stop()

    @tornado.gen.coroutine
    def handle_stream( self, stream, address):
        connection = TcpConnection( stream, self.command_callback)
        yield connection.on_connect()

    def start( self, port):
        host = "0.0.0.0"
        self.listen( port, host)
        print("Listening on %s:%d..." % (host, port))
        signal.signal( signal.SIGINT, self.do_shutdown)
        self.io_loop.start()


class RequestClient( tornado.tcpclient.TCPClient):
    @tornado.gen.coroutine
    def issueRequest( self, stream, msg):
        print "issueRequest 0"
        blob = struct.pack( ">I", len(msg)) + bytes(msg)
        print "issueRequest 1"
        stream.write( blob);
        print "issueRequest 2"
        replysizemsg = yield stream.read_bytes( struct.calcsize(">I"))
        print "issueRequest 3"
        print( binascii.hexlify( bytes( replysizemsg)))
        print "issueRequest 4"
        (replysize,) = struct.unpack( ">I", replysizemsg)
        print "issueRequest 5 %u" % replysize
        reply = yield stream.read_bytes( replysize)
        print "issueRequest 6"
        raise tornado.gen.Return( reply)







