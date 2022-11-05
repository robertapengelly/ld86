/******************************************************************************
 * @file            msdos.h
 *****************************************************************************/
#ifndef     _MSDOS_H
#define     _MSDOS_H

struct msdos_header {

    unsigned char   e_magic[2];         /* Magic number                     */
    unsigned short  e_cblp;             /* Bytes on last page of file       */
    unsigned short  e_cp;               /* Pages in file                    */
    unsigned short  e_crlc;             /* Relocations                      */
    unsigned short  e_cparhdr;          /* Size of header in paragraphs     */
    unsigned short  e_minalloc;         /* Minimum extra paragraphs needed  */
    unsigned short  e_maxalloc;         /* Maximum extra paragraphs needed  */
    unsigned short  e_ss;               /* Initial (relative) SS value      */
    unsigned short  e_sp;               /* Initial SP value                 */
    unsigned short  e_csum;             /* Checksum                         */
    unsigned short  e_ip;               /* Initial IP value                 */
    unsigned short  e_cs;               /* Initial (relative) CS value      */
    unsigned short  e_lfarlc;           /* File address of relocation table */
    unsigned short  e_ovno;             /* Overlay number                   */

};

#endif      /* _MSDOS_H */
