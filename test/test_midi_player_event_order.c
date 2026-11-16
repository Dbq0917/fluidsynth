#include "test.h"
#include "fluidsynth.h"
#include "midi/fluid_midi.h"

typedef struct
{
    int count;
    struct
    {
        int type;
        int param1;
        int param2;
    } events[16];
} event_log_t;

static int log_event(void *data, fluid_midi_event_t *event)
{
    event_log_t *log = data;
    int type = fluid_midi_event_get_type(event);

    if(type == NOTE_ON || type == NOTE_OFF ||
            (type == CONTROL_CHANGE && fluid_midi_event_get_control(event) == SUSTAIN_SWITCH))
    {
        TEST_ASSERT(log->count < (int)(sizeof(log->events) / sizeof(log->events[0])));
        log->events[log->count].type = type;
        log->events[log->count].param1 =
            type == CONTROL_CHANGE ? fluid_midi_event_get_control(event) : fluid_midi_event_get_key(event);
        log->events[log->count].param2 =
            type == CONTROL_CHANGE ? fluid_midi_event_get_value(event) : fluid_midi_event_get_velocity(event);
        log->count++;
    }

    return FLUID_OK;
}

int main(void)
{
    /*
     * Format 1, 72 PPQN. Track 2 presses sustain at tick 95 and releases it
     * at tick 97, while track 1 releases the note at tick 96. At 8 kHz all
     * three events fall into the same 64-frame block.
     */
    unsigned char midi[] = {
        'M','T','h','d', 0,0,0,6, 0,1, 0,3, 0,72,
        'M','T','r','k', 0,0,0,11,
        0,0xFF,0x51,3, 0x07,0xA1,0x20,
        0,0xFF,0x2F,0,
        'M','T','r','k', 0,0,0,12,
        0,0x90,60,100,
        96,0x80,60,64,
        0,0xFF,0x2F,0,
        'M','T','r','k', 0,0,0,12,
        95,0xB0,SUSTAIN_SWITCH,127,
        1,0xB0,SUSTAIN_SWITCH,0,
        0,0xFF,0x2F,0
    };
    fluid_settings_t *settings = new_fluid_settings();
    fluid_synth_t *synth;
    fluid_player_t *player;
    event_log_t log = {0};
    int i;

    TEST_ASSERT(settings != NULL);
    TEST_SUCCESS(fluid_settings_setnum(settings, "synth.sample-rate", 8000));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.reverb.active", 0));
    TEST_SUCCESS(fluid_settings_setint(settings, "synth.chorus.active", 0));

    synth = new_fluid_synth(settings);
    TEST_ASSERT(synth != NULL);
    player = new_fluid_player(synth);
    TEST_ASSERT(player != NULL);

    TEST_SUCCESS(fluid_player_set_playback_callback(player, log_event, &log));
    TEST_SUCCESS(fluid_player_add_mem(player, midi, sizeof(midi)));
    TEST_SUCCESS(fluid_player_play(player));

    for(i = 0; i < 1000 && fluid_player_get_status(player) == FLUID_PLAYER_PLAYING; i++)
    {
        TEST_SUCCESS(fluid_synth_process(synth, 64, 0, NULL, 0, NULL));
    }

    TEST_ASSERT(log.count == 4);
    TEST_ASSERT(log.events[0].type == NOTE_ON && log.events[0].param1 == 60);
    TEST_ASSERT(log.events[1].type == CONTROL_CHANGE &&
                log.events[1].param1 == SUSTAIN_SWITCH && log.events[1].param2 == 127);
    TEST_ASSERT(log.events[2].type == NOTE_OFF && log.events[2].param1 == 60);
    TEST_ASSERT(log.events[3].type == CONTROL_CHANGE &&
                log.events[3].param1 == SUSTAIN_SWITCH && log.events[3].param2 == 0);

    delete_fluid_player(player);
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    return EXIT_SUCCESS;
}
