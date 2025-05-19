#include "util.h"
#include <stdbool.h>
#include <stddef.h>
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



FileBuffer* file_buffer_create(const char* name){
    FILE* file = fopen(name, "r");
    
    if(file == NULL){
        fprintf(stderr, "Failed to open file: %s\n", name);
        return NULL;
    }

    FileBuffer* fb = malloc(sizeof(FileBuffer) + FILE_BUFFER_CAPACITY);

    if(fb == NULL){
        fprintf(stderr, "Failed to allocate memory for file: %s\n", name);
        return NULL;
    }
    fb->name = name;
    fb->file = file;
    fb->size = 0;
    fb->index = 0;
    fb->data = (char*)(fb + sizeof(FileBuffer));

    return fb;
}



void file_buffer_delete(FileBuffer* buff){
    if(buff != NULL) free(buff);
}

bool file_buffer_eof(FileBuffer* buff){
    return buff->size < FILE_BUFFER_CAPACITY && buff->index >= buff->size;
}


char file_buffer_get_char(FileBuffer* buff){
    if(buff->size == 0){ 
        size_t size = fread(buff->data, 1, FILE_BUFFER_CAPACITY, buff->file);
        if(size == 0) return EOF;
        buff->size = size;
        buff->index = 0;
    }
    return buff->data[buff->index++];
}

char file_buffer_peek_char(FileBuffer* buff){
    if(buff->size == 0){ 
        size_t size = fread(buff->data, 1, FILE_BUFFER_CAPACITY, buff->file);
        if(size == 0) return EOF;
        buff->size = size;
        buff->index = 0;
    }

    else if(buff->index >= buff->size){
        fpos_t pos;
        fgetpos(buff->file, &pos);
        char c = fgetc(buff->file);
        fsetpos(buff->file, &pos);
        return c;
    }
    return buff->data[buff->index];
}


char* file_get_line(FileBuffer* buff, int line){
    long current_offset = ftell(buff->file);
    long offset = current_offset - buff->index; 
    
    rewind(buff->file);

    //TODO: CHECK for line lengths greater than FILE_BUFFER_CAPACITY
    int current_line = 1;
    while(current_line != line){
        fgets(buff->data, FILE_BUFFER_CAPACITY, buff->file);
        current_line++;
    }

    char* line_data = fgets(buff->data, FILE_BUFFER_CAPACITY, buff->file);

    for(int i = 0;;i++){
        if(line_data[i] == EOF || line_data[i] == '\n' || line_data[i] == ';'){
            line_data[i] = 0;
            break;
        }

    }    
    fseek(buff->file, offset, SEEK_SET); 
    buff->size = 0;
    buff->index = 0;
    return buff->data;
}
