/*
 * Copyright (c) 2011, Macmillan Publishers Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name Macmillan Publishers Ltd. nor the names
 *       of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written
 *       permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */

#include "noit_defines.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <ctype.h>
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif
#include <inttypes.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "noit_module.h"
#include "noit_check.h"
#include "noit_check_tools.h"
#include "utils/noit_log.h"
#include "utils/noit_hash.h"


static noit_log_stream_t nlerr = NULL;
static noit_log_stream_t nldeb = NULL;

typedef struct _mod_config {
  noit_hash_table *options;
  int ipv4_fd;
  int ipv6_fd;
} statsd_mod_config_t;

typedef struct statsd_closure_s {
	noit_module_t *self;
	stats_t current;
	int stats_count;
} statsd_closure_t;

#define NET_DEFAULT_PORT 8125

static int noit_statsd_handler(eventer_t e, int mask, void *closure,
															 struct timeval *now) {
  union {
    struct sockaddr_in  skaddr;
    struct sockaddr_in6 skaddr6;
  } remote;
}

static int noit_statsd_init(noit_module_t *self) {
	const char *config_val;
	int sockaddr_len;
	statsd_mod_config_t *conf;
	conf = noit_module_get_userdata(self);
	int portint = 0;
  struct sockaddr_in skaddr;
  struct sockaddr_in6 skaddr6;
  struct in6_addr in6addr_any = IN6ADDR_ANY_INIT;
  const char *host;
  unsigned short port;

	portint = NET_DEFAULT_PORT;
	if(noit_hash_retr_str(conf->options,
												"statsd_port", strlen("statsd_port"),
												(const char**)&config_val))
		portint = atoi(config_val);

	if(!noit_hash_retr_str(conf->options,
												"statsd_host", strlen("statsd_host"),
												(const char**)&config_val))
		host = "*";

	port = (unsigned short) portint;

	conf->ipv4_fd = ipv6_fd = -1;

  conf->ipv4_fd = socket(PF_INET, SOCK_DGRAM, 0);

  if(conf->ipv4_fd < 0) {
    noitL(noit_error, "statsd: socket failed: %s\n",
          strerror(errno));
  }
  else {
    if(eventer_set_fd_nonblocking(conf->ipv4_fd)) {
      close(conf->ipv4_fd);
      conf->ipv4_fd = -1;
      noitL(noit_error,
            "statsd: could not set socket non-blocking: %s\n",
            strerror(errno));
    }
  }
  skaddr.sin_family = AF_INET;
  skaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  skaddr.sin_port = htons(port);

  sockaddr_len = sizeof(skaddr);
  if(bind(conf->ipv4_fd, (struct sockaddr *)&skaddr, sockaddr_len) < 0) {
    noitL(noit_error, "statsd: bind failed[%s]: %s\n", host, strerror(errno));
    close(conf->ipv4_fd);
    return -1;
  }

  if(conf->ipv4_fd >= 0) {
    eventer_t newe;
    newe = eventer_alloc();
    newe->fd = conf->ipv4_fd;
    newe->mask = EVENTER_READ | EVENTER_EXCEPTION;
    newe->callback = noit_statsd_handler;
    newe->closure = self;
    eventer_add(newe);
  }

  conf->ipv6_fd = socket(AF_INET6, SOCK_DGRAM, 0);
  if(conf->ipv6_fd < 0) {
    noitL(noit_error, "statsd: IPv6 socket failed: %s\n",
          strerror(errno));
  }
  else {
    if(eventer_set_fd_nonblocking(conf->ipv6_fd)) {
      close(conf->ipv6_fd);
      conf->ipv6_fd = -1;
      noitL(noit_error,
            "statsd: could not set socket non-blocking: %s\n",
               strerror(errno));
    }
  }
  sockaddr_len = sizeof(skaddr6);
  memset(&skaddr6, 0, sizeof(skaddr6));
  skaddr6.sin6_family = AF_INET6;
  skaddr6.sin6_addr = in6addr_any;
  skaddr6.sin6_port = htons(port);

  if(bind(conf->ipv6_fd, (struct sockaddr *)&skaddr6, sockaddr_len) < 0) {
    noitL(noit_error, "bind(IPv6) failed[%s]: %s\n", host, strerror(errno));
    close(conf->ipv6_fd);
    conf->ipv6_fd = -1;
  }

  if(conf->ipv6_fd >= 0) {
    eventer_t newe;
    newe = eventer_alloc();
    newe->fd = conf->ipv6_fd;
    newe->mask = EVENTER_READ;
    newe->callback = noit_statsd_handler;
    newe->closure = self;
    eventer_add(newe);
  }

  noit_module_set_userdata(self, conf);
  return 0;
}

#include "statsd.xmlh"
noit_module_t statsd = {
	{
		NOIT_MODULE_MAGIC,
		NOIT_MODULE_ABI_VERSION,
		"statsd",
		"statsd collection",
		statsd_xml_descriptions,
		noit_statsd_onload
	},
	noit_statds_config,
	noit_statsd_init,
	noit_statsd_initiate_check,
	NULL
};
