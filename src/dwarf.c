#include "dwarf.h"
#include "util.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


typedef struct{
    uint32_t line;
    uint32_t offset;
} LineInfo;


static ArrayList line_info_list = {0};


typedef struct {
    uint8_t* data;
    uint32_t size;
    uint32_t capacity;
} Buffer;

static void buffer_create(Buffer* buff, size_t bytes){
    buff->data = malloc(bytes); 
    if(buff->data == NULL) fatal_error("Failed to alloc mem\n");
    buff->capacity = bytes;
    buff->size = 0;
}

static void buffer_realloc(Buffer* buff){
    buff->capacity *= 2;
    buff->data = realloc(buff->data, buff->capacity);
    if(buff->data == NULL) fatal_error("Failed to alloc memory\n");
}

static void buffer_append_byte(Buffer* buff, uint8_t byte){
    if(buff->size + 1 >= buff->capacity)
        buffer_realloc(buff);
    
    buff->data[buff->size++] = byte;
}

static void buffer_append_word(Buffer* buff, uint16_t word){
    if(buff->size + 2 >= buff->capacity)
        buffer_realloc(buff);
    
    memcpy(buff->data + buff->size,&word, 2);
    buff->size += 2;
}

static void buffer_append_dword(Buffer* buff, uint32_t dword){
    if(buff->size + 4 >= buff->capacity)
        buffer_realloc(buff);
    
    memcpy(buff->data + buff->size,&dword, 4);
    buff->size += 4;
}

static void buffer_append_qword(Buffer* buff, uint64_t dword){
    if(buff->size + 8 >= buff->capacity)
        buffer_realloc(buff);
    
    memcpy(buff->data + buff->size,&dword, 8);
    buff->size += 8;
}

static void buffer_append_str(Buffer* buff, char* str){
    while(*str != 0){
        buffer_append_byte(buff, *str);
        str++;
    }
    buffer_append_byte(buff, 0);
}

void buffer_append_uleb128(Buffer* buff, uint64_t value){
    do {
        uint8_t byte = value & 0x7f;
        value >>= 7;

        if (value != 0)
            byte |= 0x80;

        buffer_append_byte(buff, byte);
    } while (value != 0);
}

void buffer_append_sleb128(Buffer* buff, int64_t value){
    int more = 1;

    while (more) {
        uint8_t byte = value & 0x7f;
        int negative = (value < 0);

        value >>= 7;

        if ((value == 0 && !(byte & 0x40)) ||
            (value == -1 && (byte & 0x40))) {
            more = 0;
        } else {
            byte |= 0x80;
        }
        buffer_append_byte(buff, byte);
    }
}


#define DWARF_MAX_OP_PER_INSTR 1 
#define DWARF_VERSION 5 
#define DWARF_OPCODE_BASE 13 

#define LINE_BASE -5
#define LINE_RANGE 14
#define MAX_LINE_INC (LINE_BASE + LINE_RANGE - 1)

#define DWARF_LNCT_PATH 0x1 
#define DWARF_FORM_STRING 0x8 

#define DW_LNS_copy         1
#define DW_LNS_advance_pc   2
#define DW_LNS_advance_line 3
#define DW_LNS_set_file     4

static void dwarf_emit_line_header(DwarfDebugInfo* debug_info, uint32_t text_size, const char* file_name){
    Buffer line_info_buffer = {0};
    buffer_create(&line_info_buffer, 512);

    buffer_append_dword(&line_info_buffer, 0); // length
    buffer_append_word(&line_info_buffer, DWARF_VERSION);
    buffer_append_byte(&line_info_buffer, 8); //address size
    buffer_append_byte(&line_info_buffer, 0); //segment size
    uint8_t* header_len_ptr = line_info_buffer.data + line_info_buffer.size;
    buffer_append_dword(&line_info_buffer, 0); //header len
    uint32_t buffer_size = line_info_buffer.size;
    buffer_append_byte(&line_info_buffer, 1); //min instruction len
    buffer_append_byte(&line_info_buffer, 1); //max op per instruction
    buffer_append_byte(&line_info_buffer, 1); //is_stmt
    buffer_append_byte(&line_info_buffer, LINE_BASE);
    buffer_append_byte(&line_info_buffer, LINE_RANGE);
    buffer_append_byte(&line_info_buffer, DWARF_OPCODE_BASE);

    const uint8_t std_opcode_lengths[DWARF_OPCODE_BASE - 1] = {
        0,1,1,1,1,0,0,0,1,0,0,1
    };

    for(int i = 0; i < DWARF_OPCODE_BASE - 1; i++){
        buffer_append_byte(&line_info_buffer, std_opcode_lengths[i]);
    }

    buffer_append_byte(&line_info_buffer, 0); //directory entry format cnt
    buffer_append_byte(&line_info_buffer, 0); //directory cnt

    buffer_append_byte(&line_info_buffer, 1); //file entry_fmt_cnt
    buffer_append_byte(&line_info_buffer, DWARF_LNCT_PATH);
    buffer_append_byte(&line_info_buffer, DWARF_FORM_STRING);
    buffer_append_byte(&line_info_buffer, 1); //file names cnt

    buffer_append_str(&line_info_buffer, (char*)file_name);

    uint32_t header_len = line_info_buffer.size - buffer_size;
    memcpy(header_len_ptr, &header_len, 4);

    buffer_append_byte(&line_info_buffer, DW_LNS_set_file);
    buffer_append_byte(&line_info_buffer, 0);

    //set address
    buffer_append_byte(&line_info_buffer, 0); // extended opcode
    buffer_append_byte(&line_info_buffer, 9); // size of opcode + param
    buffer_append_byte(&line_info_buffer, 2); // opcode
    debug_info->lines_pc_offset = line_info_buffer.size;
    buffer_append_qword(&line_info_buffer, 0); // address


    //TODO: Optimize the line program
    uint64_t line = 1;
    uint64_t last_offset = 0;
    if(line_info_list.size > 1){
        LineInfo i = array_list_get(line_info_list, LineInfo, 0);
        if(i.line != 1){
           buffer_append_byte(&line_info_buffer, DW_LNS_advance_line);
           line = i.line;
           buffer_append_sleb128(&line_info_buffer, i.line - 1);
        }
        if(i.offset != 0){
            buffer_append_byte(&line_info_buffer, DW_LNS_advance_pc);
            buffer_append_uleb128(&line_info_buffer, i.offset);
        }
        buffer_append_byte(&line_info_buffer, DW_LNS_copy);
    }



    for(int i = 1; i < line_info_list.size; i++){
        LineInfo li = array_list_get(line_info_list, LineInfo, i);
        uint64_t pc_inc = li.offset - last_offset;
        int64_t line_inc = li.line - line;
        last_offset = li.offset;
        line = li.line;

        uint16_t opcode = (line_inc - LINE_BASE) + (LINE_RANGE * pc_inc) + DWARF_OPCODE_BASE;
        if(opcode > 255 || line_inc > MAX_LINE_INC || line_inc < LINE_BASE){
            buffer_append_byte(&line_info_buffer, DW_LNS_advance_pc);
            buffer_append_uleb128(&line_info_buffer, pc_inc);

            buffer_append_byte(&line_info_buffer, DW_LNS_advance_line);
            buffer_append_sleb128(&line_info_buffer, line_inc);
            buffer_append_byte(&line_info_buffer, DW_LNS_copy);
        } else{
            buffer_append_byte(&line_info_buffer, opcode);
        }
    }

    //advance the pc one last time to finish the last instruction
    buffer_append_byte(&line_info_buffer, DW_LNS_advance_pc);
    buffer_append_uleb128(&line_info_buffer, text_size - last_offset);

    //end sequence
    buffer_append_byte(&line_info_buffer, 0);
    buffer_append_byte(&line_info_buffer, 1);
    buffer_append_byte(&line_info_buffer, 1); 

    uint32_t line_size = line_info_buffer.size - 4;
    memcpy(line_info_buffer.data, &line_size, 4);
    array_list_delete(line_info_list);
    debug_info->debug_lines_size = line_info_buffer.size;
    debug_info->debug_lines_data = line_info_buffer.data;
}


void dwarf_emit_debug_info(DwarfDebugInfo* debug_info, uint32_t text_size, const char* file_name){
    uint64_t abbreviation_code = 1;

    Buffer debug_info_data, debug_abbrev_data;
    buffer_create(&debug_abbrev_data, 16);
    buffer_create(&debug_info_data, 128);
    // debug info header
    // PDF Page 200
    buffer_append_dword(&debug_info_data, 0); //length
    buffer_append_word(&debug_info_data, DWARF_VERSION); //version
    buffer_append_byte(&debug_info_data, 0x1); // compile unit
    buffer_append_byte(&debug_info_data, 8); // addr_size 
    buffer_append_dword(&debug_info_data, 0); // debug abbrev offset 


    buffer_append_byte(&debug_info_data, abbreviation_code);
    buffer_append_dword(&debug_info_data, 0); // stmt_list
    buffer_append_str(&debug_info_data, "BASM 2026"); // producer
    buffer_append_str(&debug_info_data, (char*)file_name); // AT_NAME 
    buffer_append_word(&debug_info_data, 32769); //indicates assembly
    debug_info->info_low_pc_offset = debug_info_data.size;
    buffer_append_qword(&debug_info_data, 0); //low pc
    buffer_append_qword(&debug_info_data, 0); //high pc


    buffer_append_byte(&debug_info_data, 0); //terminate the tree
    buffer_append_byte(&debug_info_data, 0); //terminate the tree
    

    buffer_append_byte(&debug_abbrev_data, abbreviation_code); //abbreviation_code
    buffer_append_byte(&debug_abbrev_data, 0x11); //compile unit
    buffer_append_byte(&debug_abbrev_data, 0x0); // has children

    buffer_append_byte(&debug_abbrev_data, 0x10); // stmt_list
    buffer_append_byte(&debug_abbrev_data, 0x17); // sec_offset 
                                          //
    buffer_append_byte(&debug_abbrev_data, 0x25); // producer 
    buffer_append_byte(&debug_abbrev_data, 0x8); //  string
                                                 
    buffer_append_byte(&debug_abbrev_data, 0x03); // AT_NAME 
    buffer_append_byte(&debug_abbrev_data, 0x8); //  string

    buffer_append_byte(&debug_abbrev_data, 0x13); // AT_LANGUAGE
    buffer_append_byte(&debug_abbrev_data, 0x05); // Data2 

    
    buffer_append_byte(&debug_abbrev_data, 0x11); // low pc
    buffer_append_byte(&debug_abbrev_data, 0x01); //addr 

    buffer_append_byte(&debug_abbrev_data, 0x12); // high pc
    buffer_append_byte(&debug_abbrev_data, 0x01); //addr 


    // end the parent with zero for attribute & zero for form
    buffer_append_word(&debug_abbrev_data, 0); // terminate entry
    buffer_append_byte(&debug_abbrev_data, 0); // terminate abbrev section


    uint32_t length = debug_info_data.size - 4;
    memcpy(debug_info_data.data, &length, 4);

    debug_info->debug_info_size = debug_info_data.size;
    debug_info->debug_info_data = debug_info_data.data;

    debug_info->debug_abbrev_size = debug_abbrev_data.size;
    debug_info->debug_abbrev_data = debug_abbrev_data.data;
    dwarf_emit_line_header(debug_info, text_size, file_name);
}

void dwarf_add_line_info(uint32_t line_number, uint32_t section_offset){
    if(line_info_list.data == NULL){
        array_list_create_cap(line_info_list, LineInfo, 64);
    }
    LineInfo l;
    l.line = line_number;
    l.offset = section_offset;
    array_list_append(line_info_list, LineInfo, l);
}
