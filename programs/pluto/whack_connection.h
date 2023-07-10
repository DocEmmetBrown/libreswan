/* whack receive routines, for libreswan
 *
 * Copyright (C) 2023  Andrew Cagney
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

#ifndef WHACK_CONNECTION_H
#define WHACK_CONNECTION_H

#include <stdbool.h>

struct connection;
struct whack_message;
struct show;

struct each {
	const char *future_tense;
	const char *past_tense;
	bool log_unknown_name;
	bool skip_instances;
};

void whack_all_connections(const struct whack_message *m, struct show *s,
			   bool (*whack_connection)
			   (struct show *s,
			    struct connection **c,
			    const struct whack_message *m));

void whack_each_connection(const struct whack_message *m, struct show *s,
			   bool (*whack_connection)
			   (struct show *s,
			    struct connection **c,
			    const struct whack_message *m),
			   struct each each);

#endif
