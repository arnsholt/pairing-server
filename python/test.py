from pairings_web.model import Connection

import re

def main():
    c = Connection(address="localhost:1234")

    t = c.new_tournament('Test script', 1)
    ok(True, "creating tournament")
    t_prime = c.tournament(t.id.uuid)
    is_eq(t_prime.name, 'Test script', 'tournament name')
    is_eq(t_prime.rounds, 1, 'tournament rounds')

    players = [('a', 1200),
               ('b', 800),
               ('c', 1600)]
    ids = []
    for name, rating in players:
        p = c.new_player(name, rating, t)
        ok(True, "signing up player")
        p_prime = c.player(p.id.uuid)
        is_eq(p_prime.name, name, "player name")
        is_eq(p_prime.rating, rating, "player rating")
        is_eq(p_prime.tournament.id.uuid, t.id.uuid, "player tournament")

tests = 0
def ok(condition, desc=None):
    global tests
    tests += 1
    output = "ok %d" % tests
    if desc is not None:
        output = "%s - %s" % (output, desc)
    if not condition:
        output = "not " + condition
    print(output)

def is_eq(got, expected, desc=None):
    success = got == expected
    ok(success, desc)
    if not success:
        diag("Got: %s" % str(got))
        diag("Expected: %s" % str(expected))

def diag(msg):
    print(re.sub("(?ms)^", "# ", msg))

if __name__ == '__main__':
    main()
