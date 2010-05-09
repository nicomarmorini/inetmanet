/*****************************************************************************
 *
 * Copyright (C) 2001 Uppsala University & Ericsson AB.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Erik Nordstr�m, <erik.nordstrom@it.uu.se>
 *
 *****************************************************************************/
#ifndef _DEBUG_H
#define _DEBUG_H

#ifndef  _WIN32
#include <syslog.h>
#else
#define LOG_DEBUG 0
#define LOG_NOTICE 0
#define LOG_ERR 0
#define LOG_WARNING 0
#endif


//#include <syslog.h>

#ifndef NS_PORT
extern int debug;
#endif

#ifndef NS_NO_DECLARATIONS
void log_init();
void log_cleanup();

const char *packet_type(unsigned int type);
void alog(int type, int errnum, const char *function, const char *format, ...);
void log_pkt_fields(AODV_msg * msg);
void print_rt_table(void *arg);
void log_rt_table_init();
char *ip_to_str(struct in_addr addr);

#ifdef NS_PORT
void write_to_log_file(char *msg, int len);
char *devs_ip_to_str();
char *rreq_flags_to_str(RREQ * rreq);
char *rrep_flags_to_str(RREP * rrep);
char *rt_flags_to_str(u_int16_t flags);
const char *state_to_str(u_int8_t state);
#endif
#endif				/* NS_NO_DECLARATIONS */

#ifndef NS_NO_GLOBALS

/*
#ifdef _WIN32
#ifdef DEBUG
#undef DEBUG
#endif
#endif
*/

#ifdef DEBUG
#undef DEBUG
#endif


#ifdef DEBUG
#undef DEBUG
#define DEBUG_OUTPUT
#define DEBUG(l, s, args...) alog(l, s, __FUNCTION__, ## args)
//#define DEBUG(l, i,s, args...) syslog(l, strcpy  s, ## args)
#else
/*
#ifdef _WIN32
#define DEBUG(l, s, args)
#else
#define DEBUG(l, s, args...)
#endif
*/
#define DEBUG(l, s, args...)
#endif /* DEBUG*/
#endif				/* NS_NO_GLOBALS */

#endif				/* _DEBUG_H */

