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

#define MIDI_MAX_NOTE 127
#define MIDI_STARTING_NOTE 24

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

        if(mm->group_pressed) {
            for(int i = 0; i < mm->max_groups; i++) {
                int id = daemon->info.grid_info[0].info.params[0] + i;
                uint32_t col = group_pads_mapping[i];
                if(mm->group_id == i) {
                    col += 0xff000000; /* Lighten up currently selected group */
                } else {
                    col += 0x00000000;
                }
                ctlra_dev_light_set(dev, id, col);
            }
        } else {
            for(int i = 0; i < daemon->pads_count; i++) {
                int id = daemon->info.grid_info[0].info.params[0] + i;
                uint32_t col = daemon->grid_col * (daemon->grid[i] > 0);
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
                switch (e->button.id) {
                    case NI_MASCHINE_MIKRO_MK3_BTN_GROUP:
                        mm->group_pressed = e->button.pressed;
                        break;
                    case NI_MASCHINE_MIKRO_MK3_BTN_SHIFT:
                        mm->shift_pressed = e->button.pressed;
                        break;
                    default:
                        msg[0] = 0xb0;
                        msg[1] = 60 + e->button.id;
                        msg[2] = e->button.pressed ? 0x7f : 0;
                        ret = ctlra_midi_output_write(midi, 3, msg);
                        break;
                }

            case CTLRA_EVENT_ENCODER:
                break;

            case CTLRA_EVENT_SLIDER:
                msg[0] = 0xb0;
                msg[1] = e->slider.id;
                msg[2] = (int)(e->slider.value * 127.f);
                ret = ctlra_midi_output_write(midi, 3, msg);
                break;

            case CTLRA_EVENT_GRID: {
                if(mm->group_pressed) {
                    if(e->grid.pos > mm->max_groups) {
                        mm->group_id = mm-> max_groups;
                    } else {
                        mm->group_id = e->grid.pos;
                    }
                    mm->group_pressed = 0;
                } else {
                    msg[0] = e->grid.pressed ? 0x90 : 0x80;
                    int pos = e->grid.pos;
                    daemon->grid[pos] = e->grid.pressed;
//                msg[1] = pos + 36; /* GM kick drum note */
                    msg[1] = MIDI_STARTING_NOTE + mm->group_id * daemon->pads_count + pos;
                    msg[2] = e->grid.pressed ?
                             e->grid.pressure * 127 : 0;
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
