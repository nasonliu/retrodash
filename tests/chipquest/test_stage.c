#define DINGHY_HOST
#define main cq_host_main_unused
#include "../../firmware/src/retrodash.c"
#undef main
#include <assert.h>
int main(void){
 int32_t impact,lead,x,prev;uint32_t t;
 cq_init_project();kf_init();kg_start(CQ_SLOT,100,0,44100);
 assert(kg.song->bpm_x100==7200u);assert(cq_get(148)==1800u);
 t=kg.notes[0].t;impact=(int32_t)t*256;lead=cq_approach_pos();
 assert(cq_stage_x(t,impact)==64);
 x=cq_stage_x(t,impact-lead);assert(x>=227&&x<=229);
 prev=cq_stage_x(t,impact-lead);
 for(int32_t p=impact-lead+256;p<=impact;p+=256){x=cq_stage_x(t,p);assert(x<=prev&&prev-x<=2);prev=x;}
 /* While the first note reaches its target, the second enemy is already visible. */
 x=cq_stage_x(kg.notes[1].t,impact);assert(x>64&&x<228);
 for(unsigned i=1;i<kg.n;i++)assert(kg.notes[i].t-kg.notes[i-1].t==96u);
 puts("72 BPM: one note per beat, 1.8 s approach, monotonic movement and following enemy visible passed");return 0;
}
