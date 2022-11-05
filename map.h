/******************************************************************************
 * @file            map.h
 *****************************************************************************/
#ifndef     _MAP_H
#define     _MAP_H

void add_map_object (const char *filename, unsigned int a_bss, unsigned int a_data, unsigned int a_text);

void add_map_bss_symbol (const char *filename, const char *symname, unsigned int value);
void add_map_data_symbol (const char *filename, const char *symname, unsigned int value);
void add_map_text_symbol (const char *filename, const char *symname, unsigned int value);

void generate_map (void);
void set_map_sections_size (unsigned int text_size, unsigned int data_size, unsigned int bss_size);
void set_map_sections_start (unsigned int text_start, unsigned int data_start, unsigned int bss_start);

#endif      /* _MAP_H */
