#ifndef _DATABASE_H
#define _DATABASE_H

#include <stdbool.h>
#include <stdint.h>

#include "requests.h"

bool database_connect(char *dbname, char *username, char *password);
bool database_insert_tournament(Tournament *t);
bool database_get_tournament(Tournament *t);
bool database_insert_player(TournamentPlayer *p);

#endif
