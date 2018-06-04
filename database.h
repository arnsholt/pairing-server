#ifndef _DATABASE_H
#define _DATABASE_H

#include <exception>
#include <postgresql/libpq-fe.h>
#include <string>

#include "types.pb.h"

class Database {
    public:
        Database(const char *dbname, const char *user, const char *password,
                const char *host = "127.0.0.1");
        ~Database();

        void begin();
        void commit();
        void rollback();

        void getTournament(pairing_server::Tournament *t);
        void insertTournament(pairing_server::Tournament *t);

        void insertPlayer(pairing_server::TournamentPlayer *p);
    private:
        PGconn *db;
        void prepare(const char *name, const char *sql, int count);
        PGresult *execute(const char *stmt, int count, const char **values,
                const int *lengths, const int *formats, int resultFormat);
        void sqlDo(const char *sql);
};

class DatabaseError : std::exception {
    public:
        DatabaseError(const char *dbmsg);
    private:
        std::string msg;
};

#endif
