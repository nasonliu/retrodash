/* Offline reference renderer. Link against unmodified LGPL snes_spc sources. */
#include "SNES_SPC.h"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>
#include <string>

static FILE *trace;
static uint64_t base_clock;
static void le32(FILE *f, uint32_t n) {
    for (unsigned i=0; i<4; ++i) std::fputc((n>>(8*i))&255,f);
}
void rd_spc_dsp_write(int clock, int reg, int value) {
    if (!trace || reg>=128) return;
    le32(trace, uint32_t(base_clock+clock));
    std::fputc(reg,trace); std::fputc(value,trace);
}
static void fail(const char *e) { std::fprintf(stderr,"%s\n",e); std::exit(1); }
int main(int argc, char **argv) {
    if(argc!=5) fail("usage: spc_reference input.spc seconds output.pcm output.trace");
    FILE *f=std::fopen(argv[1],"rb"); if(!f) fail("cannot read SPC");
    std::vector<unsigned char> data;
    for(int c;(c=std::fgetc(f))!=EOF;) data.push_back((unsigned char)c);
    std::fclose(f);
    int seconds=std::atoi(argv[2]); if(seconds<1 || seconds>600) fail("seconds must be 1..600");
    SNES_SPC spc; const char *e=spc.init(); if(e) fail(e);
    e=spc.load_spc(data.data(),data.size()); if(e) fail(e);
    // Preserve the supplied snapshot, including its echo RAM, for reference playback.
    f=std::fopen(argv[3],"wb"); trace=std::fopen(argv[4],"wb");
    if(!f || !trace) fail("cannot create output");
    short samples[1280];
    for(int i=0;i<seconds*50;++i) {
        e=spc.play(1280,samples); if(e) fail(e);
        for(short s:samples) { unsigned v=(uint16_t)s; std::fputc(v&255,f);std::fputc(v>>8,f); }
        base_clock+=640*32;
    }
    std::fclose(f); std::fclose(trace); trace=nullptr;
    spc.save_spc(data.data());
    std::string end_path=std::string(argv[4])+".end.spc";
    f=std::fopen(end_path.c_str(),"wb"); if(!f) fail("cannot save end state");
    std::fwrite(data.data(),1,SNES_SPC::spc_file_size,f);std::fclose(f);
}
