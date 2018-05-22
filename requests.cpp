#include <arpa/inet.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "err.h"
#include "hmac.h"
#include "requests.h"

extern "C" {
    using namespace pairing_server;
    using namespace google::protobuf;
    using namespace google::protobuf::io;

    /* Requests: */
    Request *request_read(int socket) {
        Request *req;
        FileInputStream stream(socket);
        const void *buffer;
        int bufsize;
        uint32_t msgsize;
        bool success;

        success = stream.Next(&buffer, &bufsize);
        if(!success) {
            err("Failed to obtain buffer for reading request size.\n");
            return NULL;
        }
        if(bufsize < sizeof(msgsize)) {
            err("Obtained buffer is smaller than message size type.\n");
            return NULL;
        }
        msgsize = ntohl(*(uint32_t *) buffer);
        stream.BackUp(bufsize - sizeof(msgsize));

        req = new Request;

        success = req->ParseFromBoundedZeroCopyStream(&stream, msgsize);
        if(!success) {
            err("Error while reading request.\n");
            delete req;
            return NULL;
        }

        return req;
    }

    void request_free(Request *req) {
        delete req;
    }

    Tournament *request_tournament(Request *req) {
        return req->has_tournament()?
            req->mutable_tournament():
            NULL;
    }

    TournamentPlayer *request_player(Request *req) {
        return req->has_player()?
            req->mutable_player():
            NULL;
    }

    TournamentGame *request_game(Request *req) {
        return req->has_game()?
            req->mutable_game():
            NULL;
    }

    Operation request_operation(Request *req) {
        return req->operation();
    }

    /* Responses: */
    Response *response_new() {
        return new Response;
    }

    void response_free(Response *resp) {
        delete resp;
    }

    void response_set_status(Response *resp, uint32_t status) {
        resp->set_status(static_cast<Status>(status));
    }

    bool response_write(Response *resp, int socket) {
        FileOutputStream stream(socket);
        bool success;
        void *buffer;
        int bufsize;
        uint32_t msgsize = resp->ByteSize();

        success = stream.Next(&buffer, &bufsize);
        if(!success) {
            err("Failed to obtain buffer for message size\n");
            return false;
        }
        if(bufsize < sizeof(msgsize)) {
            err("Buffer is too small for message size\n");
            return false;
        }
        *((int *) buffer) = htonl(msgsize);
        stream.BackUp(bufsize - sizeof(msgsize));
        success = resp->SerializeToZeroCopyStream(&stream);

        if(success) {
            success = stream.Flush();
            if(!success) {
                err("Error while flushing output stream.\n");
                return false;
            }
            else {
                return true;
            }
        }
        else {
            err("Error while writing response to socket.\n");
            return false;
        }
    }

    void response_add_tournament(Response *resp, Tournament *t) {
        Tournament *added = resp->add_tournaments();
        *added = *t;
    }

    void response_add_player(Response *resp, TournamentPlayer *p) {
        TournamentPlayer *added = resp->add_players();
        *added = *p;
    }

    void response_add_game(Response *resp, TournamentGame *g) {
        TournamentGame *added = resp->add_games();
        *added = *g;
    }

    void response_add_reason(Response *resp, Reason reason) {
        resp->add_reasons(reason);
    }

    /* Tournaments: */
    bool tournament_complete(Tournament *t) {
        return t->name().size() > 0 && t->rounds() > 0;
    }

    bool tournament_identified(Tournament *t) {
        return t->uuid().size() > 0;
    }

    bool tournament_authenticated(Tournament *t) {
        return tournament_identified(t)
            && t->hmac().size() > 0
            && hmac_validate(t->uuid().c_str(),  t->hmac().c_str());
    }

    const char *tournament_name(Tournament *t) {
        return t->name().c_str();
    }

    uint32_t tournament_rounds(Tournament *t) {
        return t->rounds();
    }

    const char *tournament_uuid(Tournament *t) {
        return t->uuid().c_str();
    }

    const char *tournament_hmac(Tournament *t) {
        return t->hmac().c_str();
    }

    void tournament_set_name(Tournament *t, const char *name) {
        t->set_name(name);
    }

    void tournament_set_rounds(Tournament *t, uint32_t rounds) {
        t->set_rounds(rounds);
    }

    void tournament_set_uuid(Tournament *t, const char *uuid, const char *digest) {
        t->set_uuid(uuid);
        if(!digest) {
            char *h = hmac(uuid);
            t->set_hmac(h);
            free(h);
        }
        else {
            t->set_hmac(digest);
        }
    }

    /* Players: */
    bool player_complete(TournamentPlayer *p) {
        return p->name().size() > 0 && p->has_tournament() && p->rating() > 0
            && tournament_identified(p->mutable_tournament());
    }

    bool player_identified(TournamentPlayer *p) {
        return p->uuid().size() > 0;
    }

    bool player_authenticated(TournamentPlayer *p) {
        return player_identified(p)
            && p->hmac().size() > 0
            && hmac_validate(p->uuid().c_str(), p->hmac().c_str());
    }

    const char *player_name(TournamentPlayer *p) {
        return p->name().c_str();
    }

    Tournament *player_tournament(TournamentPlayer *p) {
        return p->mutable_tournament();
    }

    uint32_t player_rating(TournamentPlayer *p) {
        return p->rating();
    }

    const char *player_uuid(TournamentPlayer *p) {
        return p->uuid().c_str();
    }

    const char *player_hmac(TournamentPlayer *p) {
        return p->hmac().c_str();
    }

    void player_set_name(TournamentPlayer *p, const char *name) {
        p->set_name(name);
    }

    void player_set_tournament(TournamentPlayer *p, Tournament *t) {
        *(p->mutable_tournament()) = *t;
    }

    void player_set_rating(TournamentPlayer *p, uint32_t rating) {
        p->set_rating(rating);
    }

    void player_set_uuid(TournamentPlayer *p, const char *uuid, const char *digest) {
        p->set_uuid(uuid);
        if(!digest) {
            char *h = hmac(uuid);
            p->set_hmac(h);
            free(h);
        }
        else {
            p->set_hmac(digest);
        }
    }

    /* Games: */
    bool game_complete(TournamentGame *g) {
        return g->round() > 0
            && g->has_white()
            && player_identified(g->mutable_white())
            && g->has_black()
            && player_identified(g->mutable_black())
            && g->result() > 0;
    }

    bool game_identified(TournamentGame *g) {
        return g->uuid().size() > 0;
    }

    bool game_authenticated(TournamentGame *g) {
        return game_identified(g)
            && g->hmac().size() > 0
            && hmac_validate(g->uuid().c_str(), g->hmac().c_str());
    }

    uint32_t game_round(TournamentGame *g) {
        return g->round();
    }

    TournamentPlayer *game_white(TournamentGame *g) {
        return g->mutable_white();
    }

    TournamentPlayer *game_black(TournamentGame *g) {
        return g->mutable_black();
    }

    Result game_result(TournamentGame *g) {
        return g->result();
    }

    const char *game_uuid(TournamentGame *g) {
        return g->uuid().c_str();
    }

    const char *game_hmac(TournamentGame *g) {
        return g->hmac().c_str();
    }
}
