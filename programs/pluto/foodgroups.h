/* Implement policygroups-style control files (aka "foodgroups")
 * Copyright (C) 2002  D. Hugh Redelmeier.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <https://www.gnu.org/licenses/gpl2.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#ifndef FOODGROUPS_H
#define FOODGROUPS_H

struct connection;      /* forward declaration */
struct logger;

void connection_group_route(struct connection *c, where_t where);
void connection_group_unroute(struct connection *c, where_t where);

void load_groups(struct logger *logger);

void remove_from_group(struct connection *c);

#endif
