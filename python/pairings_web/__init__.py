from flask import Flask, redirect, render_template, request, url_for
import grpc

import pairings_web.model as model
from .model import Tournament, Player, Game, Identification

from .proto.types_pb2 import Tournament, Player, Game, Identification
from .proto.service_pb2_grpc import PairingServerStub

app = Flask(__name__)
connection = model.Connection(address="localhost:1234")

@app.route('/')
def index():
    return render_template('index.html', msg='Hello world!')

@app.route('/tournament', methods=['POST'])
def tournament_post():
    tournament = connection.new_tournament(request.form['name'],
        request.form['rounds'])
    return redirect(url_for('tournament', uuid=tournament.uuid.hex(),
        hmac=tournament.hmac.digest.hex()))

@app.route('/tournament/<uuid>/')
@app.route('/tournament/<uuid>/<hmac>/')
def tournament(uuid, hmac=None):
    uuid = bytes.fromhex(uuid)
    if hmac is not None:
        hmac = bytes.fromhex(hmac)
    return render_template('tournament.html',
            tournament=connection.tournament(uuid, hmac))

@app.route('/player/<uuid>/')
@app.route('/player/<uuid>/<hmac>/')
def player(uuid, hmac=None):
    uuid = bytes.fromhex(uuid)
    if hmac is not None:
        hmac = bytes.fromhex(hmac)
    return render_template('player.html',
            player=connection.player(uuid, hmac))
