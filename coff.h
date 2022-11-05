/******************************************************************************
 * @file            coff.h
 *****************************************************************************/
#ifndef     _COFF_H
#define     _COFF_H

#include    <stddef.h>

struct coff_header {

    unsigned short Machine;
    unsigned short NumberOfSections;
    
    unsigned int TimeDateStamp;
    unsigned int PointerToSymbolTable;
    unsigned int NumberOfSymbols;
    
    unsigned short SizeOfOptionalHeader;
    unsigned short Characteristics;

};

#define     IMAGE_FILE_MACHINE_I386                         0x014C

#endif      /* _COFF_H */
