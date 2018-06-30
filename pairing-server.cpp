#include <exception>
#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <iostream>
#include <mutex>
#include <openssl/hmac.h>
#include <string>

#include "database.h"
#include "service.grpc.pb.h"

using namespace grpc;
using namespace pairing_server;

class PairingServerImpl final : public PairingServer::Service {
    public:
        explicit PairingServerImpl(const char *secret, const char *dbname,
                const char *dbuser, const char *dbpass) : secret(std::string(secret)),
                db(Database(dbname, dbuser, dbpass)) {}

        /* Generalized status creation:
         * Status(StatusCode code)
         * Status(StatusCode code, string &message)
         * Status(StatusCode code, string &message, string &detail)
         *
         * Available codes:
         * - OK
         * - CANCELLED
         * - UNKNOWN
         * - INVALID_ARGUMENT
         * - DEADLINE_EXCEEDED
         * - NOT_FOUND
         * - ALREADY_EXISTS
         * - PERMISSION_DENIED
         * - UNAUTHENTICATED
         * - RESOURCE_EXHAUSTED
         * - FAILED_PRECONDITION
         * - ABORTED
         * - OUT_OF_RANGE
         * - UNIMPLEMENTED
         * - INTERNAL
         * - UNAVAILABLE
         * - DATA_LOSS
         * - DO_NOT_USE
         */
        #define IDENTIFIED(id, type) if(!identified(*id)) \
            return Status(StatusCode::INVALID_ARGUMENT, "Missing or invalid UUID in " type " identification.")
        #define COMPLETE(obj, type) if(!complete(*obj)) \
            return Status(StatusCode::INVALID_ARGUMENT, "Incomplete " type ".")
        Status GetTournament(ServerContext *ctx, const Identification *req, Tournament *resp) override {
            IDENTIFIED(req, "tournament");
            resp->mutable_id()->set_uuid(req->uuid());
            db.getTournament(resp);
            return Status::OK;
        }

        Status GetPlayers(ServerContext *ctx, const Identification *req, ServerWriter<Player> *writer) override {
            IDENTIFIED(req, "tournament");
            for(Player &p: db.tournamentPlayers(req)) {
                writer->Write(p);
            }
            return Status::OK;
        }

        Status GetGames(ServerContext *ctx, const Identification *req, ServerWriter<Game> *writer) override {
            IDENTIFIED(req, "tournament");
            for(Game &g: db.tournamentGames(req)) {
                writer->Write(g);
            }
            return Status::OK;
        }

        Status CreateTournament(ServerContext *ctx, const Tournament *req, Identification *resp) override {
            COMPLETE(req, "tournament");
            *resp = db.insertTournament(req);
            sign(*resp);
            return Status::OK;
        }

        Status PairNextRound(ServerContext *ctx, const Identification *req, ServerWriter<Game> *writer) override {
            /* This is where the magic needs to happen, and we call into
             * bbpPairings. */
            return Status::OK;
        }

        Status SignupPlayer(ServerContext *ctx, const Player *req, Identification *resp) override {
            COMPLETE(req, "player");
            *resp = db.insertPlayer(req);
            sign(*resp);
            return Status::OK;
        }

        Status RegisterResult(ServerContext *ctx, const RegisterResultRequest *req, Identification *resp) override {
            return Status::OK;
        }

    private:
        std::string secret;
        std::mutex db_lock;
        Database db;

        std::string hmac(const Identification &id) {
            char buf[EVP_MAX_MD_SIZE];
            unsigned int len;
            unsigned char *ret = HMAC(EVP_sha256(), secret.c_str(),
                    secret.size(), (const unsigned char *) id.uuid().c_str(),
                    16, (unsigned char *) &buf[0], &len);
            // TODO: Better exception.
            if(!ret) throw nullptr;
            return std::string(&buf[0], len);
        }

        bool identified(const Identification &id) { return id.uuid().size() == 16; }
        bool authenticated(const Identification &id) { return hmac(id) == id.hmac().digest(); }

        void sign(Identification &id) {
            id.mutable_hmac()->set_algorithm("sha256");
            id.mutable_hmac()->set_digest(hmac(id));
        }

        bool complete(const Tournament &t) {
            return t.name().size() > 0 && t.rounds() > 0;
        }

        bool complete(const Player &p) {
            return p.name().size() > 0
                && p.rating() > 0
                && identified(p.tournament().id());
        }
};

class ArgError : public std::exception {
    public:
        ArgError(const char *m) : msg(m) {}
        ArgError(std::string m) : msg(m) {}
        const char *what() const noexcept { return msg.c_str(); }
    private:
        std::string msg;
};

const char *getArg(const char **argv, int i, int argc, const char *arg) {
    if(i >= argc) {
        throw ArgError(std::string("Missing argument to option --") + arg + ".\n");
    }
    return argv[i];
}

int main(int argc, const char **argv) {
    try {
        const char *dbname, *dbuser, *dbpass;
        const char *listen = "127.0.0.1";
        const char *port = "1234";
        for(int i = 1; i < argc; i++) {
            std::string arg(argv[i]);
            if(arg == "--help" || arg == "-h") {}
            else if(arg == "--db"     || arg == "-d") { dbname = getArg(argv, ++i, argc, "db"); }
            else if(arg == "--dbuser" || arg == "-u") { dbuser = getArg(argv, ++i, argc, "dbuser"); }
            else if(arg == "--dbpass" || arg == "-P") { dbpass = getArg(argv, ++i, argc, "dbpass"); }
            else if(arg == "--listen" || arg == "-l") {
                const char *listen = getArg(argv, ++i, argc, "listen");
            }
            else if(arg == "--port"   || arg == "-p") {
                const char *port = getArg(argv, ++i, argc, "port");
            }
            else if(arg == "--secret" || arg == "-s") {
                const char *secret = getArg(argv, ++i, argc, "secret");
            }
            else {
                throw ArgError(std::string("Unknown option ") + arg + ".\n");
            }
        }

        std::string address = listen + std::string(":") + port;
        const char *secret = "deadbeef"; // TODO: Read from secret file.
        PairingServerImpl service(secret, dbname, dbuser, dbpass);
        ServerBuilder builder;
        // TODO: Optionally SSL server credentials.
        builder.AddListeningPort(address, InsecureServerCredentials());
        builder.RegisterService(&service);
        std::unique_ptr<Server> server(builder.BuildAndStart());
        std::cout << "Waiting on server..." << std::endl;
        server->Wait();
    }
    catch(const std::exception &e) {
        std::cerr << e.what();
        return 1;
    }
    return 0;
}
