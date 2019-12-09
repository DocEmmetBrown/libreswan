/* IKEv2 authentication, for libreswan
 *
 * Copyright (C) 2019  Andrew Cagney
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#ifndef IKEV2_SIGHASH_H
#define IKEV2_SIGHASH_H

#include <stdbool.h>

#include "chunk.h"
#include "packet.h"		/* for pbs_out */

struct state;
struct connection;
struct ike_sa;

struct crypt_mac v2_calculate_sighash(const struct state *st,
				      enum original_role role,
				      const struct crypt_mac *idhash,
				      const chunk_t firstpacket,
				      const struct hash_desc *hasher);

enum keyword_authby v2_auth_by(struct ike_sa *ike);
enum ikev2_auth_method v2_auth_method(struct ike_sa *ike, enum keyword_authby authby);
const struct hash_desc *v2_auth_negotiated_signature_hash(struct ike_sa *ike);

shunk_t authby_asn1_hash_blob(const struct hash_desc *hash_algo,
			      enum keyword_authby authby);
bool emit_v2_asn1_hash_blob(const struct hash_desc *hash_algo,
			    pb_stream *a_pbs, enum keyword_authby authby);

struct hash_signature v2_auth_signature(struct ike_sa *ike,
					const struct crypt_mac *sighash,
					const struct hash_desc *hash_algo,
					enum keyword_authby authby);

bool emit_v2_auth(struct ike_sa *ike,
		  const struct hash_signature *auth_sig,
		  const struct crypt_mac *idhash,
		  pb_stream *outpbs);

#endif
