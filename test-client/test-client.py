import grpc
from types_pb2 import Tournament, TournamentPlayer, TournamentGame
import service_pb2_grpc

channel = grpc.insecure_channel("localhost:1234")
stub = service_pb2_grpc.PairingServerStub(channel)

# Create tournament
t1 = Tournament()
t1.name = "abcdefg"
t1.rounds = 1
ident = stub.CreateTournament(t1)

t2 = stub.GetTournament(ident)
if not (t1.name == t2.name and t1.rounds == t2.rounds):
    print(str(t1))
    print(str(t2))
else:
    print("ok")

# Read tournament back and make sure it's identical.
# Sign up some players, read back player list and make sure they match up.
# Pair round and read back games. Run some kind of basic sanity check?
# Pair *another* round, and make sure we get an error (since we've specified
# only a single round).
