#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <mutex>
#include <openssl/hmac.h>

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
        Status GetTournament(ServerContext *ctx, Identification *req, Tournament *resp) {
            if(!identified(req)) return Status(StatusCode::INVALID_ARGUMENT, "Missing or invalid UUID in tournament identification.");
            resp->mutable_id()->set_uuid(req->uuid());
            transaction([=]() -> void { db.getTournament(resp); });
            return Status::OK;
        }

        Status CreateTournament(ServerContext *ctx, Tournament *req, Identification *resp) {
            if(!complete(req)) return Status(StatusCode::INVALID_ARGUMENT, "Incomplete tournament");
            transaction([=]() -> void { db.insertTournament(req); });
            resp->set_uuid(req->id().uuid());
            sign(resp);
            return Status::OK;
        }

        Status SignupPlayer(ServerContext *ctx, Identification *req, TournamentPlayer *resp) {
        }

    private:
        std::string secret;
        std::mutex db_lock;
        Database db;

        std::string hmac(Identification *id) {
            char buf[EVP_MAX_MD_SIZE];
            unsigned int len;
            unsigned char *ret = HMAC(EVP_sha256(), secret.c_str(),
                    secret.size(), (const unsigned char *) id->uuid().c_str(),
                    16, (unsigned char *) &buf[0], &len);
            // TODO: Better exception.
            if(!ret) throw nullptr;
            return std::string(&buf[0], len);
        }

        bool identified(Identification *id) { return id->uuid().size() == 16; }
        bool authenticated(Identification *id) { return hmac(id) == id->hmac().digest(); }

        void sign(Identification *id) {
            id->mutable_hmac()->set_algorithm("sha256");
            id->mutable_hmac()->set_digest(hmac(id));
        }

        bool complete(Tournament *t) { return t->name().size() > 0 && t->rounds() > 0; }

        template<typename Func>
        void transaction(Func cb) {
            try {
                cb();
            }
            catch(...) {
                db.rollback();
                throw;
            }
            db.commit();
        }
};

int main(int argc, char **argv) {
    std::string address("0.0.0.0:1234"); // TODO: Read from args.
    const char *secret = "deadbeef"; // TODO: Read from secret file.
    PairingServerImpl service(secret, NULL, NULL, NULL);
    ServerBuilder builder;
    // TODO: Optionally SSL server credentials.
    builder.AddListeningPort(address, InsecureServerCredentials());
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    server->Wait();
    return 0;
}
