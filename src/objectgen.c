#include "util.h"
#include "objectgen.h"
#include "dwarf.h"
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


typedef struct {
    uint8_t ident[16];
    uint16_t file_type;
    uint16_t machine_type;
    uint32_t version;
    uint64_t entry;
    uint64_t program_header_offset;
    uint64_t section_header_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_header_size;
    uint16_t program_header_entries;
    uint16_t section_header_size;
    uint16_t section_header_entries;
    uint16_t string_table_index;
} ElfHeader;



typedef struct {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addralign;
    uint64_t entsize;
} ElfSectionHeader;



typedef enum{
    ELF_SECTION_UNUSED = 0,
    ELF_SECTION_PINFO = 1,
    ELF_SECTION_LSYMTABLE = 2,
    ELF_SECTION_STRING_TABLE= 3,
    ELF_SECTION_RELAENTRY = 4,
    ELF_SECTION_NOBITS = 8,
} ElfSectionType;


typedef enum {
    ELF_SF_WRITE = 0x1,
    ELF_SF_ALLOC = 0x2,
    ELF_SF_EXECINSTR = 0x4,
    ELF_SF_MERGE = 0x10,
    ELF_SF_STRINGS = 0x20,
    ELF_SF_INFO_LINK = 0x40,
    ELF_SF_LINK_ORDER = 0x80,
} ElfSectionFlag;


typedef struct {
    uint32_t name;
    unsigned char info;
    unsigned char other;
    uint16_t section_index;
    uint64_t value;
    uint64_t size;
} ElfSymbolEntry;


typedef enum {
    SB_LOCAL = 0, 
    SB_GLOBAL = 0x10,
    SB_WEAK = 0x20,


    SB_OBJECT = 1, 
    SB_FUNCTION = 2,
    SB_SECTION = 3,
    SB_FILE = 4,
    SB_COMMON = 5,
} ElfSymbolBind;



typedef struct {
    uint64_t offset; //from the beginning of the section 
    uint64_t info;
    int64_t addend;
} ElfRelocatableEntry;


//I think these are the only 3 we will need 
typedef enum{
    RELOC_64 = 1,
    RELOC_PC32 = 2,
    RELOC_32 = 10, 
    RELOC_32S = 11, 
} ElfRelocationTypes;

typedef enum {
    SEC_NULL,
    SEC_TEXT = SECTION_TEXT,
    SEC_DATA = SECTION_DATA,
    SEC_BSS  = SECTION_BSS,
    SEC_DEBUG_INFO = 4,
    SEC_DEBUG_LINE = 5,
    SEC_DEBUG_ABBREV = 6,
    SEC_SHRSTRTAB = 7,
    SEC_SYMTAB = 8,
    SEC_STRTAB = 9,
    SEC_RELA_TEXT = 10,
    SEC_RELA_DEBUG_INFO = 11,
    SEC_RELA_DEBUG_LINE = 12,
    ELF_MAX_SECTIONS,
} ElfSectionId;


#define ELF_NON_USER_DEFINED_SYMBOL_CNT 8
#define ELF_SYMBOL_TBL_SIZE (sizeof(ElfSymbolEntry) * ELF_NON_USER_DEFINED_SYMBOL_CNT)
static inline bool section_goes_in_symtable(ElfSectionId id) {
    switch(id){
        case SEC_TEXT:
        case SEC_DATA:
        case SEC_BSS:
        case SEC_DEBUG_INFO:
        case SEC_DEBUG_LINE:
        case SEC_DEBUG_ABBREV:
            return true;
        default:
            return false;
    }
}

#define ELF_MAX_SECTION_HEADER_SIZE (sizeof(ElfSectionHeader) * ELF_MAX_SECTIONS)

// the current total is 128 so this should give enough room to grow
#define ELF_MAX_SECTION_STRTABLE_LEN 256

typedef struct{
    uint8_t  section_header_cnt;
    uint8_t  strtable_size;
    uint8_t symbol_count;
    uint8_t ids[ELF_MAX_SECTIONS];
    uint8_t  indices[ELF_MAX_SECTIONS];
    ElfSectionHeader* section_headers;
    ElfSymbolEntry* symbols;
    char*    sh_str_table;
} ElfCtx;


static void elf_add_section_header(ElfCtx* ctx,
        const char* name, ElfSectionId section_id, ElfSectionHeader sh)
{
    sh.name = ctx->strtable_size;
    if(section_goes_in_symtable(section_id)){
        ctx->symbols[ctx->symbol_count++] = (ElfSymbolEntry){
            .name = scratch_buffer_offset(),
            .info = SB_LOCAL | SB_SECTION,
            .other = 0,
            .section_index = ctx->section_header_cnt,
            .value = 0,
            .size  = 0,
        };
        scratch_buffer_append_str((char*)name);
    }
    while(*name != 0)
        ctx->sh_str_table[ctx->strtable_size++] = *name++;
    ctx->sh_str_table[ctx->strtable_size++] = '\0';

    ctx->section_headers[ctx->section_header_cnt] = sh;
    ctx->indices[section_id] = ctx->section_header_cnt;
    ctx->ids[ctx->section_header_cnt] = section_id;
    ctx->section_header_cnt++;
}



static int compare_visibility(const void *p1, const void *p2){
    const SymbolTableEntry* e1 = (p1);
    const SymbolTableEntry* e2 = (p2);    
    return e1->visibility - e2->visibility;
}



#define MACHINE_X86_64 62

static uint32_t get_reloc_count(Program* p){
    uint32_t count = 0;
    //only allow text section relocations for now
    for(int i = 0; i < p->symTable.symbols.size; i++){
        SymbolTableEntry e = array_list_get(p->symTable.symbols, SymbolTableEntry, i);
        for(int j = 0; j < e.instances.size; j++){
            SymbolInstance instance = array_list_get(e.instances, SymbolInstance, j);
            //skip over the relative instances in the text section
            if(!instance.is_relative || (e.section != SECTION_TEXT && instance.is_relative)){
                count++;
            }
        }
    }
    return count;
}


static void elf_write_rela_text(ElfCtx* ctx, Program* p, FILE* output_stream){
    for(int i = 0; i < p->symTable.symbols.size; i++){
        SymbolTableEntry e = array_list_get(p->symTable.symbols, SymbolTableEntry, i);
        for(int j = 0; j < e.instances.size; j++){
            SymbolInstance instance = array_list_get(e.instances, SymbolInstance, j);
            //relative addresses in the text section can be resolved by the assembler
            if(instance.is_relative && e.section == SECTION_TEXT) continue;

            if(instance.is_relative){
                ElfRelocatableEntry reloc_e = {
                    .offset = instance.offset,
                    .addend = instance.addend,
                    .info = ((uint64_t)(ctx->symbol_count + i) << 32)| RELOC_PC32,
                };
                fwrite(&reloc_e, sizeof(reloc_e), 1, output_stream);
            } else{
                // index into symbol table
                // +1 accounts for file name
                uint64_t section = ctx->indices[e.section] + 1;
                ElfRelocatableEntry reloc_e = {
                    .offset = instance.offset,
                    .addend = e.section_offset + instance.addend,
                    .info = ((uint64_t)(section) << 32)| RELOC_32S,
                };
                fwrite(&reloc_e, sizeof(reloc_e), 1, output_stream);
            }
        }
    }
}


bool write_elf(const char* input_file, const char* output_file, Program* p){
    FILE* output_stream = fopen(output_file, "wb");

    if(output_stream == NULL){
        fatal_error("Failed to create file %s\n", input_file);
        return false;
    }

    scratch_buffer_clear();
    ElfCtx ctx;
    memset(&ctx.indices, 0, ELF_MAX_SECTIONS);
    ctx.section_header_cnt = 1; // null header
    ctx.strtable_size = 1;
    ctx.symbol_count  = 1; //nul symbol
    uint8_t* ctx_data = malloc(ELF_MAX_SECTION_HEADER_SIZE + ELF_MAX_SECTION_STRTABLE_LEN
            + ELF_SYMBOL_TBL_SIZE);

    if(ctx_data == NULL){
        fatal_error("Failed alloc memory\n");
        return false;
    }
    ctx.section_headers = (ElfSectionHeader*)ctx_data;
    ctx_data += ELF_MAX_SECTION_HEADER_SIZE;
    ctx.symbols = (ElfSymbolEntry*)ctx_data;
    ctx_data += ELF_SYMBOL_TBL_SIZE;
    ctx.sh_str_table = (char*)(ctx_data);

    // add null section header
    memset(ctx.section_headers, 0, sizeof(ElfSectionHeader));
    ctx.sh_str_table[0] = '\0';

    //add null symbol table entry
    memset(ctx.symbols, 0, sizeof(ElfSymbolEntry));
    scratch_buffer_append_char('\0');


    //add the file name symbol
    scratch_buffer_append_str((char*)input_file);
    ctx.symbols[ctx.symbol_count++] = (ElfSymbolEntry){
        .name = 1,
        .info = SB_LOCAL | SB_FILE,
        .other = 0,
        .section_index = 0xfff1,
        .value = 0,
        .size  = 0,
    };

    elf_add_section_header(&ctx, ".text", SEC_TEXT, (ElfSectionHeader){
        .name   = 0,
        .type   = ELF_SECTION_PINFO,
        .flags  = ELF_SF_ALLOC | ELF_SF_EXECINSTR,
        .addr   = 0,
        .offset = 0,
        .size   = p->text.size,
        .link   = 0,
        .info   = 0,
        .addralign = 16,
        .entsize   = 0,
    });

    if(p->data.size > 0) {
        elf_add_section_header(&ctx, ".data", SEC_DATA, (ElfSectionHeader){
            .name   = 0,
            .type   = ELF_SECTION_PINFO,
            .flags  = ELF_SF_ALLOC | ELF_SF_WRITE,
            .addr   = 0,
            .offset = 0,
            .size   = p->data.size,
            .link   = 0,
            .info   = 0,
            .addralign = 4,
            .entsize   = 0,
        });
    }

    if(p->bss.size > 0) {
        elf_add_section_header(&ctx, ".bss", SEC_BSS, (ElfSectionHeader){
            .name   = 0,
            .type   = ELF_SECTION_NOBITS,
            .flags  = ELF_SF_ALLOC | ELF_SF_WRITE,
            .addr   = 0,
            .offset = 0,
            .size   = p->bss.size,
            .link   = 0,
            .info   = 0,
            .addralign = 4,
            .entsize   = 0,
        });
    }

    DwarfDebugInfo all_debug_info;
    if(p->flags.debugSymbols){
        dwarf_emit_debug_info(&all_debug_info, p->text.size, input_file);

        elf_add_section_header(&ctx, ".debug_info", SEC_DEBUG_INFO, (ElfSectionHeader){
            .name   = 0,
            .type   = ELF_SECTION_PINFO,
            .flags  = 0,
            .addr   = 0,
            .offset = 0,
            .size   = all_debug_info.debug_info_size,
            .link   = 0,
            .info   = 0,
            .addralign = 1,
            .entsize   = 0,
        });

        elf_add_section_header(&ctx, ".debug_abbrev", SEC_DEBUG_ABBREV,
                (ElfSectionHeader){
            .name   = 0,
            .type   = ELF_SECTION_PINFO,
            .flags  = 0,
            .addr   = 0,
            .offset = 0,
            .size   = all_debug_info.debug_abbrev_size,
            .link   = 0,
            .info   = 0,
            .addralign = 1,
            .entsize   = 0,
        });

        elf_add_section_header(&ctx, ".debug_line", SEC_DEBUG_LINE, (ElfSectionHeader){
            .name   = 0,
            .type   = ELF_SECTION_PINFO,
            .flags  = 0,
            .addr   = 0,
            .offset = 0,
            .size   = all_debug_info.debug_lines_size,
            .link   = 0,
            .info   = 0,
            .addralign = 1,
            .entsize   = 0,
        });

        elf_add_section_header(&ctx, ".rela.debug_info",
                SEC_RELA_DEBUG_INFO, (ElfSectionHeader){
            .name   = 0,
            .type   = ELF_SECTION_RELAENTRY,
            .flags  = 0,
            .addr   = 0,
            .offset = 0,
            .size   = 2 * sizeof(ElfRelocatableEntry),
            .link   = SEC_SYMTAB,      // going to need to point to symbol table
            .info   = SEC_DEBUG_INFO, // needs to point to debug info
            .addralign = 8,
            .entsize   = sizeof(ElfRelocatableEntry),
        });

        elf_add_section_header(&ctx, ".rela.debug_line",
                SEC_RELA_DEBUG_LINE, (ElfSectionHeader){
            .name   = 0,
            .type   = ELF_SECTION_RELAENTRY,
            .flags  = 0,
            .addr   = 0,
            .offset = 0,
            .size   = sizeof(ElfRelocatableEntry),
            .link   = SEC_SYMTAB,      // going to need to point to symbol table
            .info   = SEC_DEBUG_LINE, // needs to point to debug line
            .addralign = 8,
            .entsize   = sizeof(ElfRelocatableEntry),
        });
    }

    uint32_t num_text_reloca_entries = get_reloc_count(p);
    if(num_text_reloca_entries > 0){
        elf_add_section_header(&ctx, ".rela.text", SEC_RELA_TEXT, (ElfSectionHeader){
            .name   = 0,
            .type   = ELF_SECTION_RELAENTRY,
            .flags  = 0,
            .addr   = 0,
            .offset = 0,
            .size   = sizeof(ElfRelocatableEntry) * num_text_reloca_entries,
            .link   = SEC_SYMTAB, // going to need to point to symbol table
            .info   = SEC_TEXT,  // needs to point to text section
            .addralign = 8,
            .entsize   = sizeof(ElfRelocatableEntry),
        });
    }

    elf_add_section_header(&ctx, ".symtab", SEC_SYMTAB, (ElfSectionHeader){
            .name   = 0,
            .type   = ELF_SECTION_LSYMTABLE,
            .flags  = 0,
            .addr   = 0,
            .offset = 0,
            .size   = 0, // need to backfill the size
            .link   = SEC_STRTAB, // going to need to point to symbol string table
            .info   = 0, // one plus index of last local symbol
            .addralign = 8,
            .entsize   = sizeof(ElfSymbolEntry),
    });

    elf_add_section_header(&ctx, ".strtab", SEC_STRTAB, (ElfSectionHeader){
            .name   = 0,
            .type   = ELF_SECTION_STRING_TABLE,
            .flags  = 0,
            .addr   = 0,
            .offset = 0,
            .size   = 0, // need to backfill the size
            .link   = 0,
            .info   = 0,
            .addralign = 1,
            .entsize   = 0,
    });

    elf_add_section_header(&ctx, ".shrstrtab", SEC_SHRSTRTAB, (ElfSectionHeader){
            .name   = 0,
            .type   = ELF_SECTION_STRING_TABLE,
            .flags  = 0,
            .addr   = 0,
            .offset = 0,
            .size   = 0, // need to backfill the size
            .link   = 0,
            .info   = 0,
            .addralign = 1,
            .entsize   = 0,
    });

    ElfHeader head = {0};
    head.ident[0] = 0x7f;
    head.ident[1] = 'E';
    head.ident[2] = 'L';
    head.ident[3] = 'F';
    head.ident[4] = 2;//64 bit objects
    head.ident[5] = 1; //endianness
    head.ident[6] = 1;  // File Version
    head.ident[7] = 0; //System V OS ABI
    head.ident[8] = 0; //ABI Version


    head.flags = 0;
    head.file_type = 1; //RELOCATABLE FILE 
    head.machine_type = MACHINE_X86_64;
    head.version = 1;
    head.header_size = sizeof(ElfHeader);
    head.section_header_offset = head.header_size;
    head.section_header_size = sizeof(ElfSectionHeader);

    head.section_header_entries = ctx.section_header_cnt;
    head.string_table_index = ctx.section_header_cnt - 1;
    fwrite(&head, sizeof(ElfHeader),1, output_stream);

    //fix all link and info conflicts
    for(int i = 1; i < ctx.section_header_cnt; i++){
        ElfSectionHeader* temp = &ctx.section_headers[i];
        temp->link = ctx.indices[temp->link];
        temp->info = ctx.indices[temp->info];
    }

    ctx.section_headers[ctx.indices[SEC_SHRSTRTAB]].size = ctx.strtable_size;
    // symbol table info in the section header is
    // 1 greater than the last local symbol
    int sym_table_info = ctx.symbol_count;
    if(p->symTable.symbols.data != NULL){
        // the global symbols must come after local symbols
        qsort(p->symTable.symbols.data, p->symTable.symbols.size,
                sizeof(SymbolTableEntry),compare_visibility);
        //get the index of the last local var
        for(int i = p->symTable.symbols.size - 1; i >= 0; i--){
            SymbolTableEntry e = array_list_get(p->symTable.symbols, SymbolTableEntry, i);
            if(e.visibility == VISIBILITY_LOCAL){
                sym_table_info += i + 1;
                break;
            }
        }
    }
    ctx.section_headers[ctx.indices[SEC_SYMTAB]].info = sym_table_info;

    // get size of symbol string table and size of symbol table
    uint64_t symbol_str_table_size = scratch_buffer_offset();
    uint64_t symbol_table_size = ctx.symbol_count * sizeof(ElfSymbolEntry);
    for(int i = 0; i < p->symTable.symbols.size; i++){
        SymbolTableEntry e = array_list_get(p->symTable.symbols, SymbolTableEntry, i);
        symbol_str_table_size += strlen(e.name) + 1;
        symbol_table_size += sizeof(ElfSymbolEntry);
    }
    ctx.section_headers[ctx.indices[SEC_SYMTAB]].size = symbol_table_size;
    ctx.section_headers[ctx.indices[SEC_STRTAB]].size = symbol_str_table_size;

    // now we must resolve offsets
    uint64_t offset = head.section_header_size 
        * head.section_header_entries + head.header_size;
    uint64_t curr_offset = offset;
    //write null header
    fwrite(&ctx.section_headers[0], sizeof(ElfSectionHeader), 1, output_stream);

    for(int i = 1; i < ctx.section_header_cnt; i++){
        ElfSectionHeader* sh = &ctx.section_headers[i];
        if(offset % sh->addralign != 0)
            offset += (sh->addralign - (offset % sh->addralign)) % sh->addralign;
        sh->offset = offset;
        fwrite(sh, sizeof(ElfSectionHeader), 1, output_stream);

        if(sh->type != ELF_SECTION_NOBITS)
            offset += sh->size;
    }

    //write all the data
    for(int i = 1; i < ctx.section_header_cnt; i++){
        ElfSectionHeader* sh = &ctx.section_headers[i];
        while(curr_offset % sh->addralign != 0){
            fputc(0, output_stream);
            curr_offset++;
        }
        switch (ctx.ids[i]) {
            case SEC_NULL:
            case SEC_BSS:
                break;
            case SEC_TEXT:
                fwrite(p->text.data, 1, p->text.size, output_stream);
                break;
            case SEC_DATA:
                fwrite(p->data.data, 1, p->data.size, output_stream);
                break;
            case SEC_DEBUG_INFO:
                fwrite(all_debug_info.debug_info_data, 1,
                        all_debug_info.debug_info_size, output_stream);
                free(all_debug_info.debug_info_data);
                break;
            case SEC_DEBUG_LINE:
                fwrite(all_debug_info.debug_lines_data, 1,
                        all_debug_info.debug_lines_size, output_stream);
                free(all_debug_info.debug_lines_data);
                break;
            case SEC_DEBUG_ABBREV:
                fwrite(all_debug_info.debug_abbrev_data, 1,
                        all_debug_info.debug_abbrev_size, output_stream);
                free(all_debug_info.debug_abbrev_data);
                break;
            case SEC_SHRSTRTAB:
                fwrite(ctx.sh_str_table,1, ctx.strtable_size, output_stream);
                break;
            case SEC_SYMTAB:{
                //write the symbols for the file name and section headers first
                fwrite(ctx.symbols, sizeof(ElfSymbolEntry), ctx.symbol_count, output_stream);
                uint64_t name_offset = scratch_buffer_offset();
                for(int i = 0; i < p->symTable.symbols.size; i++){
                    SymbolTableEntry* e =
                        &array_list_get(p->symTable.symbols, SymbolTableEntry, i);
                    ElfSymbolEntry symbol = {
                        .name = name_offset,
                        .info = (e->visibility == VISIBILITY_GLOBAL) ? SB_GLOBAL : SB_LOCAL,
                        .other = 0,
                        .section_index = ctx.indices[e->section],
                        .value = e->section_offset,
                        .size = 0,
                    };
                    fwrite(&symbol, sizeof(symbol), 1,output_stream);
                    name_offset += strlen(e->name) + 1;
                }
                break;
            }
            case SEC_STRTAB:
                //write the symbols names for the file name and section headers first
                fwrite(scratch_buffer_get_data(0),1,scratch_buffer_offset(), output_stream);
                //write the names of the user defined symbols
                for(int i = 0; i < p->symTable.symbols.size; i++){
                    SymbolTableEntry e =
                        array_list_get(p->symTable.symbols, SymbolTableEntry, i);
                    while(*e.name != 0)
                        fputc(*e.name++, output_stream);
                    fputc(0, output_stream);
                }
                break;
            case SEC_RELA_TEXT:
                elf_write_rela_text(&ctx, p, output_stream);
                break;
            case SEC_RELA_DEBUG_INFO: {
                ElfRelocatableEntry low_pc = {
                    .info   = ((uint64_t)(ctx.indices[SEC_TEXT] + 1) << 32) | RELOC_64,
                    .offset = all_debug_info.info_low_pc_offset,
                    .addend = 0,
                };
                fwrite(&low_pc, sizeof(ElfRelocatableEntry), 1, output_stream);
                
                //write the high pc
                // high pc will come right after
                low_pc.offset += 8;
                low_pc.addend = p->text.size;
                fwrite(&low_pc, sizeof(ElfRelocatableEntry), 1, output_stream);
                break;
            }
            case SEC_RELA_DEBUG_LINE: {
                ElfRelocatableEntry low_pc = {
                    .info   = ((uint64_t)(ctx.indices[SEC_TEXT] + 1) << 32) | RELOC_64,
                    .offset = all_debug_info.lines_pc_offset,
                    .addend = 0,
                };
                fwrite(&low_pc, sizeof(ElfRelocatableEntry), 1, output_stream);
                break;
            }
        }
        if(sh->type != ELF_SECTION_NOBITS)
            curr_offset += sh->size;
    }
    fclose(output_stream);
    free(ctx.section_headers);
    return true;
}

#define PE_X86_64 0x8664

typedef struct {
    uint16_t machine_type;
    uint16_t section_count;
    uint32_t date;
    uint32_t symbol_table_offset;
    uint32_t symbol_count;
    uint16_t opt_header_size;
    uint16_t flags;
} PEHeader;



typedef struct{
    char name[8];
    uint32_t virtual_size;
    uint32_t virtual_addr;
    uint32_t size;
    uint32_t offset;
    uint32_t reloc_offset;
    uint32_t line_num_offset;
    uint16_t reloc_count;
    uint16_t line_num_count;
    uint32_t flags;
} PESectionHeader;

typedef enum {
    PE_SF_INITIALIZED = 0x00000040,
    PE_SF_UNINITIALIZED = 0x00000080,
    PE_SF_EXEC= 0x00000020,
    PE_SF_ALIGN_1 = 0x00100000,
    PE_SF_EXEC_CODE = 0x20000000,
    PE_SF_READ = 0x40000000,
    PE_SF_WRITE = 0x80000000,
} PESectionFlags;


typedef struct {
    uint32_t virtual_addr; //section offset
    uint32_t symbol_table_index;
    uint16_t type;
} __attribute__((packed)) PERelocatableEntry;


typedef enum {
    PE_RELOC_AMD64_ADDR64 = 0x1,
    PE_RELOC_AMD64_ADDR32 = 0x2,
    PE_RELOC_AMD64_REL32 = 0x4,
} PERelocationTypes;



typedef struct {
    union{
        char name[8]; 
        uint32_t offset[2];
    };
    uint32_t value;
    int16_t section;
    uint16_t type;
    uint8_t storage_class;
    uint8_t aux_symbol_count;
} __attribute__((packed)) PESymbolTableEntry;



typedef struct{
    uint32_t size;
    uint16_t reloc_count;
    uint16_t line_num_count;
    uint32_t checksum;
    uint16_t number;
    uint8_t selection;
    uint8_t unused[3];
} __attribute__((packed)) PEAuxiliarySection;




typedef enum {
    PE_SC_EXTERNAL = 2,
    PE_SC_STATIC = 3,
    PE_SC_FILE = 103,
} PEStorageClass;




static void write_pe_symbol_section(FILE* output_stream, const char* name, int index, uint64_t size, uint32_t reloc_count){
    PESymbolTableEntry temp_entry = {0};
    strcpy(temp_entry.name, name);
    temp_entry.section = index;
    temp_entry.aux_symbol_count = 1;
    temp_entry.storage_class = PE_SC_STATIC;
    fwrite(&temp_entry, sizeof(temp_entry), 1, output_stream);

    PEAuxiliarySection aux = {0};
    aux.reloc_count = reloc_count;
    aux.size = size; 
    fwrite(&aux,sizeof(aux), 1, output_stream);
}



bool write_pe(const char* input_file, const char* output_file, Program* p){
    FILE* output_stream = fopen(output_file, "wb");

    if(output_stream == NULL){
        fatal_error("Failed to create file %s\n", input_file);
        return false;
    }


    uint32_t sym_offset = 0;

    PEHeader head = {0};
    head.machine_type = PE_X86_64;
    head.date = (uint32_t)time(NULL);
    head.section_count = 1;

    if(p->bss.size != 0) head.section_count++;
    if(p->data.size != 0){
        sym_offset += p->data.size;
        head.section_count++;
    } 
    //At least 2 for the file name + One for .Absolut? 
    //Each section has 2
    head.symbol_count = 3 + head.section_count * 2 + p->symTable.symbols.size;


    uint32_t sym_table_text_offset = 2;

    PESectionHeader text_section = {0};

    strcpy(text_section.name, ".text");
    text_section.size = p->text.size; 
    text_section.offset = head.section_count * sizeof(PESectionHeader) + sizeof(PEHeader);
    text_section.reloc_offset = text_section.offset + p->text.size;
    text_section.reloc_count = get_reloc_count(p);
    text_section.flags = PE_SF_ALIGN_1 | PE_SF_READ | PE_SF_EXEC | PE_SF_EXEC_CODE;

    sym_offset += text_section.reloc_offset + text_section.reloc_count * sizeof(PERelocatableEntry);
    head.symbol_table_offset = sym_offset;

    fwrite(&head, sizeof(PEHeader),1, output_stream);
    fwrite(&text_section, sizeof(PESectionHeader), 1, output_stream);


    if(p->data.size != 0){
        PESectionHeader data_section = {0};
        strcpy(data_section.name, ".data");
        data_section.size = p->data.size; 
        data_section.offset = text_section.reloc_offset + text_section.reloc_count * sizeof(PERelocatableEntry);
        data_section.reloc_offset = data_section.offset + p->data.size;
        data_section.reloc_count = 0; //going to be zero for now
        data_section.flags = PE_SF_ALIGN_1 | PE_SF_READ | PE_SF_INITIALIZED | PE_SF_WRITE;
        fwrite(&data_section, sizeof(data_section), 1, output_stream);
    }

    if(p->bss.size != 0){
        PESectionHeader bss_section = {0};
        strcpy(bss_section.name, ".bss");
        bss_section.size = p->bss.size; 
        bss_section.offset = 0;
        bss_section.reloc_offset = 0;
        bss_section.reloc_count = 0; //going to be zero for now
        bss_section.flags = PE_SF_ALIGN_1 | PE_SF_READ | PE_SF_UNINITIALIZED| PE_SF_WRITE;
        fwrite(&bss_section, sizeof(bss_section), 1, output_stream);

    }

    //write the data
    fwrite(p->text.data, 1, p->text.size, output_stream);
    if(text_section.reloc_count != 0){
        for(int i = 0; i < p->symTable.symbols.size; i++){
            SymbolTableEntry e = array_list_get(p->symTable.symbols, SymbolTableEntry, i);
            for(int j = 0; j < e.instances.size; j++){
                SymbolInstance instance = array_list_get(e.instances, SymbolInstance, j);

                //relative addresses in the text section can be resolved by the assembler
                if(instance.is_relative && e.section == SECTION_TEXT) continue;

                PERelocatableEntry reloc_e = {0};
                reloc_e.virtual_addr = instance.offset;

                
                if(instance.is_relative){
                    //get the index of this symbol in the symbol table
                    reloc_e.symbol_table_index = sym_table_text_offset + head.section_count * 2 + 1 + i;
                    reloc_e.type = PE_RELOC_AMD64_REL32; 
                } else{
                    //each section in the symbol table has an auxiliary section 
                    //thats why we multiply by 2
                    reloc_e.symbol_table_index = sym_table_text_offset + (e.section - 1) * 2;
                    reloc_e.type = PE_RELOC_AMD64_ADDR32;
                }
                fwrite(&reloc_e, sizeof(reloc_e), 1, output_stream);
            }
        }
    }

    if(p->data.size != 0) fwrite(p->data.data, 1, p->data.size, output_stream);


    PESymbolTableEntry temp_entry = {0};
    strcpy(temp_entry.name, ".file");
    temp_entry.section = -2;
    temp_entry.aux_symbol_count = 1;
    temp_entry.storage_class = PE_SC_FILE;
    fwrite(&temp_entry, sizeof(temp_entry),1, output_stream);

    char file_name[18] = {0};
    //nasm just seems to just truncate file name if it is larger than 18 chars
    //so thats what we are going to do
    strncpy(file_name,input_file, 18);
    fwrite(file_name, 1, 18, output_stream);

    write_pe_symbol_section(output_stream, ".text", 1, p->text.size, text_section.reloc_count);

    int index = 2;
    if(p->data.size != 0){
        write_pe_symbol_section(output_stream, ".data", 2, p->data.size, 0);
        index++;
    }
    if(p->bss.size != 0){
        write_pe_symbol_section(output_stream, ".bss", index, p->bss.size, 0);
    }

    memset(&temp_entry, 0,sizeof(temp_entry));

    //write Absolut entry (I don't know what its for)
    memcpy(temp_entry.name, ".absolut", 8);
    temp_entry.section = -1;
    temp_entry.storage_class = PE_SC_STATIC;
    fwrite(&temp_entry, sizeof(temp_entry), 1, output_stream);


    scratch_buffer_clear();
    for(int i = 0; i < p->symTable.symbols.size; i++){
        PESymbolTableEntry pe_entry = {0};
        SymbolTableEntry e = array_list_get(p->symTable.symbols, SymbolTableEntry, i);
        if(strlen(e.name) < 8){
            strcpy(pe_entry.name, e.name);
        } else{
            pe_entry.offset[0] = 0;
            pe_entry.offset[1] = scratch_buffer_offset() + 4;
            scratch_buffer_append_str(e.name);
        }
        pe_entry.value = e.section_offset; 
        pe_entry.section = e.section;

        //if there is no data section bss section will be at SECTION_BSS - 1
        if(e.section == SECTION_BSS && p->data.size == 0){
            pe_entry.section = 2;    
        } 
        pe_entry.storage_class = (e.visibility == VISIBILITY_GLOBAL) ? PE_SC_EXTERNAL : PE_SC_STATIC; 
        fwrite(&pe_entry, sizeof(pe_entry), 1,output_stream);
    }

    //write the string table
    uint32_t string_table_size = scratch_buffer_offset() + 4;
    fwrite(&string_table_size, 4, 1, output_stream);

    if(string_table_size != 4){
        fwrite(scratch_buffer_get_data(0), 1,string_table_size - 4, output_stream);
    }


    fclose(output_stream);

    return true; 
}


typedef struct {
     uint32_t magic; 
     uint32_t cpu_type;
     uint32_t cpu_sub_type;
     uint32_t file_type;
     uint32_t num_cmds;
     uint32_t cmd_size;
     uint32_t flags;
     uint32_t reserved;
} MachoHeader;



typedef struct{
    uint32_t type;
    uint32_t size; 
    char name[16];
    uint64_t addr;
    uint64_t seg_size;
    uint64_t offset;
    uint64_t file_size;
    uint32_t max_virtual_mem_protections;
    uint32_t initial_virtual_mem_protections;
    uint32_t num_sections;
    uint32_t flags;
}MachoSegmentCmd;


typedef struct{
    char section_name[16];
    char segment_name[16];
    uint64_t addr;
    uint64_t size;
    uint32_t offset;
    uint32_t align;
    uint32_t reloc_offset;
    uint32_t reloc_count;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
}MachoSection;


typedef enum {
    MACHO_FLAG_ZERO = 0x1,
    MACHO_FLAG_LOCAL_RELOC = 0x100,
    MACHO_FLAG_EXTERNAL_RELOC = 0x200,
    MACHO_FLAG_SOME_INSTRUCTIONS = 0x400,
    MACHO_FLAG_PURE_INSTRUCTIONS = 1 << 31,
} MachoSectionFlag;


typedef struct {
  uint32_t cmd;
  uint32_t cmdsize;
  uint32_t offset;
  uint32_t symbol_count;
  uint32_t string_table_offset;
  uint32_t string_table_size;
} MachoSymbolCmd;


typedef struct {
    uint32_t index;
    uint8_t type;
    uint8_t section;
    uint16_t desc;
    uint64_t value;
} MachoSymbolTableEntry;

typedef struct {
    int32_t addr;
    uint32_t sym_num: 24,
             is_pc_rel: 1,
             length: 2,
             external: 1,
             type: 4;
} MachoRelocationInfo;

bool write_macho(const char* input_file, const char* output_file, Program* p){
    FILE* output_stream = fopen(output_file, "wb");

    if(output_stream == NULL){
        fatal_error("Failed to create file %s\n", input_file);
        return false;
    }


    MachoHeader header = {0};
    header.magic = 0xfeedfacf;
    header.cpu_type = 0x1000007;
    header.cpu_sub_type = 0x03; //all x86 processors
    header.file_type = 0x01; // obj file
    header.num_cmds = 1;


    int symbol_count = p->symTable.symbols.size;
    if(symbol_count > 0) header.num_cmds++;

    int section_count = 1;

    if(p->bss.size != 0) section_count++;
    if(p->data.size != 0) section_count++;

    header.cmd_size = section_count * sizeof(MachoSection) + sizeof(MachoSegmentCmd);
    header.cmd_size += sizeof(MachoSymbolCmd); 

    fwrite(&header, sizeof(MachoHeader), 1, output_stream);

    uint64_t offset = sizeof(MachoHeader) + header.cmd_size;


    MachoSegmentCmd seg_cmd = {0};
    seg_cmd.type = 0x19; //64 bit segment load cmd
    seg_cmd.size = sizeof(MachoSegmentCmd) + sizeof(MachoSection) * section_count; 

    seg_cmd.offset = offset;
    seg_cmd.seg_size = p->text.size + p->bss.size + p->data.size; 

    //this is what nasm uses not sure what it does
    seg_cmd.max_virtual_mem_protections = 0x07;
    seg_cmd.initial_virtual_mem_protections = 0x07;

    seg_cmd.file_size = p->data.size + p->text.size; 
    seg_cmd.num_sections = section_count;

    fwrite(&seg_cmd, sizeof(MachoSegmentCmd), 1, output_stream);


    MachoSection text = {0};
    strcpy(text.section_name, "__text");
    strcpy(text.segment_name, "__TEXT");
    text.addr = 0;
    text.size = p->text.size;
    text.offset = offset; 

    offset += text.size;
    text.reloc_offset = offset + p->data.size;

    int padding = 0;
    while(text.reloc_offset % 8 != 0){
        padding++;
        text.reloc_offset++;
    }

    text.reloc_count = get_reloc_count(p);
    text.flags = MACHO_FLAG_LOCAL_RELOC | MACHO_FLAG_EXTERNAL_RELOC | MACHO_FLAG_SOME_INSTRUCTIONS | MACHO_FLAG_PURE_INSTRUCTIONS; 

    fwrite(&text, sizeof(MachoSection), 1, output_stream);

    int data_addr = 0;
    int bss_addr= 0;

    if(p->data.size != 0){
        MachoSection data = {0};
        strcpy(data.section_name, "__data");
        strcpy(data.segment_name, "__DATA");
        data.addr = p->text.size;
        data.size = p->data.size;
        data.offset = offset; 
        data_addr = data.addr;
        offset += data.size;
        fwrite(&data, sizeof(MachoSection), 1, output_stream);
    }

    if(p->bss.size != 0){
        MachoSection bss = {0};
        strcpy(bss.section_name, "__bss");
        strcpy(bss.segment_name, "__BSS");
        bss.addr = p->text.size + p->data.size;
        bss.size = p->bss.size;
        bss_addr = bss.addr;
        bss.flags = MACHO_FLAG_ZERO;
        fwrite(&bss, sizeof(MachoSection), 1, output_stream);
    }

    offset += padding;


    ArrayList reloc_info ={0};
    ArrayList sym_table_entries = {0};

    if(symbol_count != 0){
        scratch_buffer_clear();
        array_list_create_cap(reloc_info, MachoRelocationInfo, 16);
        array_list_create_cap(sym_table_entries, MachoSymbolTableEntry, 8);

        int symbol_index = 0;
        for(int i = 0; i < p->symTable.symbols.size; i++){
            SymbolTableEntry e = array_list_get(p->symTable.symbols, SymbolTableEntry, i);

            //add symbol to symbol table
            MachoSymbolTableEntry sym_entry = {0};
            sym_entry.index = scratch_buffer_offset() + 1;
            if(e.section == SECTION_EXTERN || e.visibility == VISIBILITY_GLOBAL){
                sym_entry.type |= 1;
            }

            if(e.section != SECTION_EXTERN){
                sym_entry.type |= 0xe;
                sym_entry.section = e.section;
                if(e.section == SECTION_BSS && section_count == 2){
                    sym_entry.section--;
                }
                if(e.section == SECTION_TEXT){
                    sym_entry.value = text.addr;
                } else if (e.section == SECTION_DATA) { 
                    sym_entry.value = data_addr;
                } else{
                    sym_entry.value = bss_addr;
                }
                sym_entry.value += e.section_offset;
            }
            
            array_list_append(sym_table_entries, MachoSymbolTableEntry, sym_entry);
            scratch_buffer_append_str(e.name);

            for(int j = 0; j < e.instances.size; j++){
                SymbolInstance instance = array_list_get(e.instances, SymbolInstance, j);
                //relative addresses in the text section can be resolved by the assembler
                if(instance.is_relative && e.section == SECTION_TEXT) continue;

                MachoRelocationInfo info = {0};
                info.addr = instance.offset;
                info.is_pc_rel = 1;
                info.length = 2;
                info.external = 1; 
                info.sym_num = symbol_index;

                if(e.section == SECTION_EXTERN){
                    info.type = 2;
                } else{
                    info.type = 1;
                }
                array_list_append(reloc_info, MachoRelocationInfo, info); 
            }
            symbol_index++;
        }

        MachoSymbolCmd sym_cmd = {0};
        sym_cmd.cmd = 0x2;
        sym_cmd.cmdsize = sizeof(MachoSymbolCmd);
        sym_cmd.offset = offset + reloc_info.size * sizeof(MachoRelocationInfo); 
        sym_cmd.symbol_count = sym_table_entries.size;
        sym_cmd.string_table_offset = sym_cmd.offset + sym_table_entries.size * sizeof(MachoSymbolTableEntry);
        sym_cmd.string_table_size = scratch_buffer_offset() + 1;

        fwrite(&sym_cmd, sizeof(MachoSymbolCmd),1, output_stream);
    }


    fwrite(program.text.data, 1, program.text.size, output_stream); 
    fwrite(program.data.data, 1, program.data.size, output_stream); 

    if(padding != 0){
        for(int i = padding; i > 0; i--){
            fputc(0, output_stream);
        }
    }

    fwrite(reloc_info.data, sizeof(MachoRelocationInfo), reloc_info.size, output_stream); 
    fwrite(sym_table_entries.data, sizeof(MachoSymbolTableEntry), sym_table_entries.size, output_stream); 

    if(scratch_buffer_offset() != 0){
        fputc(0, output_stream);
        fwrite(scratch_buffer_get_data(0), 1,scratch_buffer_offset(), output_stream);
    }

    return true;
}
