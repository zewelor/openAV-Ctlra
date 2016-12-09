/*
 * Copyright (c) 2016, OpenAV Productions,
 * Harry van Haaren <harryhaaren@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "device_impl.h"

/* Uncomment to print on actions */
/* #define DEBUG_PRINTS */

struct ni_kontrol_z1_t {
	struct ctlr_dev_t base;
	uint32_t event_counter;
};

static uint32_t ni_kontrol_z1_poll(struct ctlr_dev_t *dev);
static int32_t ni_kontrol_z1_disconnect(struct ctlr_dev_t *dev);
static int32_t ni_kontrol_z1_disconnect(struct ctlr_dev_t *dev);
static void ni_kontrol_z1_light_set(struct ctlr_dev_t *dev, uint32_t light_id,
				uint32_t light_status);

struct ctlr_dev_t *ni_kontrol_z1_connect(ctlr_event_func event_func,
				  void *userdata, void *future)
{
	(void)future;
	struct ni_kontrol_z1_t *dev = calloc(1, sizeof(struct ni_kontrol_z1_t));
	if(!dev)
		goto fail;

	dev->base.poll = ni_kontrol_z1_poll;
	dev->base.disconnect = ni_kontrol_z1_disconnect;
	dev->base.light_set = ni_kontrol_z1_light_set;

	dev->base.event_func = event_func;
	dev->base.event_func_userdata = userdata;

	return (struct ctlr_dev_t *)dev;
fail:
	free(dev);
	return 0;
}

static uint32_t ni_kontrol_z1_poll(struct ctlr_dev_t *base)
{
	struct ni_kontrol_z1_t *dev = (struct ni_kontrol_z1_t *)base;

	/* when an event occurs, call the event writing callback */
	//dev->base.event_func(base, 1, e, dev->base.event_func_userdata);

	return 0;
}

static int32_t ni_kontrol_z1_disconnect(struct ctlr_dev_t *base)
{
	struct ni_kontrol_z1_t *dev = (struct ni_kontrol_z1_t *)base;
	free(dev);
	return 0;
}

static void ni_kontrol_z1_light_set(struct ctlr_dev_t *dev, uint32_t light_id,
				uint32_t light_status)
{
	uint32_t blink  = (light_status >> 31);
	uint32_t bright = (light_status >> 24) & 0x7F;
	uint32_t r      = (light_status >> 16) & 0xFF;
	uint32_t g      = (light_status >>  8) & 0xFF;
	uint32_t b      = (light_status >>  0) & 0xFF;
#ifdef DEBUG_PRINTS
	printf("%s : dev %p, light %d, status %d\n", __func__, dev,
	       light_id, light_status);
	printf("decoded: blink[%d], bright[%d], r[%d], g[%d], b[%d]\n",
	       blink, bright, r, g, b);
#endif
}
