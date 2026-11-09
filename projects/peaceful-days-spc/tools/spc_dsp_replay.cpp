/* Offline proof: replay timestamped S-DSP writes without an SPC700 CPU. */
#include "SPC_DSP.h"
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <cstdint>
static std::vector<unsigned char> read(const char *p) {
    FILE *f=std::fopen(p,"rb");if(!f) std::exit(2);
    std::vector<unsigned char> b;
    for(int c;(c=std::fgetc(f))!=EOF;) b.push_back((unsigned char)c);
    std::fclose(f);return b;
}
int main(int argc,char **argv) {
    if(argc!=5) return 2;
    auto spc=read(argv[1]),ev=read(argv[2]);
    if(spc.size()<0x10180 || ev.size()%6) return 2;
    int seconds=std::atoi(argv[3]);if(seconds<1 || seconds>600) return 2;
    std::vector<short> out(seconds*64000+32);
    SPC_DSP dsp;dsp.init(spc.data()+256);dsp.load(spc.data()+0x10100);
    dsp.set_output(out.data(),out.size());uint32_t now=0,end=seconds*1024000;
    for(size_t i=0;i<ev.size();i+=6) {
        uint32_t t=0;for(unsigned j=0;j<4;++j)t|=uint32_t(ev[i+j])<<(8*j);
        if(t<now || t>end) return 3;
        dsp.run(t-now);now=t;dsp.write(ev[i+4],ev[i+5]);
    }
    dsp.run(end-now);
    FILE *f=std::fopen(argv[4],"wb");if(!f) return 2;
    for(int i=0;i<seconds*64000;++i) {
        unsigned v=(uint16_t)out[i];std::fputc(v&255,f);std::fputc(v>>8,f);
    }
    std::fclose(f);
}
