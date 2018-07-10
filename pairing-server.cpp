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
        #define IDENTIFIED(id, type) if(!identified(id)) \
            return Status(StatusCode::INVALID_ARGUMENT, "Missing or invalid UUID in " type " identification.")
        #define AUTHENTICATED(id) ({ Status status = authenticated(id); \
                if(status.error_code() != StatusCode::OK) return status; })
        #define COMPLETE(obj, type) if(!complete(obj)) \
            return Status(StatusCode::INVALID_ARGUMENT, "Incomplete " type ".")
        #define HANDLER_PROLOGUE try {
        #define HANDLER_EPILOGUE } \
                                 catch(DatabaseError e) { \
                                     std::cerr << "Got DB exception (" << __FILE__ << ":" << __LINE__ << "): " << e.what(); \
                                     return Status(StatusCode::INTERNAL, "Database error", e.what()); \
                                 } \
                                 catch(std::exception e) { \
                                     std::cerr << "Got other exception (" << __FILE__ << ":" << __LINE__ << "): " << e.what(); \
                                     return Status(StatusCode::INTERNAL, "Other error", e.what()); \
                                 }
        Status GetTournament(ServerContext *ctx, const Identification *req, Tournament *resp) override {
            IDENTIFIED(*req, "tournament");
            resp->mutable_id()->set_uuid(req->uuid());
            db.getTournament(resp);
            return Status::OK;
        }

        Status GetPlayers(ServerContext *ctx, const Identification *req, ServerWriter<Player> *writer) override {
            IDENTIFIED(*req, "tournament");
            for(Player &p: db.tournamentPlayers(req)) {
                writer->Write(p);
            }
            return Status::OK;
        }

        Status GetGames(ServerContext *ctx, const Identification *req, ServerWriter<Game> *writer) override {
            IDENTIFIED(*req, "tournament");
            for(Game &g: db.tournamentGames(req)) {
                writer->Write(g);
            }
            return Status::OK;
        }

        Status CreateTournament(ServerContext *ctx, const Tournament *req, Identification *resp) override {
            COMPLETE(*req, "tournament");
            *resp = db.insertTournament(req);
            sign(*resp);
            return Status::OK;
        }

        Status PairNextRound(ServerContext *ctx, const Identification *req, ServerWriter<Game> *writer) override {
            /* This is where the magic needs to happen, and we call into
             * bbpPairings. For now, to get something running to test
             * end-to-end, we just do a dummy pairing [utting the players
             * together in the order the DB returns them. */
            HANDLER_PROLOGUE
            IDENTIFIED(*req, "tournament");
            AUTHENTICATED(*req);
            Game g;
            Tournament t;
            *(t.mutable_id()) = *req;
            db.getTournament(&t);
            uint32_t nextRound = db.nextRound(req);
            if(t.rounds() < nextRound) {
                return Status(StatusCode::INVALID_ARGUMENT, "Last round paired");
            }

            g.mutable_tournament()->mutable_id()->set_uuid(req->uuid());
            g.set_round(nextRound);
            bool white = true;
            try {
                db.transaction([&] {
                    for(Player &p: db.tournamentPlayers(req)) {
                        if(white) {
                            *(g.mutable_white()) = p;
                        }
                        else {
                            *(g.mutable_black()) = p;
                            Identification id = db.insertGame(&g);
                            sign(id);
                            *(g.mutable_id()) = id;
                            writer->Write(g);
                            g.clear_white();
                            g.clear_black();
                        }
                        white = !white;
                    }
                    if(g.has_white()) {
                        Identification id = db.insertGame(&g);
                        sign(id);
                        *(g.mutable_id()) = id;
                        writer->Write(g);
                    }});
            }
            catch(DatabaseError &e) {
                return Status(StatusCode::INTERNAL, "Database error", e.what());
            }
            return Status::OK;
            HANDLER_EPILOGUE
        }

        Status SignupPlayer(ServerContext *ctx, const Player *req, Identification *resp) override {
            COMPLETE(*req, "player");
            *resp = db.insertPlayer(req);
            sign(*resp);
            return Status::OK;
        }

        Status RegisterResult(ServerContext *ctx, const RegisterResultRequest *req, Identification *resp) override {
            IDENTIFIED(req->gameid(), "game");
            AUTHENTICATED(req->gameid());
            COMPLETE(*req, "game");
            db.registerResult(req->gameid(), req->result());
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
        Status authenticated(const Identification &id) {
            if(!complete(id.hmac()))
                return Status(StatusCode::INVALID_ARGUMENT, "Incomplete HMAC signature in identification");
            /* Possibly a slight abuse of the error codes here, but oh well.
             * Since we don't have users as such I've chosen to interpret
             * passing an HMAC object as being "logged in" and passing a valid
             * HMAC object (for the resource) as being logged in as a user
             * with permissions to the object. */
            if(!id.has_hmac())
                return Status(StatusCode::UNAUTHENTICATED, "Missing HMAC signature in identification");
            if(hmac(id) != id.hmac().digest())
                return Status(StatusCode::PERMISSION_DENIED, "Invalid HMAC signature in identification");
            return Status::OK;
        }

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

        bool complete(const Hmac &hmac) {
            return hmac.algorithm().size() > 0 && hmac.digest().size() > 0;
        }

        bool complete(const RegisterResultRequest &req) {
            return req.result() > 0;
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
