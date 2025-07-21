#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdnoreturn.h>


typedef struct{
   void* data;
   int size;
   int capacity;
} ArrayList;


#define array_list_create_cap(list, type, cap) \
do { \
    list.size = 0; \
    list.capacity = cap; \
    list.data = malloc(sizeof(type) * list.capacity); \
    memset(list.data, 0, sizeof(type) * list.capacity); \
} while(0) 


#define array_list_resize(list, type) \
do { \
    list.capacity *= 2; \
    list.data = realloc(list.data, sizeof(type) * list.capacity); \
} while(0) 

#define array_list_append(list, type, value) \
    do { \
        if(list.size == list.capacity){ \
            array_list_resize(list, type); \
        } \
        type* temp = (type*)list.data; \
        temp[list.size] = value; \
        list.size++; \
    } while(0)

#define array_list_get(list, type, index)((type*)list.data)[index]

#define array_list_delete(list) \
    do { \
        if(list.data != NULL) {  \
            if(list.capacity != 0) free(list.data); \
        } \
    } while(0)


void init_scratch_buffer();

void scratch_buffer_append_char(char c);

void scratch_buffer_clear();

uint32_t scratch_buffer_offset();

char* scratch_buffer_fmt(const char* fmt, ...);

char* scratch_buffer_vfmt(const char* fmt, va_list list);

void scratch_buffer_append_str(char* str);

void* scratch_buffer_get_data(uint32_t offset);

char* scratch_buffer_as_str();

#define FILE_BUFFER_CAPACITY 4096

typedef struct {
    const char* name;
    FILE* file;
    char* data;
    uint64_t size;
    uint64_t index;
} FileBuffer;


FileBuffer* file_buffer_create(const char* name);

void file_buffer_delete(FileBuffer* buff);

bool file_buffer_eof(FileBuffer* buff);

char file_buffer_get_char(FileBuffer* buff);

char file_buffer_peek_char(FileBuffer* buff);

char* file_get_line(FileBuffer* buff, int line);

int string_cmp_lower(const void* a, const void* b);

noreturn void fatal_error(const char* fmt, ...);

#if defined(_WIN64) 
   void PRINT_COLORED_ERROR(); 
#elif defined (__linux__)
    #define PRINT_COLORED_ERROR()  fprintf(stderr, "\e[31mError: \e[0m")
#else 
    #define PRINT_COLORED_ERROR()  fprintf(stderr, "Error: ")
#endif
