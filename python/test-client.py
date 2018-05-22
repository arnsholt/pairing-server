import argparse
from requests_pb2 import Request, Response
import socket
import struct
import types_pb2

def read_bytes(s, count):
    print("Reading %d bytes" % count)
    bytes_left = count
    data = b""
    while bytes_left > 0:
        got = s.recv(bytes_left)
        data = data + got
        bytes_left -= len(got)
    print("done")
    return data

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--host', default='localhost')
    parser.add_argument('--port', default=1234)
    args = parser.parse_args()

    req = Request()
    #req.operation = Request.PING
    req.operation = Request.TOURNAMENT_CREATE
    req.tournament.name = "Hello from Python!"
    req.tournament.rounds = 42

    print("Hello world! (%s, %d)" % (args.host, args.port))
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((args.host, args.port))

    buf = req.SerializeToString()
    data = struct.pack("!L%ds" % len(buf), req.ByteSize(), buf)
    print(str(len(data)))
    s.send(data)
    s.shutdown(socket.SHUT_WR)

    respsize = struct.unpack("!L", read_bytes(s, 4))[0]
    resp = Response()
    resp.ParseFromString(read_bytes(s, respsize))
    print("status=" + str(resp.status))
    print("reasons=" + str(resp.reasons))
    print("reasons=" + str(resp.tournaments[0].uuid))
    print("reasons=" + str(resp.tournaments[0].hmac))

    s.shutdown(socket.SHUT_RD)
    s.close()

if __name__ == "__main__":
    main()
