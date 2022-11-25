/******************************************************************************
 * @file            coff.h
 *****************************************************************************/
#ifndef     _COFF_H
#define     _COFF_H

#include    <stddef.h>

struct coff_header {

    uint16_t Machine;
    uint16_t NumberOfSections;
    
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;

};

#define     IMAGE_FILE_MACHINE_I386                         0x014C

#endif      /* _COFF_H */
