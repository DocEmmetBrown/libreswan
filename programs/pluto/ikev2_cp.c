/* IKEv2 Configuration Payload, for Libreswan
 *
 * Copyright (C) 2007-2008 Michael Richardson <mcr@xelerance.com>
 * Copyright (C) 2008-2011 Paul Wouters <paul@xelerance.com>
 * Copyright (C) 2008 Antony Antony <antony@xelerance.com>
 * Copyright (C) 2008-2009 David McCullough <david_mccullough@securecomputing.com>
 * Copyright (C) 2010,2012 Avesh Agarwal <avagarwa@redhat.com>
 * Copyright (C) 2010-2019 Tuomo Soini <tis@foobar.fi
 * Copyright (C) 2012-2019 Paul Wouters <pwouters@redhat.com>
 * Copyright (C) 2012-2018 Antony Antony <antony@phenome.org>
 * Copyright (C) 2013-2019 D. Hugh Redelmeier <hugh@mimosa.com>
 * Copyright (C) 2013 David McCullough <ucdevel@gmail.com>
 * Copyright (C) 2013 Matt Rogers <mrogers@redhat.com>
 * Copyright (C) 2015-2019 Andrew Cagney <cagney@gnu.org>
 * Copyright (C) 2017-2018 Sahana Prasad <sahana.prasad07@gmail.com>
 * Copyright (C) 2017-2018 Vukasin Karadzic <vukasin.karadzic@gmail.com>
 * Copyright (C) 2017 Mayank Totale <mtotale@gmail.com>
 * Copyright (C) 2020 Yulia Kuzovkova <ukuzovkova@gmail.com>
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

#include "ip_info.h"

#include "defs.h"
#include "demux.h"
#include "connections.h"
#include "state.h"
#include "log.h"
#include "addresspool.h"
#include "ikev2_cp.h"

static bool need_v2_configuration_payload(const struct connection *const cc,
					  const lset_t st_nat_traversal)
{
	return (cc->local->host.config->modecfg.client &&
		(!cc->local->host.config->client_address_translation ||
		 LHAS(st_nat_traversal, NATED_HOST)));
}

bool expect_v2CP_response(const struct connection *const cc,
		       const lset_t st_nat_traversal)
{
	return need_v2_configuration_payload(cc, st_nat_traversal);
}

bool need_v2CP_request(const struct connection *const cc,
		       const lset_t st_nat_traversal)
{
	return (need_v2_configuration_payload(cc, st_nat_traversal) ||
		cc->config->modecfg.domains != NULL ||
		cc->config->modecfg.dns != NULL);
}

/* Misleading name, also used for NULL sized type's */
static stf_status ikev2_ship_cp_attr_ip(uint16_t type, ip_address *ip,
					const char *story, struct pbs_out *outpbs)
{
	struct pbs_out a_pbs;

	struct ikev2_cp_attribute attr = {
		.type = type,
	};

	/* could be NULL */
	const struct ip_info *afi = address_type(ip);

	if (afi == NULL) {
		attr.len = 0;
	} else if (afi == &ipv6_info) {
		attr.len = IKEv2_INTERNAL_IP6_ADDRESS_SIZE; /* RFC hack to append IPv6 prefix len */
	} else {
		attr.len = address_type(ip)->ip_size;
	}

	if (!out_struct(&attr, &ikev2_cp_attribute_desc, outpbs,
				&a_pbs))
		return STF_INTERNAL_ERROR;

	if (attr.len > 0) {
		if (!pbs_out_address(&a_pbs, *ip, story)) {
			/* already logged */
			return STF_INTERNAL_ERROR;
		}
	}

	if (attr.len == IKEv2_INTERNAL_IP6_ADDRESS_SIZE) { /* IPv6 address add prefix */
		uint8_t ipv6_prefix_len = IKEv2_INTERNAL_IP6_PREFIX_LEN;
		if (!pbs_out_raw(&a_pbs, &ipv6_prefix_len, sizeof(uint8_t), "INTERNAL_IP6_PREFIX_LEN")) {
			/* already logged */
			return STF_INTERNAL_ERROR;
		}
	}

	close_output_pbs(&a_pbs);
	return STF_OK;
}

static bool emit_v2CP_attribute(struct pbs_out *outpbs,
				uint16_t type, shunk_t attrib,
				const char *story)
{
	struct ikev2_cp_attribute attr = {
		.type = type,
		.len = attrib.len,
	};

	pb_stream a_pbs;
	if (!pbs_out_struct(outpbs, &ikev2_cp_attribute_desc,
			    &attr, sizeof(attr), &a_pbs)) {
		/* already logged */
		return false; /*fatal*/
	}

	if (attrib.len > 0) {
		if (!pbs_out_hunk(&a_pbs, attrib, story)) {
			/* already logged */
			return false;
		}
	}

	close_output_pbs(&a_pbs);
	return true;
}

/*
 * CHILD is negotiating configuration; hence log against child.
 */

bool emit_v2CP_response(const struct child_sa *child, struct pbs_out *outpbs)
{
	struct connection *c = child->sa.st_connection;
	pb_stream cp_pbs;
	struct ikev2_cp cp = {
		.isacp_critical = ISAKMP_PAYLOAD_NONCRITICAL,
		.isacp_type = IKEv2_CP_CFG_REPLY,
	};

	dbg("send %s Configuration Payload", enum_name(&ikev2_cp_type_names, cp.isacp_type));

	if (!out_struct(&cp, &ikev2_cp_desc, outpbs, &cp_pbs))
		return false;

	ip_address that_client_address = selector_prefix(c->spd->remote->client);
	ikev2_ship_cp_attr_ip(selector_type(&c->spd->remote->client) == &ipv4_info ?
			      IKEv2_INTERNAL_IP4_ADDRESS : IKEv2_INTERNAL_IP6_ADDRESS,
			      &that_client_address, "Internal IP Address", &cp_pbs);

	for (ip_address *dns = c->config->modecfg.dns;
	     dns != NULL && dns->is_set; dns++) {
		const struct ip_info *afi = address_type(dns);
		switch (afi->ip_version) {
		case IPv4:
			if (ikev2_ship_cp_attr_ip(IKEv2_INTERNAL_IP4_DNS, dns,
						  "IP4_DNS", &cp_pbs) != STF_OK) {
				return false;
			}
			break;
		case IPv6:
			if (ikev2_ship_cp_attr_ip(IKEv2_INTERNAL_IP6_DNS, dns,
						  "IP6_DNS", &cp_pbs) != STF_OK) {
				return false;
			}
			break;
		}
	}

	for (const shunk_t *domain = c->config->modecfg.domains;
	     domain != NULL && domain->ptr != NULL; domain++) {
		if (!emit_v2CP_attribute(&cp_pbs,
					 IKEv2_INTERNAL_DNS_DOMAIN,
					 *domain,
					 "IKEv2_INTERNAL_DNS_DOMAIN")) {
			/* already logged */
			return false;
		}
	}

	close_output_pbs(&cp_pbs);
	return true;
}

bool emit_v2CP_request(const struct child_sa *child, struct pbs_out *outpbs)
{
	struct pbs_out cp_pbs;
	struct ikev2_cp cp = {
		.isacp_critical = ISAKMP_PAYLOAD_NONCRITICAL,
		.isacp_type = IKEv2_CP_CFG_REQUEST,
	};

	dbg("emit %s Configuration Payload", enum_name(&ikev2_cp_type_names, cp.isacp_type));

	if (!out_struct(&cp, &ikev2_cp_desc, outpbs, &cp_pbs))
		return false;

	struct connection *cc = child->sa.st_connection;
	lset_t ipset = LEMPTY;

	FOR_EACH_THING(afi, &ipv4_info, &ipv6_info) {
		if (cc->pool[afi->ip_index] != NULL) {
			dbg("pool says to ask for %s", afi->ip_name);
			ipset |= LELEM(afi->ip_index);
		}
	}

	for (struct spd_route *spd = cc->spd; spd != NULL; spd = spd->spd_next) {
		const struct ip_info *afi = selector_info(spd->local->client);
		if (afi != NULL) {
			dbg("SPD says to ask for %s", afi->ip_name);
			ipset |= LELEM(afi->ip_index);
		}
	}

	if (ipset == LEMPTY) {
		llog_pexpect(child->sa.st_logger, HERE,
			     "can't figure out which internal address is needed");
		return false;
	}

	if (ipset & LELEM(ipv4_info.ip_index)) {
		ikev2_ship_cp_attr_ip(IKEv2_INTERNAL_IP4_ADDRESS, NULL, "IPv4 address", &cp_pbs);
		ikev2_ship_cp_attr_ip(IKEv2_INTERNAL_IP4_DNS, NULL, "IPv4 DNS", &cp_pbs);
	}
	if (ipset & LELEM(ipv6_info.ip_index)) {
		ikev2_ship_cp_attr_ip(IKEv2_INTERNAL_IP6_ADDRESS, NULL, "IPv6 address", &cp_pbs);
		ikev2_ship_cp_attr_ip(IKEv2_INTERNAL_IP6_DNS, NULL, "IPv6 DNS", &cp_pbs);
	}
	ikev2_ship_cp_attr_ip(IKEv2_INTERNAL_DNS_DOMAIN, NULL, "Domain", &cp_pbs);

	close_output_pbs(&cp_pbs);
	return true;
}

bool process_v2_IKE_AUTH_request_v2CP_request_payload(struct ike_sa *ike, struct child_sa *child,
						      struct payload_digest *cp_digest)
{
	pexpect(ike->sa.st_connection == child->sa.st_connection);
	struct connection *cc = child->sa.st_connection;

	struct ikev2_cp *cp =  &cp_digest->payload.v2cp;
	struct pbs_in *cp_pbs = &cp_digest->pbs;

	ldbg(child->sa.st_logger, "parsing ISAKMP_NEXT_v2CP payload");

	if (cp->isacp_type != IKEv2_CP_CFG_REQUEST) {
		llog_sa(RC_LOG_SERIOUS, child,
			"ERROR: expected IKEv2_CP_CFG_REQUEST got a %s",
			enum_name(&ikev2_cp_type_names, cp->isacp_type));
		return false;
	}

	lset_t seen = LEMPTY;
	static const struct ip_info *internal_to_info[] = {
		[IKEv2_INTERNAL_IP4_ADDRESS] = &ipv4_info,
		[IKEv2_INTERNAL_IP6_ADDRESS] = &ipv6_info,
	};

	while (pbs_left(cp_pbs) > 0) {

		struct ikev2_cp_attribute cp_attr;
		struct pbs_in cp_attr_pbs;
		diag_t d = pbs_in_struct(cp_pbs, &ikev2_cp_attribute_desc,
					 &cp_attr, sizeof(cp_attr), &cp_attr_pbs);
		if (d != NULL) {
			llog_diag(RC_LOG_SERIOUS, child->sa.st_logger, &d,
				 "ERROR: malformed CP attribute");
			return false;
		}

		enum ikev2_cp_attribute_type type = cp_attr.type; 
		switch (type) {
		case IKEv2_INTERNAL_IP4_ADDRESS:
		case IKEv2_INTERNAL_IP6_ADDRESS:
		{
			const struct ip_info *afi = internal_to_info[type];
			passert(afi != NULL);
			const struct addresspool *pool = cc->pool[afi->ip_index];
			if (pool == NULL) {
				enum_buf eb;
				ldbg(child->sa.st_logger, "ignoring %s, no pool",
				     str_enum_short(&ikev2_cp_attribute_type_names, type, &eb));
			} else {
				err_t e = lease_that_address(child->sa.st_connection, &child->sa, afi);
				if (e != NULL) {
					enum_buf eb;
					llog_sa(RC_LOG, child, "%s lease_an_address failure %s",
						str_enum_short(&ikev2_cp_attribute_type_names, type, &eb),
						e);
					return false;
				}
				seen |= type;
			}
			break;
		}

		default:
		{
			enum_buf eb;
			ldbg(child->sa.st_logger,
			     "ignoring attribute %s length %u",
			     str_enum_short(&ikev2_cp_attribute_type_names, type, &eb),
			     cp_attr.len);
			break;
		}
		}
	}

	if (seen == LEMPTY) {
		llog_sa(RC_LOG_SERIOUS, child,
			"ERROR: no valid internal address request");
		return false;
	}

	return true;
}

static void ikev2_set_domain(struct pbs_in *cp_a_pbs, struct child_sa *child)
{
	bool responder = (child->sa.st_sa_role == SA_RESPONDER);
	bool ignore = LIN(POLICY_IGNORE_PEER_DNS, child->sa.st_connection->policy);

	if (!responder) {
		char *safestr = cisco_stringify(cp_a_pbs, "INTERNAL_DNS_DOMAIN",
						ignore, child->sa.st_logger);
		if (safestr != NULL) {
			append_st_cfg_domain(&child->sa, safestr);
		}
	} else {
		llog_sa(RC_LOG, child,
			  "initiator INTERNAL_DNS_DOMAIN CP ignored");
	}
}

static bool ikev2_set_dns(struct pbs_in *cp_a_pbs, struct child_sa *child,
			  const struct ip_info *af)
{
	struct connection *c = child->sa.st_connection;
	bool ignore = LIN(POLICY_IGNORE_PEER_DNS, c->policy);

	if (c->policy & POLICY_OPPORTUNISTIC) {
		llog_sa(RC_LOG, child,
			  "ignored INTERNAL_IP%d_DNS CP payload for Opportunistic IPsec",
			  af->ip_version);
		return true;
	}

	ip_address ip;
	diag_t d = pbs_in_address(cp_a_pbs, &ip, af, "INTERNAL_IP_DNS CP payload");
	if (d != NULL) {
		llog_diag(RC_LOG, child->sa.st_logger, &d, "%s", "");
		return false;
	}

	/* i.e. all zeros */
	if (!address_is_specified(ip)) {
		address_buf ip_str;
		llog_sa(RC_LOG, child,
			  "ERROR INTERNAL_IP%d_DNS %s is invalid",
			  af->ip_version, str_address(&ip, &ip_str));
		return false;
	}

	bool responder = (child->sa.st_sa_role == SA_RESPONDER);
	if (!responder) {
		address_buf ip_buf;
		const char *ip_str = ipstr(&ip, &ip_buf);

		llog_sa(RC_LOG, child,
			  "received %sINTERNAL_IP%d_DNS %s",
			  ignore ? "and ignored " : "",
			  af->ip_version, ip_str);
		if (!ignore)
			append_st_cfg_dns(&child->sa, ip_str);
	} else {
		llog_sa(RC_LOG, child,
			  "initiator INTERNAL_IP%d_DNS CP ignored",
			  af->ip_version);
	}

	return true;
}

static bool ikev2_set_internal_address(struct pbs_in *cp_a_pbs, struct child_sa *child,
				       const struct ip_info *af, bool *seen_an_address)
{
	struct connection *c = child->sa.st_connection;

	ip_address ip;
	diag_t d = pbs_in_address(cp_a_pbs, &ip, af, "INTERNAL_IP_ADDRESS");
	if (d != NULL) {
		llog_diag(RC_LOG, child->sa.st_logger, &d, "%s", "");
		return false;
	}

	/*
	 * if (af->af == AF_INET6) pbs_in_address only reads 16 bytes.
	 * There should be one more byte in the pbs, 17th byte is prefix length.
	 */

	if (!address_is_specified(ip)) {
		address_buf ip_str;
		llog_sa(RC_LOG, child,
			  "ERROR INTERNAL_IP%d_ADDRESS %s is invalid",
			  af->ip_version, str_address(&ip, &ip_str));
		return false;
	}

	ipstr_buf ip_str;
	llog_sa(RC_LOG, child,
		  "received INTERNAL_IP%d_ADDRESS %s%s",
		  af->ip_version, ipstr(&ip, &ip_str),
		  *seen_an_address ? "; discarded" : "");

	bool responder = (child->sa.st_sa_role == SA_RESPONDER);
	if (responder) {
		llog_sa(RC_LOG, child, "bogus responder CP ignored");
		return true;
	}

	if (*seen_an_address) {
		return true;
	}

	*seen_an_address = true;
	c->spd->local->has_client = true;
	c->local->child.has_internal_address = true;

	if (c->local->host.config->client_address_translation) {
		dbg("CAT is set, not setting host source IP address to %s",
		    ipstr(&ip, &ip_str));
		ip_address this_client_prefix = selector_prefix(c->spd->local->client);
		if (address_eq_address(this_client_prefix, ip)) {
			/*
			 * The address we received is same as this
			 * side should we also check the host_srcip.
			 */
			dbg("#%lu %s[%lu] received INTERNAL_IP%d_ADDRESS that is same as this->client.addr %s. Will not add CAT iptable rules",
			    child->sa.st_serialno, c->name, c->instance_serial,
			    af->ip_version, ipstr(&ip, &ip_str));
		} else {
			set_end_selector(c, c->local->config->index,
					 selector_from_address(ip),
					 "CP scribbling on end while ignoring TS");
			c->local->child.has_cat = true; /* create iptable entry */
		}
	} else {
		set_end_selector(c, c->local->config->index,
				 selector_from_address(ip),
				 "CP scribbling on end while ignoring TS");
	}

	return true;
}

bool process_v2CP_response_payload(struct ike_sa *ike UNUSED, struct child_sa *child,
				   struct payload_digest *cp_pd)
{
	struct ikev2_cp *cp =  &cp_pd->payload.v2cp;
	struct connection *c = child->sa.st_connection;
	pb_stream *attrs = &cp_pd->pbs;

	dbg("#%lu %s[%lu] parsing ISAKMP_NEXT_v2CP payload",
	    child->sa.st_serialno, c->name, c->instance_serial);

	switch (child->sa.st_sa_role) {
	case SA_INITIATOR:
		if (cp->isacp_type != IKEv2_CP_CFG_REPLY) {
			llog_sa(RC_LOG_SERIOUS, child,
				  "ERROR expected IKEv2_CP_CFG_REPLY got a %s",
				  enum_name(&ikev2_cp_type_names, cp->isacp_type));
			return false;
		}
		break;
	case SA_RESPONDER:
		if (cp->isacp_type != IKEv2_CP_CFG_REQUEST) {
			llog_sa(RC_LOG_SERIOUS, child,
				  "ERROR expected IKEv2_CP_CFG_REQUEST got a %s",
				  enum_name(&ikev2_cp_type_names, cp->isacp_type));
			return false;
		}
		break;
	default:
		bad_case(child->sa.st_sa_role);
	}

	bool seen_internal_address = false;
	while (pbs_left(attrs) > 0) {
		struct ikev2_cp_attribute cp_a;
		pb_stream cp_a_pbs;

		diag_t d = pbs_in_struct(attrs, &ikev2_cp_attribute_desc,
					 &cp_a, sizeof(cp_a), &cp_a_pbs);
		if (d != NULL) {
			llog_diag(RC_LOG_SERIOUS, child->sa.st_logger, &d,
				 "ERROR malformed CP attribute");
			return false;
		}

		switch (cp_a.type) {
		case IKEv2_INTERNAL_IP4_ADDRESS:
			if (!ikev2_set_internal_address(&cp_a_pbs, child, &ipv4_info,
							&seen_internal_address)) {
				llog_sa(RC_LOG_SERIOUS, child,
					  "ERROR malformed INTERNAL_IP4_ADDRESS attribute");
				return false;
			}
			break;

		case IKEv2_INTERNAL_IP4_DNS:
			if (!ikev2_set_dns(&cp_a_pbs, child, &ipv4_info)) {
				llog_sa(RC_LOG_SERIOUS, child,
					  "ERROR malformed INTERNAL_IP4_DNS attribute");
				return false;
			}
			break;

		case IKEv2_INTERNAL_IP6_ADDRESS:
			if (!ikev2_set_internal_address(&cp_a_pbs, child, &ipv6_info,
							&seen_internal_address)) {
				llog_sa(RC_LOG_SERIOUS, child,
					  "ERROR malformed INTERNAL_IP6_ADDRESS attribute");
				return false;
			}
			break;

		case IKEv2_INTERNAL_IP6_DNS:
			if (!ikev2_set_dns(&cp_a_pbs, child, &ipv6_info)) {
				llog_sa(RC_LOG_SERIOUS, child,
					  "ERROR malformed INTERNAL_IP6_DNS attribute");
				return false;
			}
			break;

		case IKEv2_INTERNAL_DNS_DOMAIN:
			ikev2_set_domain(&cp_a_pbs, child); /* can't fail */
			break;

		default:
			llog_sa(RC_LOG, child,
				  "unknown attribute %s length %u",
				  enum_name(&ikev2_cp_attribute_type_names, cp_a.type),
				  cp_a.len);
			break;
		}
	}
	return true;
}
