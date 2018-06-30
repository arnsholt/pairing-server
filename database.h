#ifndef _DATABASE_H
#define _DATABASE_H

#include <exception>
#include <postgresql/libpq-fe.h>
#include <string>
#include <vector>

#include "types.pb.h"

class Database {
    public:
        Database(const char *dbname, const char *user, const char *password,
                const char *host = "127.0.0.1");
        ~Database();

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

        void getTournament(pairing_server::Tournament *t);
        pairing_server::Identification insertTournament(const pairing_server::Tournament *t);
        std::vector<pairing_server::Player> tournamentPlayers(const pairing_server::Identification *id);
        std::vector<pairing_server::Game> tournamentGames(const pairing_server::Identification *id);
        pairing_server::Identification insertPlayer(const pairing_server::Player *p);
    private:
        PGconn *db;
        void prepare(const char *name, const char *sql, int count);
        PGresult *execute(const char *stmt, int count, const char **values,
                const int *lengths, const int *formats, int resultFormat);
        void sqlDo(const char *sql);
        pairing_server::Player playerFromRow(PGresult *res, int i,
                const char *name_col = "player_name", const char *rating_col = "rating",
                const char *uuid_col = "uuid");
};

class DatabaseError : public std::exception {
    public:
        DatabaseError(const char *dbmsg) : msg(dbmsg) {}
        const char *what() const noexcept { return msg.c_str(); }

    private:
        std::string msg;
};

#endif
