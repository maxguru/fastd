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

#include "peer.h"
#include "task.h"

#include <arpa/inet.h>


const fastd_eth_addr* fastd_get_source_address(const fastd_context *ctx, fastd_buffer buffer) {
	switch (ctx->conf->protocol) {
	case PROTOCOL_ETHERNET:
		return (fastd_eth_addr*)&((struct ethhdr*)buffer.data)->h_source;
	default:
		exit_bug(ctx, "invalid protocol");
	}
}

const fastd_eth_addr* fastd_get_dest_address(const fastd_context *ctx, fastd_buffer buffer) {
	switch (ctx->conf->protocol) {
	case PROTOCOL_ETHERNET:
		return (fastd_eth_addr*)&((struct ethhdr*)buffer.data)->h_dest;
	default:
		exit_bug(ctx, "invalid protocol");
	}
}

static inline void reset_peer(fastd_context *ctx, fastd_peer *peer) {
	int i, deleted = 0;
	for (i = 0; i < ctx->n_eth_addr; i++) {
		if (ctx->eth_addr[i].peer == peer) {
			deleted++;
		}
		else if (deleted) {
			ctx->eth_addr[i-deleted] = ctx->eth_addr[i];
		}
	}

	ctx->n_eth_addr -= deleted;

	fastd_task_delete_peer(ctx, peer);

}

static inline void setup_peer(fastd_context *ctx, fastd_peer *peer) {
	if (fastd_peer_is_temporary(peer)) {
		exit_fatal(ctx, "tried to reset temporary peer");
	}

	peer->address = peer->config->address;
	peer->state = STATE_WAIT;
	peer->seen = (struct timespec){0, 0};

	if (!fastd_peer_is_floating(peer))
		fastd_task_schedule_handshake(ctx, peer, 0);
}

void fastd_peer_reset(fastd_context *ctx, fastd_peer *peer) {
	if (is_debug(ctx)) {
		switch (peer->address.sa.sa_family) {
		case AF_UNSPEC:
			pr_debug(ctx, "resetting peer <floating>");
			break;

		case AF_INET:
			pr_debug(ctx, "resetting peer %I:%u", &peer->address.in.sin_addr, ntohs(peer->address.in.sin_port));
			break;

		case AF_INET6:
			pr_debug(ctx, "resetting peer [%lI]:%u", &peer->address.in6.sin6_addr, ntohs(peer->address.in6.sin6_port));
			break;

		default:
			exit_bug(ctx, "unsupported address family");
		}
	}

	reset_peer(ctx, peer);
	setup_peer(ctx, peer);
}



static fastd_peer* add_peer(fastd_context *ctx) {
	fastd_peer *peer = malloc(sizeof(fastd_peer));

	peer->next = ctx->peers;
	peer->last_req_id = 0;

	ctx->peers = peer;

	return peer;
}

fastd_peer* fastd_peer_add(fastd_context *ctx, fastd_peer_config *peer_conf) {
	fastd_peer *peer = add_peer(ctx);

	peer->config = peer_conf;

	setup_peer(ctx, peer);

	pr_debug(ctx, "adding peer %P", peer);

	return peer;
}

fastd_peer* fastd_peer_add_temp(fastd_context *ctx, const fastd_peer_address *address) {
	fastd_peer *peer = add_peer(ctx);

	if (!ctx->conf->n_floating)
		exit_bug(ctx, "tried to add a temporary peer with no floating remotes defined");

	peer->config = NULL;
	peer->address = *address;
	peer->state = STATE_TEMP;
	peer->seen = ctx->now;

	pr_debug(ctx, "adding peer %P (temporary)", peer);

	return peer;
}

fastd_peer* fastd_peer_merge(fastd_context *ctx, fastd_peer *perm_peer, fastd_peer *temp_peer) {
	pr_debug(ctx, "merging peer %P into %P", temp_peer, perm_peer);

	perm_peer->address = temp_peer->address;
	perm_peer->state = fastd_peer_is_established(temp_peer) ? STATE_ESTABLISHED : STATE_WAIT;
	perm_peer->seen = temp_peer->seen;

	int i;
	for (i = 0; i < ctx->n_eth_addr; i++) {
		if (ctx->eth_addr[i].peer == temp_peer) {
			ctx->eth_addr[i].peer = perm_peer;
		}
	}

	fastd_peer_delete(ctx, temp_peer);

	return perm_peer;
}

void fastd_peer_delete(fastd_context *ctx, fastd_peer *peer) {
	pr_debug(ctx, "deleting peer %P", peer);

	reset_peer(ctx, peer);

	fastd_peer **cur_peer;
	for (cur_peer = &ctx->peers; *cur_peer; cur_peer = &(*cur_peer)->next) {
		if ((*cur_peer) == peer) {
			*cur_peer = peer->next;
			break;
		}
	}

	free(peer);
}

static inline int fastd_eth_addr_cmp(const fastd_eth_addr *addr1, const fastd_eth_addr *addr2) {
	return memcmp(addr1->data, addr2->data, ETH_ALEN);
}

static inline int fastd_peer_eth_addr_cmp(const fastd_peer_eth_addr *addr1, const fastd_peer_eth_addr *addr2) {
	return fastd_eth_addr_cmp(&addr1->addr, &addr2->addr);
}

static inline fastd_peer_eth_addr* peer_get_by_addr(fastd_context *ctx, const fastd_eth_addr *addr) {
	return bsearch(container_of(addr, fastd_peer_eth_addr, addr), ctx->eth_addr, ctx->n_eth_addr, sizeof(fastd_peer_eth_addr),
		       (int (*)(const void *, const void *))fastd_peer_eth_addr_cmp);
}

void fastd_peer_eth_addr_add(fastd_context *ctx, fastd_peer *peer, const fastd_eth_addr *addr) {
	int min = 0, max = ctx->n_eth_addr;

	while (max > min) {
		int cur = (min+max)/2;
		int cmp = fastd_eth_addr_cmp(addr, &ctx->eth_addr[cur].addr);

		if (cmp == 0) {
			ctx->eth_addr[cur].peer = peer;
			ctx->eth_addr[cur].seen = ctx->now;
			return; /* We're done here. */
		}
		else if (cmp < 0) {
			max = cur;
		}
		else {
			min = cur+1;
		}
	}

	ctx->n_eth_addr++;
	if (ctx->n_eth_addr > ctx->eth_addr_size) {
		if (ctx->eth_addr_size == 0)
			ctx->eth_addr_size = 16;
		else
			ctx->eth_addr_size *= 2;

		ctx->eth_addr = realloc(ctx->eth_addr, ctx->eth_addr_size*sizeof(fastd_peer_eth_addr));
	}

	int i;
	for (i = ctx->n_eth_addr-1; i > min+1; i--)
		ctx->eth_addr[i] = ctx->eth_addr[i-1];
	
	ctx->eth_addr[min] = (fastd_peer_eth_addr){ *addr, peer, ctx->now };

	pr_debug(ctx, "Learned new MAC address %E", addr);
}

void fastd_peer_eth_addr_cleanup(fastd_context *ctx) {
	int i, deleted = 0;

	for (i = 0; i < ctx->n_eth_addr; i++) {
		if (timespec_diff(&ctx->now, &ctx->eth_addr[i].seen) > ctx->conf->eth_addr_stale_time*1000) {
			deleted++;
			pr_debug(ctx, "MAC address %E not seen for more than %u seconds, removing",
				 &ctx->eth_addr[i].addr, ctx->conf->eth_addr_stale_time);
		}
		else if (deleted) {
			ctx->eth_addr[i-deleted] = ctx->eth_addr[i];
		}
	}

	ctx->n_eth_addr -= deleted;
}

fastd_peer *fastd_peer_find_by_eth_addr(fastd_context *ctx, const fastd_eth_addr *addr) {
	fastd_peer_eth_addr *peer_eth_addr = peer_get_by_addr(ctx, addr);

	if (peer_eth_addr)
		return peer_eth_addr->peer;
	else
		return NULL;
}
