from flask import Flask, redirect, render_template, request, url_for
import grpc

from .types_pb2 import Tournament, Player, Game
from .service_pb2_grpc import PairingServerStub

app = Flask(__name__)
stub = PairingServerStub(grpc.insecure_channel("localhost:1234"))

@app.route('/')
def index():
    return render_template('index.html', msg='Hello world!')

@app.route('/tournament', methods=['POST'])
def tournament_post():
    # TODO: Data validation.
    tournament = Tournament()
    tournament.name = request.form['name']
    tournament.rounds = int(request.form['rounds'])

    try:
        response = stub.CreateTournament(tournament)
    except:
        # TODO: Error handling.
        raise

    return redirect(url_for('tournament', uuid=response.uuid.hex(),
        hmac=response.hmac.digest.hex()))

@app.route('/tournament/<uuid>/')
@app.route('/tournament/<uuid>/<hmac>/')
def tournament(uuid, hmac=None):
    write_mode = hmac is not None
    return render_template('tournament.html', write_mode=write_mode)
