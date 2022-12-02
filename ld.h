/******************************************************************************
 * @file            ld.h
 *****************************************************************************/
#ifndef     _LD_H
#define     _LD_H

enum {

    LD_FORMAT_NONE = 0,
    
    LD_FORMAT_I386_AOUT, LD_FORMAT_I386_COFF, LD_FORMAT_I386_PE,
    LD_FORMAT_BINARY, LD_FORMAT_MSDOS, LD_FORMAT_MSDOS_MZ

};

#define     GET_INT32(arr)              ((int32_t) arr[0] | (((int32_t) arr[1]) << 8) | (((int32_t) arr[2]) << 16) | (((int32_t) arr[3]) << 24))
#define     GET_UINT32(arr)             ((uint32_t) arr[0] | (((uint32_t) arr[1]) << 8) | (((uint32_t) arr[2]) << 16) | (((uint32_t) arr[3]) << 24))

#include    "types.h"

struct aout_object {

    const char *filename;
    void *raw;
    
    unsigned long size;
    
    struct aout_exec *header;
    struct relocation_info *trelocs, *drelocs;
    
    struct nlist *symtab;
    char *strtab;
    
    int32_t symtab_count, trelocs_count, drelocs_count;
    uint32_t text_slide, data_slide, bss_slide;


};

struct coff_object {

    const char *filename;
    void *raw;
    
    unsigned long size;
    struct coff_header *header;

};

struct ld_state {

    const char **files;
    long nb_files;
    
    const char *entry, *mapfile, *outfile;
    int code_offset, format, impure;
    
    struct aout_object **aout_objs;
    long nb_aout_objs;
    
    struct coff_object **coff_objs;
    long nb_coff_objs;
    
    unsigned long text_size, data_size, bss_size;
    unsigned long stack_size;

};

extern struct ld_state *state;
extern const char *program_name;

#define     FILE_ALIGNMENT              512
#define     SECTION_ALIGNMENT           4096

#define     DIV_ROUNDUP(a, b)           (((a) + ((b) - 1)) / (b))
#define     ALIGN_UP(x, a)              (DIV_ROUNDUP ((x), (a)) * (a))

#endif      /* _LD_H */
