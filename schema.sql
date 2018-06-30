DROP TABLE IF EXISTS game;
DROP TABLE IF EXISTS player;
DROP TABLE IF EXISTS tournament;

CREATE TABLE tournament (
    id SERIAL PRIMARY KEY,
    uuid UUID NOT NULL UNIQUE DEFAULT uuid_generate_v4(),
    name TEXT NOT NULL,
    rounds INTEGER NOT NULL CHECK (rounds > 0));

CREATE TABLE player (
    id SERIAL PRIMARY KEY,
    uuid UUID NOT NULL UNIQUE DEFAULT uuid_generate_v4(),
    tournament INTEGER NOT NULL REFERENCES tournament(id),
    player_name TEXT NOT NULL,
    rating INTEGER NOT NULL CHECK (rating > 0),
    UNIQUE (tournament, player_name));
CREATE INDEX player_tournament_idx ON player(tournament);

CREATE TABLE game (
    id SERIAL PRIMARY KEY,
    uuid UUID NOT NULL UNIQUE DEFAULT uuid_generate_v4(),
    round INTEGER NOT NULL,
    white INTEGER NOT NULL REFERENCES player(id),
    /* If a player gets a bye, we create a game for them where black is NULL. */
    black INTEGER REFERENCES player(id),
    result INTEGER CHECK (result IS NULL OR result IN (1, -1, 0)));
CREATE INDEX game_white_idx ON game(white);
CREATE INDEX game_black_idx ON game(black);
