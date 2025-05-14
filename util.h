#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>


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
        if(list != NULL) {  \
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


#define SECTION_TEXT 1 
#define SECTION_DATA 2 
#define SECTION_BSS 3 

#define SECTION_UNDEFINED 255

#define VISIBILITY_LOCAL 0
#define VISIBILITY_GLOBAL 1

#define VISIBILITY_UNDEFINED 255

#define MAX_OFFSET 18446744073709551615ULL



//SYMBOLS ARE ONLY VALID IN THE TEXT SECTION FOR NOW 
typedef struct {
    uint64_t offset; 
    bool is_relative;
}SymbolInstance;


typedef struct {
    char* name;
    uint8_t section;
    uint8_t visibility;
    uint64_t section_offset;
    ArrayList instances; 
} SymbolTableEntry;


//TODO: IMPLEMENT AN ACTUAL HASHMAP 
typedef struct {
    ArrayList symbols;
} SymbolTable;


typedef struct {
    uint8_t* data;
    uint64_t capacity;
    uint64_t size;
} Section;


typedef struct {
    SymbolTable symTable; //holds all the locations of the symbols 
    Section data;
    Section text;
    Section bss;
} Program;





bool write_elf(const char* input_file, const char* output_file, Program *p);
