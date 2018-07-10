import grpc
from types_pb2 import Tournament, Player, Game
import service_pb2_grpc

channel = grpc.insecure_channel("localhost:1234")
stub = service_pb2_grpc.PairingServerStub(channel)

# Create tournament, read it back and make sure the two objects match up.
t1 = Tournament()
t1.name = "abcdefg"
t1.rounds = 1
ident = stub.CreateTournament(t1)
print("ok 1 - created tournament")

t2 = stub.GetTournament(ident)
if not (t1.name == t2.name and t1.rounds == t2.rounds):
    print("not ok 2 - reading back tournament")
    print(str(t1))
    print(str(t2))
else:
    print("ok 2 - reading back tournament")

# Create some players. We make sure to create an odd number of players, to
# make sure that inserting a bye works.
expected_names = ["a", "b", "c"]
for name in expected_names:
    p = Player()
    p.name = name
    p.rating = 1
    p.tournament.id.uuid = ident.uuid
    stub.SignupPlayer(p)
print("ok 3 - creating players")

got_names = [p.name for p in stub.GetPlayers(ident)]
if sorted(got_names) == expected_names:
    print("ok 4 - reading back players")
else:
    print("not ok 4 - reading back players")

games = [g for g in stub.PairNextRound(ident)]
print("ok 5 - pairing next round")

if len(games) == 2:
    print("ok 6 - number of games created")
else:
    print("not ok 6 - number of games created")

got_names = []
for g in games:
    got_names.append(g.white.name)
    if g.HasField("black"): got_names.append(g.black.name)
if sorted(got_names) == expected_names:
    print("ok 7 - assigning players to games")
else:
    print("not ok 7 - assigning players to games")

try:
    res = stub.PairNextRound(ident)
    for x in res:
        print("not ok 8 - pairing excess round")
except grpc.RpcError as e:
    print("ok 8 - pairing excess round")
