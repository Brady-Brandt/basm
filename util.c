#include "util.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>


typedef struct {
    char* data;
    uint32_t offset;
    uint32_t size;
} ScratchBuffer;



static ScratchBuffer scratch_buffer = {0};

#define SCRATCH_BUFFER_SIZE 8092

void init_scratch_buffer(){
    scratch_buffer.data = malloc(SCRATCH_BUFFER_SIZE);
    scratch_buffer.offset = 0;

    if(scratch_buffer.data == NULL){
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
}

static inline void scratch_buffer_full(){
    if(scratch_buffer.offset >= SCRATCH_BUFFER_SIZE){
       fprintf(stderr, "ScratchBuffer is full"); 
       exit(EXIT_FAILURE);
    }
}

void scratch_buffer_append_char(char c){
    scratch_buffer_full();
    *((char*)scratch_buffer.data + scratch_buffer.offset) = c;
    scratch_buffer.offset += sizeof(char);
}


void scratch_buffer_clear(){
    scratch_buffer.offset = 0;
}


uint32_t scratch_buffer_offset(){
    return scratch_buffer.offset;
}


void* scratch_buffer_get_data(uint32_t offset){
    if(offset >= SCRATCH_BUFFER_SIZE) return NULL;
    return scratch_buffer.data + offset;
}


char* scratch_buffer_fmt(const char* fmt, ...){
    va_list list;
    va_start(list, fmt);
    uint32_t max_size = SCRATCH_BUFFER_SIZE - scratch_buffer.offset;
    size_t size = vsnprintf(scratch_buffer.data + scratch_buffer.offset, max_size, fmt, list);
    va_end(list);
    scratch_buffer.offset += size;
    return scratch_buffer.data;
}



void scratch_buffer_append_str(char* str){ 
    int len = strlen(str) + 1;
    if(len + scratch_buffer.offset > SCRATCH_BUFFER_SIZE){
        printf("Failed to allocate to scratch buffer\n");
        return;
    }

    memcpy(scratch_buffer.data + scratch_buffer.offset, str, len);
    scratch_buffer.offset += len;
}


char* scratch_buffer_vfmt(const char* fmt, va_list list){
    uint32_t max_size = SCRATCH_BUFFER_SIZE - scratch_buffer.offset;
    size_t size = vsnprintf(scratch_buffer.data + scratch_buffer.offset, max_size, fmt, list);
    scratch_buffer.offset += size;
    return scratch_buffer.data;
}


char* scratch_buffer_as_str(){ 
    if(scratch_buffer.offset == 0) return NULL;

    *((char*)scratch_buffer.data + scratch_buffer.offset) = '\0';
    return scratch_buffer.data;
}


