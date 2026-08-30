#pragma once
#include <stdint.h>

// .debug_info, .debug_abbrev, .debug_line
#define DWARF_SECTION_COUNT 3

// these 2 sections are needed to provide the starting pc and upper and lower pc
// rela.debug_info, rela.debug_line
#define DWARF_RELA_SECTION_COUNT 2

typedef struct {
    uint8_t debug_info_index;
    uint8_t debug_lines_index;
    uint8_t* debug_info_data;
    uint8_t* debug_abbrev_data;
    uint8_t* debug_lines_data;
    uint64_t debug_info_size;
    uint64_t debug_abbrev_size;
    uint64_t debug_lines_size;
    uint64_t rela_info_name_offset;
    uint64_t rela_line_name_offset;
    uint32_t info_low_pc_offset;
    uint32_t lines_pc_offset;
} DwarfDebugInfo;

#define DWARF_FREE_DEBUG_INFO(dinfo_struct) \
    do{ \
        free(dinfo_struct.debug_info_data); \
        free(dinfo_struct.debug_abbrev_data); \
        free(dinfo_struct.debug_lines_data); \
    } while(0)

#define DWARF_WRITE_DEBUG_INFO(dinfo_struct, output_stream) \
    do{ \
        fwrite(dinfo_struct.debug_info_data,dinfo_struct.debug_info_size,1,output_stream); \
        fwrite(dinfo_struct.debug_abbrev_data,dinfo_struct.debug_abbrev_size,1,output_stream); \
        fwrite(dinfo_struct.debug_lines_data,dinfo_struct.debug_lines_size,1,output_stream); \
    } while(0)



void dwarf_add_line_info(uint32_t line, uint32_t section_offset);

void dwarf_emit_debug_info(DwarfDebugInfo* debug_info, uint32_t text_size, const char* file_name);
