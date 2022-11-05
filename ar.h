/******************************************************************************
 * @file            ar.h
 *****************************************************************************/
#ifndef     _AR_H
#define     _AR_H

struct ar_header {

    char name[16];
    char mtime[12];
    char owner[6];
    char group[6];
    char mode[8];
    char size[10];
    char endsig[2];

};

#endif      /* _AR_H */
