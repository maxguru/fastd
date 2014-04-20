/*
  Copyright (c) 2012-2014, Matthias Schiffer <mschiffer@universe-factory.net>
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


#pragma once

#include "../fastd.h"


#define COMMON_NONCEBYTES 6
#define COMMON_FLAGBYTES 1

#define COMMON_HEADBYTES (COMMON_NONCEBYTES+COMMON_FLAGBYTES)

typedef struct fastd_method_common {
	struct timespec valid_till;
	struct timespec refresh_after;

	uint8_t send_nonce[COMMON_NONCEBYTES];
	uint8_t receive_nonce[COMMON_NONCEBYTES];

	struct timespec reorder_timeout;
	uint64_t receive_reorder_seen;
} fastd_method_common_t;


void fastd_method_common_init(fastd_method_common_t *session, bool initiator);
bool fastd_method_is_nonce_valid(const fastd_method_common_t *session, const uint8_t nonce[COMMON_NONCEBYTES], int64_t *age);
bool fastd_method_reorder_check(fastd_peer_t *peer, fastd_method_common_t *session, const uint8_t nonce[COMMON_NONCEBYTES], int64_t age);


static inline bool fastd_method_session_common_is_valid(const fastd_method_common_t *session) {
	if (session->send_nonce[0] == 0xff && session->send_nonce[1] == 0xff)
		return false;

	return (!fastd_timed_out(&session->valid_till));
}

static inline bool fastd_method_session_common_is_initiator(const fastd_method_common_t *session) {
	return (session->send_nonce[COMMON_NONCEBYTES-1] & 1);
}

static inline bool fastd_method_session_common_want_refresh(const fastd_method_common_t *session) {
	if (session->send_nonce[0] == 0xff)
		return true;

	if (fastd_method_session_common_is_initiator(session) && fastd_timed_out(&session->refresh_after))
		return true;

	return false;
}

static inline void fastd_method_session_common_superseded(fastd_method_common_t *session) {
	struct timespec valid_max = fastd_in_seconds(conf.key_valid_old);

	if (timespec_after(&session->valid_till, &valid_max))
		session->valid_till = valid_max;
}

static inline void fastd_method_increment_nonce(fastd_method_common_t *session) {
	session->send_nonce[COMMON_NONCEBYTES-1] += 2;

	if (!(session->send_nonce[COMMON_NONCEBYTES-1] & (~1))) {
		int i;
		for (i = COMMON_NONCEBYTES-2; i >= 0; i--) {
			if (++session->send_nonce[i])
				break;
		}
	}
}

static inline void fastd_method_put_common_header(fastd_buffer_t *buffer, const uint8_t nonce[COMMON_NONCEBYTES], uint8_t flags) {
	fastd_buffer_pull_head_from(buffer, nonce, COMMON_NONCEBYTES);
	fastd_buffer_pull_head_from(buffer, &flags, 1);
}

static inline void fastd_method_take_common_header(fastd_buffer_t *buffer, uint8_t nonce[COMMON_NONCEBYTES], uint8_t *flags) {
	fastd_buffer_push_head_to(buffer, flags, 1);
	fastd_buffer_push_head_to(buffer, nonce, COMMON_NONCEBYTES);
}

static inline bool fastd_method_handle_common_header(const fastd_method_common_t *session, fastd_buffer_t *buffer, uint8_t nonce[COMMON_NONCEBYTES], uint8_t *flags, int64_t *age) {
	fastd_method_take_common_header(buffer, nonce, flags);
	return fastd_method_is_nonce_valid(session, nonce, age);
}


static inline void fastd_method_expand_nonce(uint8_t *buf, const uint8_t nonce[COMMON_NONCEBYTES], size_t len) {
	if (!len)
		return;

	memset(buf, 0, len);
	memcpy(buf, nonce, min_size_t(len, COMMON_NONCEBYTES));
	buf[len-1] = 1;
}
