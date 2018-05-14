#ifndef _HANDLERS_H
#define _HANDLERS_H

#include "requests.h"

void handle_ping(Request *req, Response *resp);
void handle_tournament_create(Request *req, Response *resp);
void handle_tournament_get(Request *req, Response *resp);
void handle_player_signup(Request *req, Response *resp);

#endif
