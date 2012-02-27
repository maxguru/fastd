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


#ifndef _FASTD_TASK_H_
#define _FASTD_TASK_H_

#include "fastd.h"

#include <sys/uio.h>


typedef enum _fastd_task_type {
	TASK_SEND,
	TASK_HANDLE_RECV,
} fastd_task_type;

typedef struct _fastd_task_send {
	fastd_task_type type;
	const fastd_peer *peer;
	uint8_t packet_type;
	fastd_buffer buffer;
} fastd_task_send;

typedef struct _fastd_task_handle_recv {
	fastd_task_type type;
	const fastd_peer *peer;
	uint8_t packet_type;
	fastd_buffer buffer;
} fastd_task_handle_recv;

typedef union _fastd_task {
	fastd_task_type type;
	fastd_task_send send;
	fastd_task_handle_recv handle_recv;
} fastd_task;


fastd_task* fastd_task_get(fastd_context *ctx);
static inline int fastd_task_timeout(fastd_context *ctx) {	
	return fastd_queue_timeout(&ctx->task_queue);
}

void fastd_task_put_send_handshake(fastd_context *ctx, const fastd_peer *peer, fastd_buffer buffer);

void fastd_task_put_send(fastd_context *ctx, const fastd_peer *peer, fastd_buffer buffer);
void fastd_task_put_handle_recv(fastd_context *ctx, const fastd_peer *peer, fastd_buffer buffer);

#endif /* _FASTD_TASK_H_ */