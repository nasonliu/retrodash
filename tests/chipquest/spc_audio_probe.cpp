#include "rd_spc.h"
#include <cstdio>
#include <cstdlib>
#include <cassert>
int main(int argc,char **argv){
 unsigned seconds=argc>2?unsigned(std::atoi(argv[2])):20;
 FILE *f=std::fopen(argv[1],"wb");assert(f);
 rd_spc_init();assert(rd_spc_status(0)==0);rd_spc_start(0,100);
 for(unsigned t=0;t<seconds*44100;t+=256){
  int32_t out[512]={0};unsigned n=seconds*44100-t;if(n>256)n=256;
  rd_spc_render(out,n,t);
  for(unsigned i=0;i<n*2;i++){unsigned v=(uint16_t)out[i];std::fputc(v&255,f);std::fputc(v>>8,f);}
 }
 assert(rd_spc_status(0)==0);
 std::printf("DSP clocks %u; manual hits %u; suppressed %u; fills %u\n",rd_spc_status(1),rd_spc_status(2),rd_spc_status(3),rd_spc_status(4));
 std::fclose(f);
}
