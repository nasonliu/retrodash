#include "../../firmware/src/rd_pack.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 uint8_t dst[12];const uint8_t src[]={0x35,'a','b','c',3,0};
 assert(rd_unpack(src,sizeof src,dst,12));assert(!memcmp(dst,"abcabcabcabc",12));
 assert(!rd_unpack(src,sizeof src-1,dst,12));assert(!rd_unpack(src,sizeof src,dst,11));
 const uint8_t bad[]={0x10,'x',0,0};assert(!rd_unpack(bad,sizeof bad,dst,5));
 puts("Lossless decoder: overlap copies, truncated input, invalid offset and output bounds");
}
