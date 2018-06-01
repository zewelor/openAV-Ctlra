

#include "audio.h"

#include <stdlib.h>
#include "dsp_forga.h"

struct smpla_t {
	forga_t forga;
	struct sampler_t *sampler;
};


struct smpla_t *smpla_init(int rate)
{
	struct smpla_t *s = calloc(1, sizeof(struct smpla_t));
	if(!s)
		return 0;

	instanceInitforga(&s->forga, rate);

	return s;
}

void smpla_free(struct smpla_t *s)
{
	free(s);
}

int smpla_process(struct smpla_t *s,
		  uint32_t nframes,
		  const float *inputs[],
		  float *outputs[])
{
	const float *in_l = inputs[0];
	const float *in_r = inputs[1];
	float *out_l = outputs[0];
	float *out_r = outputs[1];

	for(int i = 0; i < nframes; i++) {
		out_l[i] = in_l[i];
		out_r[i] = in_r[i];
	}

	return 0;
}