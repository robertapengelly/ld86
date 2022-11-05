/******************************************************************************
 * @file            aout.h
 *****************************************************************************/
#ifndef     _AOUT_H
#define     _AOUT_H

#include    <stddef.h>

#define     N_GETMAGIC(exec)            ((exec).a_info & 0xffff)

struct aout_exec {

    unsigned int a_info;
    unsigned int a_text;
    unsigned int a_data;
    unsigned int a_bss;
    unsigned int a_syms;
    unsigned int a_entry;
    unsigned int a_trsize;
    unsigned int a_drsize;

};

#define     OMAGIC                      0407
#define     NMAGIC                      0410
#define     ZMAGIC                      0413
#define     QMAGIC                      0314

/* Relocation entry. */
struct relocation_info {

    int r_address;
    unsigned int r_symbolnum;

};

/* Symbol table entry. */
struct nlist {

    int n_strx;
    unsigned char n_type;
    
    char n_other;
    short n_desc;
    
    unsigned int n_value;

};

/* n_type values: */
#define     N_UNDF                      0x00
#define     N_ABS                       0x02
#define     N_TEXT                      0x04
#define     N_DATA                      0x06
#define     N_BSS                       0x08
#define     N_COMM                      0x12
#define     N_FN                        0x1f

#define     N_EXT                       0x01
#define     N_TYPE                      0x1e

int create_executable_from_aout_objects (void);

#endif      /* _AOUT_H */
