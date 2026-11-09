#include <assert.h>
#include <stdio.h>
#define KF_POOL
#include "kf_engine.h"
#include "kf_game.h"
static unsigned last_pitch, calls, playing[27];
static void kg_sound_on(uint32_t key, uint32_t pitch, int32_t detune, uint32_t bright) { (void)key;(void)detune;(void)bright; last_pitch=pitch; calls++; playing[key]=1; }
static void kg_sound_off(uint32_t key, uint32_t pitch) { (void)pitch; playing[key]=0; }
int main(void) {
    unsigned idx=KF_NSONG-1, i, f, key; uint32_t t;
    assert(KF_SONG[idx].transpose == 127);
    kf_init(); kg_start(idx, 100, 0, 44100);
    for (i=0;i<kg.n;i++) { unsigned j; assert(kg.notes[i].lane<27); for(j=0;j<i;j++) if(kg.notes[j].pitch==kg.notes[i].pitch) assert(kg.notes[j].lane==kg.notes[i].lane); }
    t=kg_note_frame(0); key=kg.notes[0].lane; kg_update(t-1000); kg_press(key,t);
    assert(kg.judge==KJ_PERFECT && kg.nst[0]==KN_HIT && last_pitch==kg.notes[0].pitch && calls);
    kf_init(); kg_start(idx,100,0,44100); kg.auto_on=1;
    for(f=0; f<44100u*400u; f+=256u) { kg_ms=f/44; if(kg_update(f)) break; }
    assert(kg.done && kg.acc10==1000 && kg.cnt[3]==0 && kg.full);
    for(i=0;i<27;i++) assert(!playing[i]);
    puts("fixed-key chart: stable mapping, player pitch, judgement and full autoplay passed");
    return 0;
}
