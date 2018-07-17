import grpc
from google.protobuf.message import Message
import itertools
from operator import attrgetter

import pairings_web.proto.types_pb2 as types
from .proto.service_pb2_grpc import PairingServerStub

class Connection:
    def __init__(self, *, address):
        self.stub = PairingServerStub(grpc.insecure_channel("localhost:1234"))

    def new_tournament(self, name, rounds):
        t = types.Tournament()
        t.name = name
        t.rounds = rounds
        t.id.CopyFrom(self.stub.CreateTournament(t))
        return self.model(t)

    def tournament(self, uuid, hmac=None):
        return self.model(self.stub.GetTournament(self.ident(uuid, hmac)))

    def tournament_games(self, uuid, hmac=None):
        return self.models(self.stub.GetTournamentGames(self.ident(uuid, hmac)))

    def pair_next_round(self, uuid, hmac):
        return self.models(self.stub.PairNextRound(self.ident(uuid, hmac)))

    def new_player(self, name, rating, tournament):
        p = types.Player()
        p.name = name
        p.rating = rating
        p.tournament.id.uuid = tournament.id.uuid
        p.id.CopyFrom(self.stub.SignupPlayer(p))
        return self.model(p)

    def players(self, uuid, hmac=None):
        return self.models(self.stub.GetPlayers(self.ident(uuid, hmac)))

    def player(self, uuid, hmac=None):
        return self.model(self.stub.GetPlayer(self.ident(uuid, hmac)))

    def player_games(self, uuid, hmac=None):
        return self.models(self.stub.PlayerGames(self.ident(uuid, hmac)))

    def game(self, uuid, hmac=None):
        return self.model(self.stub.GetGame(self.ident(uuid, hmac)))

    def model(self, proto):
        return ModelObject.on(proto, self)

    def models(self, protos):
        return [self.model(proto) for proto in protos]

    def ident(self, uuid, hmac=None):
        ident = types.Identification()
        ident.uuid = uuid
        if hmac is not None:
            ident.hmac.algorithm = "sha256"
            ident.hmac.digest = hmac
        return ident

class modelattribute:
    def __init__(self, name = None):
        self.attr = name

    def __get__(self, instance, owner):
        value = getattr(instance._model, self.attr)
        if isinstance(value, Message):
            value = ModelObject.on(value, instance._connection)
        return value

    def __set__(self, instance, value):
        if isinstance(value, ModelObject):
            value = value._model
        setattr(instance._model, self.attr, value)

    def __delete__(self, instance):
        delattr(instance._model, self.attr)

    def __set_name__(self, owner, name):
        self.attr = name

class ModelObject:
    @classmethod
    def on(cls, proto_object, connection):
        return cls.models[proto_object.__class__](model=proto_object,
                connection=connection)

    def __init__(self, *, model, connection):
        self._model = model
        self._connection = connection

    def has_field(self, field):
        return self._model.HasField(field)

class Hmac(ModelObject):
    algorithm = modelattribute("algorithm")
    digest = modelattribute("digest")

class Identification(ModelObject):
    uuid = modelattribute("uuid")
    hmac = modelattribute("hmac")

    def id_pair(self):
        return self.uuid, self.hmac.digest if self.has_field("hmac") else None

    def link_fragment(self):
        if self.has_field("hmac"):
            return "%s/%s" % (self.uuid.hex(), self.hmac.digest.hex())
        else:
            return self.uuid.hex()

class DomainObject(ModelObject):
    id = modelattribute("id")

    def signed(self):
        return self.id.has_field("hmac")

    @staticmethod
    def link_prefix():
        raise NotImplementedError

    def link(self):
        return "/%s/%s" % (self.link_prefix(), self.id.link_fragment())

class Tournament (DomainObject):
    name = modelattribute("name")
    rounds = modelattribute("rounds")

    @staticmethod
    def link_prefix(): return "tournament"

    def games(self):
        return self._connection.tournament_games(*self.id.id_pair())

    def games_by_round(self):
        # Games, sorted by round, with reverse sort on rating as tie breaker.
        roundget = attrgetter("round")
        games = self.games()
        if not games: return None
        games = sorted(
                    sorted(games, key=Game.sort_rating, reverse=True),
                    key=roundget)
        return itertools.groupby(games, roundget)

    def players(self):
        return self._connection.players(*self.id.id_pair())

class Player (DomainObject):
    name = modelattribute("name")
    tournament = modelattribute("tournament")
    rating = modelattribute("rating")
    withdrawn = modelattribute("withdrawn")
    expelled = modelattribute("expelled")

    @staticmethod
    def link_prefix(): return "player"

    def description(self):
        return "%s (%d)" % (self.name, self.rating)

    def games(self):
        return self._connection.player_games(*self.id.id_pair())

class Game (DomainObject):
    tournament = modelattribute("tournament")
    round = modelattribute("round")
    white = modelattribute("white")
    black = modelattribute("black")
    result = modelattribute("result")

    @staticmethod
    def link_prefix(): return "game"

    def description(self):
        return "%s vs. %s" % (self.white.description(),
                self.black.description() if self.has_black() else "Noone")

    def has_black(self):
        return self.has_field("black")

    def sort_rating(self):
        # For sorting games we use the maximum of the two players' ratings, or
        # 0 for byes/no-shows.
        return max(self.white.rating, self.black.rating) if self.has_black() else 0



ModelObject.models = {
        types.Hmac: Hmac,
        types.Identification: Identification,
        types.Tournament: Tournament,
        types.Player: Player,
        types.Game: Game}
