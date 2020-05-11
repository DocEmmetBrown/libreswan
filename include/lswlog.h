/* logging declaratons
 *
 * Copyright (C) 1998-2001,2013 D. Hugh Redelmeier <hugh@mimosa.com>
 * Copyright (C) 2004 Michael Richardson <mcr@xelerance.com>
 * Copyright (C) 2012-2013 Paul Wouters <paul@libreswan.org>
 * Copyright (C) 2017-2019 Andrew Cagney <cagney@gnu.org>
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
 *
 */

#ifndef _LSWLOG_H_
#define _LSWLOG_H_

#include <stdarg.h>
#include <stdio.h>		/* for FILE */
#include <stddef.h>		/* for size_t */

#include "lset.h"
#include "lswcdefs.h"
#include "jambuf.h"
#include "libreswan/passert.h"
#include "constants.h"		/* for DBG_... */
#include "where.h"		/* used by macros */
#include "fd.h"			/* for null_fd */
#include "impair.h"

/* Build up a diagnostic in a static buffer -- NOT RE-ENTRANT.
 * Although this would be a generally useful function, it is very
 * hard to come up with a discipline that prevents different uses
 * from interfering.  It is intended that by limiting it to building
 * diagnostics, we will avoid this problem.
 * Juggling is performed to allow an argument to be a previous
 * result: the new string may safely depend on the old one.  This
 * restriction is not checked in any way: violators will produce
 * confusing results (without crashing!).
 */
#define LOG_WIDTH	((size_t)1024)	/* roof of number of chars in log line */

extern err_t builddiag(const char *fmt, ...) PRINTF_LIKE(1);	/* NOT RE-ENTRANT */

extern bool log_to_stderr;          /* should log go to stderr? */


/*
 * Codes for status messages returned to whack.
 *
 * These are 3 digit decimal numerals.  The structure is inspired by
 * section 4.2 of RFC959 (FTP).  Since these will end up as the exit
 * status of whack, they must be less than 256.
 *
 * NOTE: ipsec_auto(8) knows about some of these numbers -- change
 * carefully.
 */

enum rc_type {
	RC_COMMENT,		/* non-commital utterance (does not affect exit status) */
	RC_PRINT,		/* ditto, but also suppresses the '[0-9]* ' prefix */
	RC_LOG,			/* message aimed at log (does not affect exit status) */
	RC_LOG_SERIOUS,		/* serious message aimed at log (does not affect exit status) */
	RC_SUCCESS,		/* success (exit status 0) */
	RC_INFORMATIONAL,	/* should get relayed to user - if there is one */
	RC_INFORMATIONAL_TRAFFIC, /* status of an established IPSEC (aka Phase 2) state */

	/* failure, but not definitive */
	RC_RETRANSMISSION = 10,

	/* improper request */
	RC_EXIT_FLOOR = 20,
	RC_DUPNAME = RC_EXIT_FLOOR,	/* attempt to reuse a connection name */
	RC_UNKNOWN_NAME,	/* connection name unknown or state number */
	RC_ORIENT,		/* cannot orient connection: neither end is us */
	RC_CLASH,		/* clash between two Road Warrior connections OVERLOADED */
	RC_DEAF,		/* need --listen before --initiate */
	RC_ROUTE,		/* cannot route */
	RC_RTBUSY,		/* cannot unroute: route busy */
	RC_BADID,		/* malformed --id */
	RC_NOKEY,		/* no key found through DNS */
	RC_NOPEERIP,		/* cannot initiate when peer IP is unknown */
	RC_INITSHUNT,		/* cannot initiate a shunt-oly connection */
	RC_WILDCARD,		/* cannot initiate when ID has wildcards */
	RC_CRLERROR,		/* CRL fetching disabled or obsolete reread cmd */
	RC_WHACK_PROBLEM,	/* whack-detected problem */

	/* permanent failure */
	RC_BADWHACKMESSAGE = 30,
	RC_NORETRANSMISSION,
	RC_INTERNALERR,
	RC_OPPOFAILURE,		/* Opportunism failed */
	RC_CRYPTOFAILED,	/* system too busy to perform required
				* cryptographic operations */
	RC_AGGRALGO,		/* multiple algorithms requested in phase 1 aggressive */
	RC_FATAL,		/* fatal error encountered, and negotiation aborted */

	/* entry of secrets */
	RC_ENTERSECRET = 40,
	RC_USERPROMPT = 41,

	RC_EXIT_ROOF = 100,

	/*
	 * progress: start of range for successful state transition.
	 * Actual value is RC_NEW_V[12]_STATE plus the new state code.
	 */
	RC_NEW_V1_STATE = RC_EXIT_ROOF,
	RC_NEW_V2_STATE = 150,

	/*
	 * Start of range for notification.
	 *
	 * Actual value is RC_NOTIFICATION plus code for notification
	 * that should be generated by this Pluto.  RC_NOTIFICATION.
	 * Since notifications are two octets, that's 64435+200 which
	 * overflows the 3-digit prefix, oops.
	 */
	RC_NOTIFICATION = 200,	/* as per IKE notification messages */
};


/*
 * A generic buffer for accumulating unbounded output.
 *
 * The buffer's contents can be directed to various logging streams.
 */

struct lswlog;

/*
 * The logging streams used by libreswan.
 *
 * So far three^D^D^D^D^D four^D^D^D^D five^D^D^D^D six^D^D^D
 * seven^D^D^D^D^D five.five streams have been identified; and let's
 * not forget that code writes to STDOUT and STDERR directly.
 *
 * The streams differ in the syslog severity and what PREFIX is
 * assumed to be present and the tool being run.
 *
 *                           PLUTO
 *              SEVERITY  WHACK  PREFIX    TOOLS    PREFIX
 *   default    WARNING    yes    state     -v
 *   log        WARNING     -     state     -v
 *   debug      DEBUG       -     "| "     debug?
 *   error      ERR         -    ERROR     STDERR  PROG:_...
 *   whack         -       yes    NNN      STDOUT  ...
 *   file          -        -      -         -
 *
 * The streams will then add additional prefixes as required.  For
 * instance, the log_whack stream will prefix a timestamp when sending
 * to a file (optional), and will prefix NNN(RC) when sending to
 * whack.
 *
 * For tools, the default and log streams go to STDERR when enabled;
 * and the debug stream goes to STDERR conditional on debug flags.
 * Should the whack stream go to stdout?
 *
 * As needed, return size_t - the number of bytes written - so that
 * implementations have somewhere to send values that should not be
 * ignored; for instance fwrite() :-/
 */

/*
 * By default messages are broadcast (to both log files and whack),
 * mix-in one of these options to limit this.
 */

enum stream {
	/*
	 * This means that a simple RC_* code will go to both whack
	 * and and the log files.
	 */
	/* Mask the whack RC; max value is 64435+200 */
	RC_MASK		= 0x0fffff,
	/*                                 Severity     Whack Prefix */
	ALL_STREAMS     = 0x000000,	/* LOG_WARNING   yes         */
	LOG_STREAM	= 0x100000,	/* LOG_WARNING   no          */
	DEBUG_STREAM	= 0x200000,	/* LOG_DEBUG     no    "| "  */
	WHACK_STREAM	= 0x300000,	/*    N/A        yes         */
	ERROR_STREAM	= 0x400000,	/* LOG_ERR       no          */
	NO_STREAM	= 0xf00000,	/* n/a */
};

void log_jambuf(lset_t rc_flags, struct fd *object_fd, jambuf_t *buf);

size_t lswlog_to_file_stream(struct lswlog *buf, FILE *file);

/*
 * XXX: since the following all use the global CUR_STATE to get
 * OBJECT_FD, they can't be implemented using log_jambuf().
 */

void lswlog_to_default_streams(struct lswlog *buf, enum rc_type rc);
void lswlog_to_error_stream(struct lswlog *buf);

/*
 * Log to the default stream(s):
 *
 * - for pluto this means 'syslog', and when connected, whack.
 *
 * - for standalone tools, this means stderr, but only when enabled.
 *
 * There are two variants, the first specify the RC (prefix sent to
 * whack), while the second default RC to RC_LOG.
 *
 * XXX: even though the the name loglog() gives the impression that
 * it, and not libreswan_log(), needs to be used when double logging
 * (i.e., to both 'syslog' and whack), it does not - both functions
 * double log!  Hence LSWLOG_RC().
 */

void jam_cur_prefix(struct lswlog *buf);

extern void libreswan_log_rc(enum rc_type, const char *fmt, ...) PRINTF_LIKE(2);
#define loglog	libreswan_log_rc

#define LSWLOG_RC(RC, BUF)						\
	LSWLOG_(true, BUF,						\
		jam_cur_prefix(BUF),					\
		lswlog_to_default_streams(BUF, RC))

/* signature needs to match printf() */
extern int libreswan_log(const char *fmt, ...) PRINTF_LIKE(1);

#define LSWLOG(BUF) LSWLOG_RC(RC_LOG, BUF)

/*
 * Log, at level RC, to the whack log (if attached).
 *
 * XXX: See programs/pluto/log.h for interface; should only be used in
 * pluto.  This code assumes that it is being called from the main
 * thread.
 *
 * LSWLOG_INFO() sends stuff just to "whack" (or for a tool STDERR?).
 * XXX: there is no prefix, bug?  Should it send stuff out with level
 * RC_COMMENT?
 */

/* XXX: should be stdout?!? */
#define LSWLOG_INFO(BUF)						\
	LSWLOG_(true, BUF, , log_jambuf(WHACK_STREAM|RC_PRINT, null_fd, buf))

/*
 * Wrap <message> in a prefix and suffix where the suffix contains
 * errno and message.
 *
 * Notes:
 *
 * Because __VA_ARGS__ may contain function calls that modify ERRNO,
 * errno's value is first saved.
 *
 * While these common-case macros could be implemented directly using
 * LSWLOG_ERRNO_() et.al. they are not, instead they are implemented
 * as wrapper functions.  This is so that a crash will include the
 * below functions _including_ the with MESSAGE parameter - makes
 * debugging much easier.
 */

void libreswan_exit(enum pluto_exit_code rc) NEVER_RETURNS;
void libreswan_log_errno(int e, const char *message, ...) PRINTF_LIKE(2);
void libreswan_exit_log_errno(int e, const char *message, ...) PRINTF_LIKE(2) NEVER_RETURNS;

#define LOG_ERRNO(ERRNO, ...) {						\
		int log_errno = ERRNO; /* save value across va args */	\
		libreswan_log_errno(log_errno, __VA_ARGS__);		\
	}

#define EXIT_LOG_ERRNO(ERRNO, ...) {					\
		int exit_log_errno = ERRNO; /* save value across va args */ \
		libreswan_exit_log_errno(exit_log_errno, __VA_ARGS__);	\
	}

/*
 * E must have been saved!  Assume it is used as "... "PRI_ERRNO.
 *
 *   _Errno E: <strerror(E)>
 */
#define PRI_ERRNO "Errno %d: %s"
#define pri_errno(E) (E), strerror(E)


/*
 * Log debug messages to the main log stream, but not the WHACK log
 * stream.
 *
 * NOTE: DBG's action can be a { } block, but that block must not
 * contain commas that are outside quotes or parenthesis.
 * If it does, they will be interpreted by the C preprocesser
 * as macro argument separators.  This happens accidentally if
 * multiple variables are declared in one declaration.
 *
 * Naming: All DBG_*() prefixed functions send stuff to the debug
 * stream unconditionally.  Hence they should be wrapped in DBGP().
 */

extern lset_t cur_debugging;	/* current debugging level */

#define DBGP(cond)	(cur_debugging & (cond))

#define DEBUG_PREFIX "| "

#define DBG(cond, action)	{ if (DBGP(cond)) { action; } }
#define DBGF(COND, MESSAGE, ...) { if (DBGP(COND)) { DBG_log(MESSAGE,##__VA_ARGS__); } }
#define dbg(MESSAGE, ...) { if (DBGP(DBG_BASE)) { DBG_log(MESSAGE,##__VA_ARGS__); } }

/* DBG_*() are unconditional */
void DBG_log(const char *message, ...) PRINTF_LIKE(1);
void DBG_dump(const char *label, const void *p, size_t len);
#define DBG_dump_hunk(LABEL, HUNK)				\
	{							\
		typeof(HUNK) hunk_ = HUNK; /* evaluate once */	\
		DBG_dump(LABEL, hunk_.ptr, hunk_.len);		\
	}
#define DBG_dump_thing(LABEL, THING) DBG_dump(LABEL, &(THING), sizeof(THING))

#define LSWDBG_(PREDICATE, BUF)						\
	LSWLOG_(PREDICATE, BUF,						\
		/*no-prefix*/,						\
		log_jambuf(DEBUG_STREAM, null_fd, BUF))

#define LSWDBGP(DEBUG, BUF) LSWDBG_(DBGP(DEBUG), BUF)
#define LSWLOG_DEBUG(BUF) LSWDBG_(true, BUF)

/*
 * Code wrappers that cover up the details of allocating,
 * initializing, de-allocating (and possibly logging) a 'struct
 * lswlog' buffer.
 *
 * BUF (a C variable name) is declared locally as a pointer to a
 * per-thread 'struct lswlog' buffer.
 *
 * Implementation notes:
 *
 * This implementation stores the output in an array on the thread's
 * stack.  It could just as easily use the heap (but that would
 * involve memory overheads) or even a per-thread static variable.
 * Since the BUF variable is a pointer the specifics of the
 * implementation are hidden.
 *
 * This implementation, unlike DBG(), does not have a code block
 * parameter.  Instead it uses a sequence of for-loops to set things
 * up for a code block.  This avoids problems with "," within macro
 * parameters confusing the parser.  It also permits a simple
 * consistent indentation style.
 *
 * The stack array is left largely uninitialized (just a few strategic
 * entries are set).  This avoids the need to zero LOG_WITH bytes.
 *
 * Apparently chaining void function calls using a comma is valid C?
 */

/*
 * Scratch buffer for accumulating extra output.
 *
 * XXX: case should be expanded to illustrate how to stuff a truncated
 * version of the output into the LOG buffer.
 *
 * For instance:
 */

#if 0
void lswbuf(struct lswlog *log)
{
	LSWBUF(buf) {
		lswlogf(buf, "written to buf");
		lswlogl(log, buf); /* add to calling array */
	}
}
#endif

/* primitive to construct an LSWBUF on the stack.  */
#define LSWBUF(BUF)							\
	/* create the buffer */						\
	for (char lswbuf[LOG_WIDTH],					\
		     *lswbuf_ = lswbuf;					\
	     lswbuf_ != NULL; lswbuf_ = NULL)				\
		/* create the jambuf */					\
		for (jambuf_t jambuf = ARRAY_AS_JAMBUF(lswbuf),		\
			     *BUF = &jambuf;				\
		     BUF != NULL; BUF = NULL)
#define LSWBUF_ LSWBUF

/*
 * Template for constructing logging output intended for a logger
 * stream.
 *
 * The code is equivlaent to:
 *
 *   if (PREDICATE) {
 *     LSWBUF(BUF) {
 *       PREFIX;
 *          BLOCK;
 *       SUFFIX;
 *    }
 */

#define LSWLOG_(PREDICATE, BUF, PREFIX, SUFFIX)				\
	for (bool lswlog_p = PREDICATE; lswlog_p; lswlog_p = false)	\
		LSWBUF_(BUF)						\
			for (PREFIX; lswlog_p; lswlog_p = false, SUFFIX)

/*
 * Write a line of output to the FILE stream as a single block;
 * includes an implicit new-line.
 *
 * For instance:
 */

#if 0
void lswlog_file(FILE f)
{
	LSWLOG_FILE(f, buf) {
		lswlogf(buf, "written to file");
	}
}
#endif

#define LSWLOG_FILE(FILE, BUF)						\
	LSWLOG_(true, BUF,						\
		,							\
		lswlog_to_file_stream(BUF, FILE))

/*
 * Log an expectation failure message to the error streams.  That is
 * the main log (level LOG_ERR) and whack log (level RC_LOG_SERIOUS).
 *
 * When evaluating ASSERTION, do not wrap it in parentheses as it will
 * suppress the warning for 'foo = bar'.
 *
 * Because static analizer tools are easily confused, explicitly
 * return the assertion result.
 */

#ifdef EXAMPLE
void lswlog_pexpect_example(void *p)
{
	if (!pexpect(p != NULL)) {
		return;
	}
}
#endif

#define pexpect(ASSERTION)						\
	({								\
		bool assertion__ = ASSERTION;				\
		if (!assertion__) {					\
			log_pexpect(HERE, "%s", #ASSERTION);		\
		}							\
		assertion__; /* result */				\
	})

void log_pexpect(where_t where, const char *message, ...) PRINTF_LIKE(2);

void lswlog_pexpect_prefix(struct lswlog *buf);
void lswlog_pexpect_suffix(struct lswlog *buf, where_t where);

#define LSWLOG_PEXPECT_WHERE(WHERE, BUF)		   \
	LSWLOG_(true, BUF,				   \
		lswlog_pexpect_prefix(BUF),		   \
		lswlog_pexpect_suffix(BUF, WHERE))

#define LSWLOG_PEXPECT(BUF)				   \
	LSWLOG_PEXPECT_WHERE(HERE, BUF)

#define LOG_PEXPECT(FMT, ...)			\
	log_pexpect(HERE, FMT,##__VA_ARGS__)
#define PEXPECT_LOG(FMT, ...)			\
	log_pexpect(HERE, FMT,##__VA_ARGS__)


/*
 * Log an assertion failure to the main log, and the whack log; and
 * then call abort().
 */

void lswlog_passert_prefix(struct lswlog *buf);
void lswlog_passert_suffix(struct lswlog *buf, where_t where) NEVER_RETURNS;

#define LSWLOG_PASSERT(BUF)				   \
	LSWLOG_(true, BUF,				   \
		lswlog_passert_prefix(BUF),		   \
		lswlog_passert_suffix(BUF, HERE))

/* for a switch statement */

void libreswan_bad_case(const char *expression, long value, where_t where) NEVER_RETURNS;

#define bad_case(N)	libreswan_bad_case(#N, (N), HERE)

#define impaired_passert(BEHAVIOUR, ASSERTION) {			\
		if (impair.BEHAVIOUR) {					\
			bool assertion_ = ASSERTION;			\
			if (!assertion_) {				\
				libreswan_log("IMPAIR: assertion '%s' failed", #ASSERTION); \
			}						\
		} else {						\
			passert(ASSERTION);				\
		}							\
	}

/*
 * Wrap <message> in a prefix and suffix where the suffix contains
 * errno and message.  Since calls may alter ERRNO, it needs to be
 * saved.
 *
 * XXX: Is error stream really the right place for this?
 *
 * LSWLOG_ERROR() sends an arbitrary message to the error stream (in
 * tools that's STDERR).  XXX: Should LSWLOG_ERRNO() and LSWERR() be
 * merged.  XXX: should LSWLOG_ERROR() use a different prefix?
 */

void lswlog_errno_prefix(struct lswlog *buf, const char *prefix);
void lswlog_errno_suffix(struct lswlog *buf, int e);

#define LSWLOG_ERROR(BUF)			\
	LSWLOG_(true, BUF,			\
		jam_cur_prefix(BUF),		\
		lswlog_to_error_stream(buf))

#endif /* _LSWLOG_H_ */
