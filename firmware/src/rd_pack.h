/* SPDX-License-Identifier: GPL-3.0-only: bounded lossless resource decoding. */
#pragma once
#include <stdint.h>
static int rd_unpack(const uint8_t *src,uint32_t bytes,uint8_t *dst,uint32_t size){
 const uint8_t *p=src,*end=src+bytes;uint32_t at=0;
 while(p<end){
  uint32_t token=*p++,n=token>>4;
  if(n==15){uint32_t v;do{if(p==end)return 0;v=*p++;n+=v;if(n>size)return 0;}while(v==255);}
  if(n>(uint32_t)(end-p)||n>size-at)return 0;for(uint32_t i=0;i<n;i++)dst[at+i]=p[i];p+=n;at+=n;if(p==end)break;
  if(end-p<2)return 0;uint32_t off=p[0]|(uint32_t)p[1]<<8;p+=2;if(!off||off>at)return 0;
  n=token&15;if(n==15){uint32_t v;do{if(p==end)return 0;v=*p++;n+=v;if(n>size)return 0;}while(v==255);}
  n+=4;if(n>size-at)return 0;while(n--){dst[at]=dst[at-off];at++;}
 }
 return at==size;
}
typedef struct {const uint8_t *bytes;const uint32_t *offsets;uint32_t size;} rd_blocks_t;
typedef struct {const rd_blocks_t *owner;uint32_t block;uint8_t data[4096];} rd_block_cache_t;
static uint8_t rd_block_byte(const rd_blocks_t *src,rd_block_cache_t *cache,uint32_t at){
 if(at>=src->size)return 0;uint32_t block=at/4096u;
 if(cache->owner!=src||cache->block!=block){
  uint32_t start=src->offsets[block],end=src->offsets[block+1],n=src->size-block*4096u;if(n>4096u)n=4096u;
  if(!rd_unpack(src->bytes+start,end-start,cache->data,n))return 0;
  cache->owner=src;cache->block=block;
 }
 return cache->data[at%4096u];
}
