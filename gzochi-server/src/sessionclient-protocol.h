/* sessionclient-protocol.h: Prototypes and decls for sessionclient-protocol.c
 * Copyright (C) 2017 Julian Graham
 *
 * gzochi is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef GZOCHID_SESSIONCLIENT_PROTOCOL_H
#define GZOCHID_SESSIONCLIENT_PROTOCOL_H

#include "protocol.h"

/* A `gzochid_client_protocol' implementation for the sessionclient protocol. */

gzochid_client_protocol gzochid_sessionclient_client_protocol;

#endif /* GZOCHID_SESSIONCLIENT_PROTOCOL_H */
