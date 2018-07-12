from flask import Flask, redirect, render_template, request, url_for
import grpc

from .types_pb2 import Tournament, Player, Game, Identification
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
    ident = Identification()
    ident.uuid = bytes.fromhex(uuid)
    tournament = stub.GetTournament(ident)
    if hmac is not None:
        tournament.id.hmac.algorithm = "sha256"
        tournament.id.hmac.digest = bytes.fromhex(hmac)
    games = [g for g in stub.GetGames(ident)]
    players = [p for p in stub.GetPlayers(ident)]

    players.sort(key=lambda p: p.rating, reverse=True)
    # TODO: Group games into rounds, and figure out a good way to sort the
    # games in a round.

    return render_template('tournament.html', tournament=tournament,
            games=games, players=players)
