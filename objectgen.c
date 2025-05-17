#include "util.h"
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






static int compare_visibility(const void *p1, const void *p2){
    const SymbolTableEntry* e1 = (p1);
    const SymbolTableEntry* e2 = (p2);    
    return e1->visibility - e2->visibility;
}



#define MACHINE_X86_64 62

 
static uint64_t section_pad(Section* section, uint64_t offset, uint64_t alignment){
    int padding = 0;
    while(offset % alignment != 0){
        offset++;
        padding++;
    }

    if(section->size + padding >= section->capacity){
        uint64_t new_capacity = section->capacity * 2;
        section->data = realloc(section->data, new_capacity);
        if(section->data == NULL){
            printf("Out of memory\n");
            return 0;
        }
        section->capacity = new_capacity;
    }

    for(int i = 0; i < padding; i++){
        section->data[section->size++] = 0;

    }
    return offset;
}


static uint32_t get_reloc_count(Program* p){
    uint32_t count = 0;
    //only allow text section relocations for now
    for(int i = 0; i < p->symTable.symbols.size; i++){
        SymbolTableEntry e = array_list_get(p->symTable.symbols, SymbolTableEntry, i);
        for(int j = 0; j < e.instances.size; j++){
            SymbolInstance instance = array_list_get(e.instances, SymbolInstance, j);
            //skip over the relative instances
            if(!instance.is_relative){
                count++;
            }
        }
    }
    return count;
}




bool write_elf(const char* input_file, const char* output_file, Program* p){
    FILE* output_stream = fopen(output_file, "wb");

    if(output_stream == NULL){
        printf("Failed to create file %s\n", input_file);
        return false;
    }

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


    int string_table_index = 2;


    //section names table, linker symbol table, string table, .text section, plus null header
    head.section_header_entries = 5;
    if(p->bss.size > 0) {
        head.section_header_entries += 1;
        string_table_index += 1;
    }
    if(p->data.size > 0){
        head.section_header_entries += 1;
        string_table_index += 1;
    } 
    head.string_table_index = string_table_index;

    bool has_text_reloca = false;
    uint32_t num_text_reloca_entries = get_reloc_count(p);
    if(num_text_reloca_entries > 0) has_text_reloca = true;

 
    head.section_header_entries += (int)has_text_reloca;

    fwrite(&head, sizeof(ElfHeader),1, output_stream);

    uint64_t offset = head.section_header_size * head.section_header_entries + head.header_size;

    int section_index = 0;
    int section_count = 0;

    uint64_t data_offset = 0;

    //first section is always null
    ElfSectionHeader null_header = {0};
    fwrite(&null_header, sizeof(null_header),1, output_stream);
    section_index++;
    
    //TODO MAKE SURE THAT IT HAS A TEXT SECTION
    scratch_buffer_clear();

    //hold the section string table
    scratch_buffer_append_char(0);
    scratch_buffer_append_str(".text");

    ElfSectionHeader text = {0};
    text.name = 1;
    text.type = ELF_SECTION_PINFO;
    text.flags = ELF_SF_ALLOC | ELF_SF_EXECINSTR;
    text.addr = 0;
    text.offset = offset;
    text.size = p->text.size;
    text.addralign = 16; //CHECK THIS
    fwrite(&text, sizeof(text), 1, output_stream);
    section_index++;
    section_count++;


    offset += text.size;
    offset = section_pad(&p->text, offset, text.addralign);

    
    if(p->data.size > 0){
        ElfSectionHeader data = {0};
        data.type = ELF_SECTION_PINFO;
        data.flags = ELF_SF_ALLOC | ELF_SF_WRITE;
        data.name = scratch_buffer_offset();
        data.size = p->data.size;
        data.offset = offset;
        data.addralign = 4;

        data_offset = data.offset;
        offset += data.size;
        offset = section_pad(&p->data, offset, data.addralign);


        scratch_buffer_append_str(".data");
        fwrite(&data, sizeof(data), 1, output_stream);

        section_index++;
        section_count++;
    }

    if(p->bss.size > 0){
        ElfSectionHeader bss = {0};
        bss.type = ELF_SECTION_NOBITS;
        bss.flags = ELF_SF_ALLOC | ELF_SF_WRITE;
        bss.name = scratch_buffer_offset();
        bss.size = p->bss.size;
        bss.offset = offset;
        bss.addralign = 4; //seems to be the default but idk
        
        scratch_buffer_append_str(".bss");
        fwrite(&bss, sizeof(bss), 1, output_stream);

        section_index++;
        section_count++;
    }


    ElfSectionHeader section_st = {0};
    section_st.type = ELF_SECTION_STRING_TABLE; 
    section_st.name = scratch_buffer_offset();
    section_st.offset = offset;
    section_st.addralign = 1;

    section_index++;

    //write the remaing
    scratch_buffer_append_str(".shrstrtab");
    uint64_t symbol_table_st_index = scratch_buffer_offset();
    scratch_buffer_append_str(".symtab");
    uint64_t symbol_string_table_st_index = scratch_buffer_offset();
    scratch_buffer_append_str(".strtab");
    uint64_t text_reloca_st_index = 0;

    if(has_text_reloca){
        text_reloca_st_index = scratch_buffer_offset();
        scratch_buffer_append_str(".rela.text");
    }

    char* section_string_table = scratch_buffer_get_data(0);
    section_st.size = scratch_buffer_offset();

    fwrite(&section_st, sizeof(section_st), 1, output_stream);


    //pad the section string table with zeros to align with the symbol table
    while((section_st.offset + section_st.size) % 8 != 0){
        section_st.size++;
        scratch_buffer_append_char(0);
    }


    //THE GLOBAL SYMBOLS MUST COME AFTER THE LOCAL ONES 
    // account for null symbol, text section, and file name
    int sym_table_info = 3;
    if(data_offset != 0) sym_table_info++;
    if(p->bss.size > 0) sym_table_info++;

    if(p->symTable.symbols.data != NULL){
        qsort(p->symTable.symbols.data, p->symTable.symbols.size, sizeof(SymbolTableEntry),compare_visibility);

        //get the index of the last local var
        for(int i = p->symTable.symbols.size - 1; i >= 0; i--){
            SymbolTableEntry e = array_list_get(p->symTable.symbols, SymbolTableEntry, i);
            if(e.visibility == VISIBILITY_LOCAL){
                sym_table_info += i + 1;
                break;
            }
        }
    }


    ElfSectionHeader symbol_table= {0};
    symbol_table.type = ELF_SECTION_LSYMTABLE;
    symbol_table.name = symbol_table_st_index;
    symbol_table.addralign = 8;
    symbol_table.link = section_index + 1; //symbol string table will follow 
    symbol_table.info = sym_table_info; // one plus index of last local symbol 
    symbol_table.entsize = sizeof(ElfSymbolEntry);
    symbol_table.addralign = 8;
    //sections plus all symbols plus the file and null header
    symbol_table.size = ((section_count + p->symTable.symbols.size) + 2) * symbol_table.entsize; 
    symbol_table.offset = section_st.offset + section_st.size; 

    fwrite(&symbol_table, sizeof(symbol_table), 1, output_stream);

    


    ElfSectionHeader symbol_str_table = {0};
    symbol_str_table.type = ELF_SECTION_STRING_TABLE;
    symbol_str_table.name = symbol_string_table_st_index;
    symbol_str_table.addralign = 1; 
    symbol_str_table.offset = symbol_table.offset + symbol_table.size;


    uint8_t padding[8] = {0};
    int symbol_table_padding = 0;
    int text_reloca_padding = 0;


    while(symbol_table.offset % symbol_table.addralign != 0){
        symbol_table.offset++;
        symbol_str_table.offset++;
        symbol_table_padding++;
    }


    symbol_str_table.size = strlen(input_file) + 1 + 1;
    for(int i = 0; i < p->symTable.symbols.size; i++){
        SymbolTableEntry e = array_list_get(p->symTable.symbols, SymbolTableEntry, i);
        symbol_str_table.size += strlen(e.name) + 1;
    }


    fwrite(&symbol_str_table, sizeof(symbol_str_table), 1, output_stream);


    if(has_text_reloca){
        uint64_t text_reloc_offset = symbol_str_table.offset + symbol_str_table.size;

        while(text_reloc_offset % 8 != 0){
            text_reloc_offset++;
            text_reloca_padding++;
        }

        ElfSectionHeader text_reloc = {0};
        text_reloc.name = text_reloca_st_index;
        text_reloc.type = ELF_SECTION_RELAENTRY; 
        text_reloc.offset = text_reloc_offset;
        text_reloc.link = symbol_table.link - 1; //points to the symbol table
        text_reloc.info = 1; // points to the text section?
        text_reloc.addralign = 8;
        text_reloc.entsize = sizeof(ElfRelocatableEntry); 
        text_reloc.size = text_reloc.entsize * num_text_reloca_entries;
        fwrite(&text_reloc, sizeof(text_reloc), 1, output_stream);
    }



    fwrite(p->text.data, 1, p->text.size, output_stream);
    if(data_offset != 0) fwrite(p->data.data,1,p->data.size, output_stream);

    fwrite(section_string_table,1, section_st.size, output_stream);
    fwrite(padding, 1, symbol_table_padding, output_stream);


    //setup the symbol string table
    scratch_buffer_clear();

    //first symbol is always null
    scratch_buffer_append_char(0);
    ElfSymbolEntry file_sym = {0};
    fwrite(&file_sym, sizeof(file_sym), 1, output_stream);

    //add the file name
    scratch_buffer_append_str((char*)input_file);


    file_sym.name = 1;
    file_sym.section_index = 0xfff1;
    file_sym.info = SB_LOCAL + SB_FILE;


    //write the file 
    fwrite(&file_sym, sizeof(file_sym), 1, output_stream);


    
    ElfSymbolEntry temp  = {0};
    temp.name = 0;
    temp.section_index = 1;
    temp.info = SB_SECTION + SB_LOCAL;

    //write text entry 
    fwrite(&temp, sizeof(temp), 1, output_stream);


    //write data entry
    if(data_offset != 0){
        ElfSymbolEntry d = {0};
        d.section_index = 2;
        d.info = SB_SECTION + SB_LOCAL;
        fwrite(&d, sizeof(d), 1, output_stream);
    }

    //write data entry 
    if(p->bss.size > 0){
        ElfSymbolEntry b = {0};
        //if there is a data section it goes right after it 
        b.section_index = (data_offset != 0) ? 3 : 2;
        b.info = SB_SECTION + SB_LOCAL;
        fwrite(&b, sizeof(b), 1, output_stream);
    }

    for(int i = 0; i < p->symTable.symbols.size; i++){
        memset(&temp, 0, sizeof(ElfSymbolEntry));
        SymbolTableEntry e = array_list_get(p->symTable.symbols, SymbolTableEntry, i);
        temp.name = scratch_buffer_offset();
        
        temp.section_index = e.section;
        //if there is no data section bss section will be at SECTION_BSS - 1
        if(e.section == SECTION_BSS && data_offset == 0){
            temp.section_index = 2;    
        } 

        temp.value = e.section_offset;
        temp.info =  (e.visibility == VISIBILITY_GLOBAL) ? SB_GLOBAL : SB_LOCAL; 
        fwrite(&temp, sizeof(temp), 1,output_stream);
        scratch_buffer_append_str(e.name);
    }

    char* sym_strt_str = scratch_buffer_get_data(0);
    fwrite(sym_strt_str,1, scratch_buffer_offset(), output_stream);

    if(has_text_reloca){
        fwrite(padding,1, text_reloca_padding, output_stream);

        //for pc relative symbols 
        //there index into the elf symbol table 
        //will be calculated using our symbol table index 
        //plus all the entries that come before our symbols (null, text, file, bss, etc)
        int pc_sym_index = 3;
        if(data_offset != 0) pc_sym_index++;
        if(p->bss.size > 0) pc_sym_index++;

        for(int i = 0; i < p->symTable.symbols.size; i++){
            SymbolTableEntry e = array_list_get(p->symTable.symbols, SymbolTableEntry, i);
            for(int j = 0; j < e.instances.size; j++){
                SymbolInstance instance = array_list_get(e.instances, SymbolInstance, j);

                if(instance.is_relative) continue;

                ElfRelocatableEntry reloc_e = {0};

                if(e.section == SECTION_EXTERN){
                    reloc_e.offset = instance.offset;
                    //still not sure exactly why its this
                    reloc_e.addend = -4;
                    reloc_e.info = ((uint64_t)(pc_sym_index + i) << 32)| RELOC_PC32;
                } else{
                    //assume 32 for now 
                    reloc_e.offset = instance.offset;
                    reloc_e.addend = e.section_offset;
                    reloc_e.info = ((uint64_t)(e.section + 1) << 32)| RELOC_32;
                }
                fwrite(&reloc_e, sizeof(reloc_e), 1, output_stream);
            }
        }




    }
    fclose(output_stream);
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
        printf("Failed to create file %s\n", input_file);
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

    //TODO: CHECK FOR FILE NAMES GREATER THAN 18 BYTES
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

                if(instance.is_relative) continue;

                PERelocatableEntry reloc_e = {0};
                reloc_e.virtual_addr = instance.offset;

                //each section in the symbol table has an auxiliary section 
                //thats why we multiply by 2
                reloc_e.symbol_table_index = sym_table_text_offset + (e.section - 1) * 2;

                if(e.section == SECTION_EXTERN){
                    reloc_e.type = PE_RELOC_AMD64_REL32; 
                } else{
                    reloc_e.type = PE_RELOC_AMD64_ADDR32;
                }
                fwrite(&reloc_e, sizeof(reloc_e), 1, output_stream);
            }
        }
    }

    if(p->data.size != 0) fwrite(p->data.data, 1, p->data.size, output_stream);


     //TODO: HANDLE FILE NAMES LARGER THAN 18 CHARS
    PESymbolTableEntry temp_entry = {0};
    strcpy(temp_entry.name, ".file");
    temp_entry.section = -2;
    temp_entry.aux_symbol_count = 1;
    temp_entry.storage_class = PE_SC_FILE;
    fwrite(&temp_entry, sizeof(temp_entry),1, output_stream);

    char file_name[18] = {0};
    strcpy(file_name,input_file);
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
    strcpy(temp_entry.name, ".absolut");
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
