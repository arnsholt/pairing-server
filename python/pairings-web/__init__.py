from flask import Flask, redirect, render_template, request, url_for
import socket
import struct

from requests_pb2 import Request, Response
import types_pb2

app = Flask(__name__)

@app.route('/')
def index():
    return render_template('index.html', msg='Hello world!')

@app.route('/tournament', methods=['POST'])
def tournament_post():
    req = Request()
    req.operation = Request.TOURNAMENT_CREATE
    # TODO: Data validation.
    req.tournament.name = request.form['name']
    req.tournament.rounds = int(request.form['rounds'])

    # TODO: Error handling?
    resp = send_request(req)
    return redirect(url_for('tournament', uuid=resp.tournaments[0].uuid,
        hmac=resp.tournaments[0].hmac))

@app.route('/tournament/<uuid>/')
@app.route('/tournament/<uuid>/<hmac>/')
def tournament(uuid, hmac=None):
    return "Tournament uuid %s, hmac %s" % (uuid, hmac)

def send_request(req):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", 1234))
    buf = req.SerializeToString()
    s.send(struct.pack("!L%ds" % len(buf), len(buf), buf))
    s.shutdown(socket.SHUT_WR)

    respsize = struct.unpack("!L", read_bytes(s, 4))[0]
    resp = Response()
    resp.ParseFromString(read_bytes(s, respsize))
    try:
        s.shutdown(socket.SHUT_RD)
        s.close()
    except OSError:
        pass
    return resp

def read_bytes(s, count):
    bytes_left = count
    data = b""
    while bytes_left > 0:
        got = s.recv(bytes_left)
        data = data + got
        bytes_left -= len(got)
    return data
