/******************************************************************************
 * @file            map.h
 *****************************************************************************/
#ifndef     _MAP_H
#define     _MAP_H

void add_map_object (const char *filename, uint32_t a_bss, uint32_t a_data, uint32_t a_text);

void add_map_bss_symbol (const char *filename, const char *symname, uint32_t value);
void add_map_data_symbol (const char *filename, const char *symname, uint32_t value);
void add_map_text_symbol (const char *filename, const char *symname, uint32_t value);

void generate_map (void);
void set_map_sections_size (uint32_t text_size, uint32_t data_size, uint32_t bss_size);
void set_map_sections_start (uint32_t text_start, uint32_t data_start, uint32_t bss_start);

#endif      /* _MAP_H */
