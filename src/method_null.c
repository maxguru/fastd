/*
  Copyright (c) 2012, Matthias Schiffer <mschiffer@universe-factory.net>
  Partly based on QuickTun Copyright (c) 2010, Ivo Smits <Ivo@UCIS.nl>.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#define _GNU_SOURCE

#include "fastd.h"
#include "task.h"
#include "peer.h"

#include <arpa/inet.h>



static bool null_check_config(fastd_context *ctx, const fastd_config *conf) {
	if (conf->n_floating > 1) {
		pr_error(ctx, "with method `null' use can't define more than one floating peer");
		return false;
	}

	return true;
}

static size_t null_max_packet_size(fastd_context *ctx) {
	return fastd_max_packet_size(ctx);
}

static char* null_peer_str(const fastd_context *ctx, const fastd_peer *peer) {
	char addr_buf[INET6_ADDRSTRLEN] = "";
	char *ret;

	switch (peer->address.sa.sa_family) {
	case AF_UNSPEC:
		return strdup("<floating>");

	case AF_INET:
		if (inet_ntop(AF_INET, &peer->address.in.sin_addr, addr_buf, sizeof(addr_buf))) {
			if (asprintf(&ret, "%s:%u", addr_buf, ntohs(peer->address.in.sin_port)) > 0)
				return ret;
		}
		break;

	case AF_INET6:
		if (inet_ntop(AF_INET6, &peer->address.in6.sin6_addr, addr_buf, sizeof(addr_buf))) {
			if (asprintf(&ret, "[%s]:%u", addr_buf, ntohs(peer->address.in6.sin6_port)) > 0)
				return ret;
		}
		break;

	default:
		exit_bug(ctx, "unsupported address family");
	}

	return NULL;
}

static void null_init(fastd_context *ctx, fastd_peer *peer) {
	fastd_task_put_send(ctx, peer, fastd_buffer_alloc(0, 0, 0));
}

static void null_handle_recv(fastd_context *ctx, fastd_peer *peer, fastd_buffer buffer) {
	if (!fastd_peer_is_established(peer)) {
		pr_info(ctx, "Connection established.");
		fastd_peer_set_established(ctx, peer);
	}

	if (fastd_peer_is_temporary(peer)) {
		fastd_peer *perm_peer;
		for (perm_peer = ctx->peers; perm_peer; perm_peer = perm_peer->next) {
			if (fastd_peer_is_floating(perm_peer))
				break;
		}

		if (!perm_peer) {
			fastd_buffer_free(buffer);
			return;
		}

		peer = fastd_peer_merge(ctx, perm_peer, peer);
	}
	
	if (buffer.len)
		fastd_task_put_handle_recv(ctx, peer, buffer);
	else
		fastd_buffer_free(buffer);
}

static void null_send(fastd_context *ctx, fastd_peer *peer, fastd_buffer buffer) {
	fastd_task_put_send(ctx, peer, buffer);
}


const fastd_method fastd_method_null = {
	.name = "null",

	.check_config = null_check_config,

	.max_packet_size = null_max_packet_size,

	.peer_str = null_peer_str,

	.init = null_init,
	.handle_recv = null_handle_recv,
	.send = null_send,
};
