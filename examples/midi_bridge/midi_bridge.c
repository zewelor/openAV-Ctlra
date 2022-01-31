#define _DEFAULT_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <impl.h>

#include "devices/ni_maschine_mikro_mk3.h"
#include "ctlra.h"
#include "midi.h"

static volatile uint32_t done;

#define GRID_SIZE 64

#define MIDI_CHANNEL_MAX 16
#define MIDI_MAX_NOTE 127
#define MIDI_MAX_VELOCITY 127
#define MIDI_STARTING_NOTE 24

#define MIDI_CHANNEL_ACTIVE_COLOR 0xff00ff00

const uint32_t group_pads_mapping[] = {
    0x000000ff, 0x0000ff00, 0x00ff0000, 0x00ff00ff, 0x00ffff00, 0x0000ffff, 0x00ffffff
};

struct mm_t
{
    uint8_t shift_pressed;
    uint8_t group_pressed;

    /* GROUP */
    uint8_t group_id; /* 0 - 5, selected group */
    uint8_t max_groups;

    uint8_t midi_channel;
    uint8_t fixed_velocity;
    uint8_t encoder_value[MIDI_CHANNEL_MAX];
};

/* a struct to pass around as userdata to callbacks */
struct daemon_t {
    struct ctlra_dev_t* dev;
    struct ctlra_midi_t *midi;
    int has_grid;
    uint8_t pads_count;
    uint32_t grid_col;
    uint8_t grid[GRID_SIZE];

    struct ctlra_dev_info_t info;
    struct mm_t* mm;
};

void demo_feedback_func(struct ctlra_dev_t *dev, void *d)
{
    struct daemon_t *daemon = d;

    if(daemon->has_grid) {
        struct mm_t *mm = daemon->mm;

        uint32_t col;

        col = mm->fixed_velocity * 0xffffffff;
        ctlra_dev_light_set(dev, NI_MASCHINE_MIKRO_MK3_BTN_FIXED_VEL, col);

        if(mm->group_pressed) {
            for (int i = 0; i < mm->max_groups; i++) {
                int id = daemon->info.grid_info[0].info.params[0] + i;
                col = group_pads_mapping[i];
                if (mm->group_id == i) {
                    col += 0xff000000; /* Lighten up currently selected group */
                } else {
                    col += 0x00000000;
                }
                ctlra_dev_light_set(dev, id, col);
            }
        } else if(mm->shift_pressed) {
            for (int i = 0; i < MIDI_CHANNEL_MAX; i++) {
                int id = daemon->info.grid_info[0].info.params[0] + i;
                if (mm->midi_channel == i) {
                    col = MIDI_CHANNEL_ACTIVE_COLOR; /* Lighten up currently selected group */
                } else {
                    col = 0;
                }
                ctlra_dev_light_set(dev, id, col);
            }
        } else {
            for(int i = 0; i < daemon->pads_count; i++) {
                int id = daemon->info.grid_info[0].info.params[0] + i;
                col = daemon->grid_col * (daemon->grid[i] > 0);
                ctlra_dev_light_set(dev, id, col);
            }
        }
    }

    ctlra_dev_light_flush(dev, 0);
}

int ignored_input_cb(uint8_t nbytes, uint8_t * buffer, void *ud)
{
    return 0;
}

void demo_event_func(struct ctlra_dev_t* dev,
                     uint32_t num_events,
                     struct ctlra_event_t** events,
                     void *userdata)
{
    struct daemon_t *daemon = userdata;
    struct mm_t *mm = daemon->mm;
    struct ctlra_midi_t *midi = daemon->midi;
    uint8_t msg[3] = {0};

    for(uint32_t i = 0; i < num_events; i++) {
        struct ctlra_event_t *e = events[i];
        int ret;
        switch(e->type) {
            case CTLRA_EVENT_BUTTON:
                if(mm->shift_pressed) {
                    switch (e->button.id) {
                        case NI_MASCHINE_MIKRO_MK3_BTN_FIXED_VEL:
                            if(e->button.pressed) {
                                mm->fixed_velocity = !mm->fixed_velocity;
                            }
                            break;
                        case NI_MASCHINE_MIKRO_MK3_BTN_SHIFT:
                            mm->shift_pressed = e->button.pressed;
                            break;
                    }
                } else {
                    switch (e->button.id) {
                        case NI_MASCHINE_MIKRO_MK3_BTN_GROUP:
                            mm->group_pressed = e->button.pressed;
                            break;
                        case NI_MASCHINE_MIKRO_MK3_BTN_SHIFT:
                            mm->shift_pressed = e->button.pressed;
                            break;
                        default:
                            msg[0] = 0xb0 + mm->midi_channel;
                            msg[1] = 60 + e->button.id;
                            msg[2] = e->button.pressed ? MIDI_MAX_VELOCITY : 0;
                            ret = ctlra_midi_output_write(midi, 3, msg);
                            break;
                    }
                }
                break;
            case CTLRA_EVENT_ENCODER:
                msg[0] = 0xb0 + mm->midi_channel;
                msg[1] = 2;
                int16_t new_value;
                if(mm->shift_pressed) {
                    new_value = mm->encoder_value[mm->midi_channel] + e->encoder.delta * 4;
                } else {
                    new_value = mm->encoder_value[mm->midi_channel] + e->encoder.delta;
                }
                if(new_value > MIDI_MAX_NOTE) {
                    mm->encoder_value[mm->midi_channel] = MIDI_MAX_NOTE;
                } else if (new_value < 0) {
                    mm->encoder_value[mm->midi_channel] = 0;
                } else {
                    mm->encoder_value[mm->midi_channel] = new_value;
                }
                msg[2] = mm->encoder_value[mm->midi_channel];
                ret = ctlra_midi_output_write(midi, 3, msg);
                break;

            case CTLRA_EVENT_SLIDER:
                msg[0] = 0xb0 + mm->midi_channel;
                msg[1] = 1;
                msg[2] = (int)(e->slider.value * 127.f);
                ret = ctlra_midi_output_write(midi, 3, msg);
                break;

            case CTLRA_EVENT_GRID: {
                if(mm->group_pressed) {
                    if (e->grid.pos > mm->max_groups) {
                        mm->group_id = mm->max_groups;
                    } else {
                        mm->group_id = e->grid.pos;
                    }
                } else if(mm->shift_pressed) {
                    if (e->grid.pos > MIDI_CHANNEL_MAX) {
                        mm->midi_channel = MIDI_CHANNEL_MAX;
                    } else {
                        mm->midi_channel = e->grid.pos;
                    }
                } else {
                    msg[0] = (e->grid.pressed ? 0x90 : 0x80) + mm->midi_channel;
                    int pos = e->grid.pos;
                    daemon->grid[pos] = e->grid.pressed;
                    msg[1] = MIDI_STARTING_NOTE + mm->group_id * daemon->pads_count + pos;
                    if(e->grid.pressed) {
                        msg[2] = mm->fixed_velocity ? MIDI_MAX_VELOCITY : e->grid.pressure * MIDI_MAX_VELOCITY;
                    } else {
                        msg[2] = 0;
                    }
                    ret = ctlra_midi_output_write(midi, 3, msg);
                }
                break;
            }
            default:
                break;
        };
        // TODO: Error check midi writes
        (void) ret;
    }
}

void sighndlr(int signal)
{
    done = 1;
    printf("\n");
}

void remove_dev_func(struct ctlra_dev_t *dev, int unexpected_removal,
                     void *userdata)
{
    struct daemon_t *daemon = userdata;
    ctlra_midi_destroy(daemon->midi);
    free(daemon);
    struct ctlra_dev_info_t info;
    ctlra_dev_get_info(dev, &info);
    printf("MidiBridge: removing %s %s\n", info.vendor, info.device);
}

int accept_dev_func(struct ctlra_t *ctlra,
                    const struct ctlra_dev_info_t *info,
                    struct ctlra_dev_t *dev,
                    void *userdata)
{
    printf("MidiBridge: accepting %s %s\n", info->vendor, info->device);
    ctlra_dev_set_event_func(dev, demo_event_func);
    ctlra_dev_set_feedback_func(dev, demo_feedback_func);
    ctlra_dev_set_remove_func(dev, remove_dev_func);

    /* TODO: open MIDI output, store pointer in device */
    struct daemon_t *daemon = calloc(1, sizeof(struct daemon_t));
    if(!daemon)
        goto fail;

    daemon->midi = ctlra_midi_open(info->device,
                                   ignored_input_cb,
                                   0x0);

    daemon->info = *info;
    if(info->control_count[CTLRA_EVENT_GRID] > 0) {
        daemon->has_grid = 1;
        daemon->pads_count = daemon->info.grid_info[0].info.params[1] - daemon->info.grid_info[0].info.params[0];
        daemon->grid_col = 0xff0040ff;

        daemon->mm = calloc(1, sizeof(struct mm_t));
        daemon->mm->max_groups = (MIDI_MAX_NOTE - MIDI_STARTING_NOTE) / daemon->pads_count;

        /* easter egg: set env var to change colour of pads */
        char *col = getenv("CTLRA_COLOUR");
        if(col)
            daemon->grid_col = atoi(col);
    }

    if(!daemon->midi)
        goto fail;

    ctlra_dev_set_callback_userdata(dev, daemon);

    return 1;
fail:
    printf("failed to alloc/open midi backend\n");
    if(daemon)
        free(daemon);

    return 0;
}

int main()
{
    signal(SIGINT, sighndlr);

    struct ctlra_t *ctlra = ctlra_create(NULL);
    int num_devs = ctlra_probe(ctlra, accept_dev_func, 0x0);
    printf("MidiBridge: connected devices: %d\n", num_devs);

    while(!done) {
        ctlra_idle_iter(ctlra);
        usleep(1000);
    }

    ctlra_exit(ctlra);

    return 0;
}
