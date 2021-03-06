#ifndef _DATABASE_H
#define _DATABASE_H

#include <exception>
#include <postgresql/libpq-fe.h>
#include <string>
#include <vector>

#include "service.pb.h"
#include "types.pb.h"

class Database {
    public:
        Database();
        Database(const char *dbname, const char *user, const char *password,
                const char *host = "127.0.0.1");
        ~Database();

        void connect();

        void begin();
        void commit();
        void rollback();

        template<typename Func>
        void transaction(Func cb) {
            begin();
            try {
                cb();
            }
            catch(...) {
                rollback();
                throw;
            }
            commit();
        }

        // Operations on tournaments:
        bool getTournament(pairing_server::Tournament *t);
        int nextRound(const pairing_server::Identification *id);
        std::vector<pairing_server::Player> tournamentPlayers(const pairing_server::Identification *id);
        std::vector<pairing_server::Game> tournamentGames(const pairing_server::Identification *id);
        pairing_server::Identification insertTournament(const pairing_server::Tournament *t);

        // Operations on players:
        bool getPlayer(pairing_server::Player *p);
        std::vector<pairing_server::Game> playerGames(const pairing_server::Identification *id);
        pairing_server::Identification insertPlayer(const pairing_server::Player *p);

        // Operations on games:
        bool getGame(pairing_server::Game *g);
        pairing_server::Identification insertGame(const pairing_server::Game *g);
        void registerResult(const pairing_server::Identification &gameId, pairing_server::Result result);

    private:
        const char *dbname = NULL;
        const char *user = NULL;
        const char *password = NULL;
        const char *host = NULL;
        PGconn *db = NULL;
        void prepare(const char *name, const char *sql, int count);
        PGresult *execute(const char *stmt, int count, const char **values,
                const int *lengths, const int *formats, int resultFormat,
                int minRows = 0, int maxRows = -1);
        void sqlDo(const char *sql);
};

class DatabaseError : public std::exception {
    public:
        DatabaseError(const char *dbmsg) : msg(dbmsg) {}
        const char *what() const noexcept { return msg.c_str(); }

    private:
        std::string msg;
};

#endif
