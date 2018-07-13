import grpc
from google.protobuf.message import Message

import pairings_web.proto.types_pb2 as types
from .proto.service_pb2_grpc import PairingServerStub

class Connection:
    def __init__(self, *, address):
        self.stub = PairingServerStub(grpc.insecure_channel("localhost:1234"))

    def new_tournament(self, name, rounds):
        t = types.Tournament()
        t.name = name
        t.rounds = rounds
        t.id = self.stub.CreateTournament(t)
        return self.model(t)

    def tournament(self, uuid, hmac=None):
        t = self.model(self.stub.GetTournament(self.ident(uuid, hmac)))
        return t

    def games(self, uuid, hmac=None):
        return [self.model(g) for g in self.stub.GetGames(self.ident(uuid, hmac))]

    def players(self, uuid, hmac=None):
        return [self.model(p) for p in self.stub.GetPlayers(self.ident(uuid, hmac))]

    def model(self, proto):
        return ModelObject.on(proto, self)

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

class Tournament (ModelObject):
    id = modelattribute("id")
    name = modelattribute("name")
    rounds = modelattribute("rounds")

    def games(self):
        return self._connection.games(*self.id.id_pair())

    def players(self):
        return self._connection.players(*self.id.id_pair())

class Player (ModelObject):
    id = modelattribute("id")
    name = modelattribute("name")
    tournament = modelattribute("tournament")
    rating = modelattribute("rating")
    withdrawn = modelattribute("withdrawn")
    expelled = modelattribute("expelled")

    def description(self):
        return "%s (%d)" % (self.name, self.rating)

class Game (ModelObject):
    id = modelattribute("id")
    tournament = modelattribute("tournament")
    round = modelattribute("round")
    white = modelattribute("white")
    black = modelattribute("black")
    result = modelattribute("result")

    def description(self):
        return "Round %d: %s vs. %s" % (self.round,
                self.white.description(), self.black.description() if
                self.has_field("black") else "Noone")

ModelObject.models = {
        types.Hmac: Hmac,
        types.Identification: Identification,
        types.Tournament: Tournament,
        types.Player: Player,
        types.Game: Game}
