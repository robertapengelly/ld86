/******************************************************************************
 * @file            map.c
 *****************************************************************************/
#include    <stddef.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "aout.h"
#include    "hashtab.h"
#include    "ld.h"
#include    "map.h"

static int32_t map_text_start = 0;
static int32_t map_data_start = 0;
static int32_t map_bss_start = 0;

static int32_t map_text_size = 0;
static int32_t map_data_size = 0;
static int32_t map_bss_size = 0;

struct section_symbol { uint32_t value; };

struct map_object {

    struct hashtab *bss_section, *data_section, *text_section;
    uint32_t a_bss, a_data, a_text, idx;

};

static struct hashtab map_objects = { 0 };

static void sort_section_symbols (struct hashtab *section) {

    unsigned long i, j;
    
    if (section->count > 0) {
    
        struct hashtab_entry *entry1, *entry2;
        void *key, *value;
        
        struct section_symbol *symbol1, *symbol2;
        
        for (i = 0; i < section->capacity; i++) {
        
            if ((entry1 = &section->entries[i]) == NULL) {
                continue;
            }
            
            if (entry1->value == NULL) {
                continue;
            }
            
            symbol1 = (struct section_symbol *) entry1->value;
            
            for (j = 0; j < section->capacity; j++) {
            
                if ((entry2 = &section->entries[j]) == NULL) {
                    continue;
                }
                
                if (entry2->value == NULL) {
                    continue;
                }
                
                symbol2 = (struct section_symbol *) entry2->value;
                
                if (symbol1->value < symbol2->value) {
                
                    key = entry2->key;
                    value = entry2->value;
                    
                    entry2->key = entry1->key;
                    entry2->value = entry1->value;
                    
                    entry1->key = key;
                    entry1->value = value;
                
                }
            
            }
        
        }
    
    }

}

static void add_section_symbol (struct hashtab *section, const char *symname, uint32_t value) {

    struct hashtab_name *name = hashtab_alloc_name (symname);
    struct section_symbol *symbol;
    
    if ((symbol = hashtab_get (section, name)) == NULL) {
    
        if ((symbol = malloc (sizeof (*symbol))) == NULL) {
        
            free (name);
            return;
        
        }
        
        symbol->value = value;
        hashtab_put (section, name, symbol);
    
    } else {
    
        free (name);
        symbol->value = value;
    
    }

}

void add_map_object (const char *filename, uint32_t a_bss, uint32_t a_data, uint32_t a_text) {

    struct hashtab_name *name = hashtab_alloc_name (filename);
    struct map_object *object;
    
    if (hashtab_get (&map_objects, name) == NULL) {
    
        if ((object = malloc (sizeof (*object))) == NULL) {
        
            free (name);
            return;
        
        }
        
        object->a_bss  = a_bss;
        object->a_data = a_data;
        object->a_text = a_text;
        
        object->bss_section  = NULL;
        object->data_section = NULL;
        object->text_section = NULL;
        
        object->idx = map_objects.count;
        hashtab_put (&map_objects, name, object);
    
    } else {
        free (name);
    }

}

void add_map_bss_symbol (const char *filename, const char *symname, uint32_t value) {

    struct hashtab_name *name = hashtab_alloc_name (filename);
    struct map_object *object;
    
    if ((object = hashtab_get (&map_objects, name)) == NULL) {
    
        free (name);
        return;
    
    }
    
    free (name);
    
    if (object->bss_section == NULL) {
    
        if ((object->bss_section = malloc (sizeof (*object->bss_section))) == NULL) {
            return;
        }
        
        memset (object->bss_section, 0, sizeof (*object->bss_section));
    
    }
    
    add_section_symbol (object->bss_section, symname, value);

}

void add_map_data_symbol (const char *filename, const char *symname, uint32_t value) {

    struct hashtab_name *name = hashtab_alloc_name (filename);
    struct map_object *object;
    
    if ((object = hashtab_get (&map_objects, name)) == NULL) {
    
        free (name);
        return;
    
    }
    
    free (name);
    
    if (object->data_section == NULL) {
    
        if ((object->data_section = malloc (sizeof (*object->data_section))) == NULL) {
            return;
        }
        
        memset (object->data_section, 0, sizeof (*object->data_section));
    
    }
    
    add_section_symbol (object->data_section, symname, value);

}

void add_map_text_symbol (const char *filename, const char *symname, uint32_t value) {

    struct hashtab_name *name = hashtab_alloc_name (filename);
    struct map_object *object;
    
    if ((object = hashtab_get (&map_objects, name)) == NULL) {
    
        free (name);
        return;
    
    }
    
    free (name);
    
    if (object->text_section == NULL) {
    
        if ((object->text_section = malloc (sizeof (*object->text_section))) == NULL) {
            return;
        }
        
        memset (object->text_section, 0, sizeof (*object->text_section));
    
    }
    
    add_section_symbol (object->text_section, symname, value);

}

void generate_map (void) {

    FILE *map_ofp = stdout;
    
    struct hashtab_entry *entry;
    struct hashtab *table;
    
    struct map_object *object;
    
    int has_bss = 0, has_data = 0, has_text = 0;
    int has_header = 0, has_newline = 0;
    
    unsigned long i, j;
    
    if (strcmp (state->mapfile, "") != 0) {
    
        if ((map_ofp = fopen (state->mapfile, "w")) == NULL) {
        
            if (program_name) {
                fprintf (stderr, "%s: ", program_name);
            }
            
            fprintf (stderr, "error: failed to open '%s' as map file\n", state->mapfile);
            return;
        
        }
    
    }
    
    if (map_objects.count > 0) {
    
        struct hashtab_entry *entry1, *entry2;
        void *key, *value;
        
        struct map_object *object1, *object2;
        
        for (i = 0; i < map_objects.capacity; i++) {
        
            if ((entry1 = &map_objects.entries[i]) == NULL) {
                continue;
            }
            
            if (entry1->value == NULL) {
                continue;
            }
            
            object1 = (struct map_object *) entry1->value;
            
            for (j = 0; j < map_objects.capacity; j++) {
            
                if ((entry2 = &map_objects.entries[j]) == NULL) {
                    continue;
                }
                
                if (entry2->value == NULL) {
                    continue;
                }
                
                object2 = (struct map_object *) entry2->value;
                
                if (object1->idx < object2->idx) {
                
                    key = entry2->key;
                    value = entry2->value;
                    
                    entry2->key = entry1->key;
                    entry2->value = entry1->value;
                    
                    entry1->key = key;
                    entry1->value = value;
                
                }
            
            }
        
        }
    
    }
    
    for (i = 0; i < map_objects.capacity; ++i) {
    
        if ((entry = &map_objects.entries[i]) == NULL) {
            continue;
        }
        
        if ((object = (struct map_object *) entry->value) == NULL) {
            continue;
        }
        
        if (object->text_section != NULL) {
        
            sort_section_symbols (object->text_section);
            
            has_text = 1;
            
            if (!has_header) {
            
                has_header = 1;
                fprintf (map_ofp, ".text          0x%08X      0x%X\n\n", map_text_start, map_text_size);
            
            }
            
            if (entry->key != NULL) {
            
                fprintf (map_ofp, ".text          0x%08X          0x%04X %s\n", map_text_start, object->a_text, entry->key->chars);
                map_text_start += object->a_text;
            
            }
            
            table = object->text_section;
            
            for (j = 0; j < table->capacity; ++j) {
            
                struct hashtab_entry *entry = &table->entries[j];
                
                if (entry != NULL && entry->key != NULL) {
                
                    if (entry->value != NULL) {
                    
                        struct section_symbol *symbol = (struct section_symbol *) entry->value;
                        fprintf (map_ofp, "               0x%08X                      %s\n", symbol->value, entry->key->chars);
                    
                    }
                
                }
            
            }
            
            fprintf (map_ofp, "\n");
        
        }
    
    }
    
    if (has_text) {
        fprintf (map_ofp, ".text                          0x%X\n", map_text_start);
    }
    
    has_header = 0;
    has_newline = 0;
    
    for (i = 0; i < map_objects.capacity; ++i) {
    
        if ((entry = &map_objects.entries[i]) == NULL) {
            continue;
        }
        
        if ((object = (struct map_object *) entry->value) == NULL) {
            continue;
        }
        
        if (object->data_section != NULL) {
        
            sort_section_symbols (object->data_section);
            
            if (!has_newline) {
            
                has_newline = 1;
                
                if (has_text) {
                    fprintf (map_ofp, "\n\n");
                }
            
            }
            
            has_data = 1;
            
            if (!has_header) {
            
                has_header = 1;
                fprintf (map_ofp, ".data          0x%08X      0x%X\n\n", map_data_start, map_data_size);
            
            }
            
            if (entry->key != NULL) {
            
                fprintf (map_ofp, ".data          0x%08X          0x%04X %s\n", map_data_start, object->a_data, entry->key->chars);
                map_data_start += object->a_data;
            
            }
            
            table = object->data_section;
            
            for (j = 0; j < table->capacity; ++j) {
            
                struct hashtab_entry *entry = &table->entries[j];
                
                if (entry != NULL && entry->key != NULL) {
                
                    if (entry->value != NULL) {
                    
                        struct section_symbol *symbol = (struct section_symbol *) entry->value;
                        fprintf (map_ofp, "               0x%08X                      %s\n", symbol->value, entry->key->chars);
                    
                    }
                
                }
            
            }
            
            fprintf (map_ofp, "\n");
        
        }
    
    }
    
    if (has_data) {
        fprintf (map_ofp, ".data                          0x%X\n", map_data_start);
    }
    
    has_header = 0;
    has_newline = 0;
    
    for (i = 0; i < map_objects.capacity; ++i) {
    
        if ((entry = &map_objects.entries[i]) == NULL) {
            continue;
        }
        
        if ((object = (struct map_object *) entry->value) == NULL) {
            continue;
        }
        
        if (object->bss_section != NULL) {
        
            sort_section_symbols (object->bss_section);
            
            if (!has_newline) {
            
                has_newline = 1;
                
                if (has_text || has_data) {
                    fprintf (map_ofp, "\n\n");
                }
            
            }
            
            has_bss = 1;
            
            if (!has_header) {
            
                has_header = 1;
                fprintf (map_ofp, ".bss           0x%08X      0x%X\n\n", map_bss_start, map_bss_size);
            
            }
            
            if (entry->key != NULL) {
            
                fprintf (map_ofp, ".bss           0x%08X          0x%04X %s\n", map_bss_start, object->a_bss, entry->key->chars);
                map_bss_start += object->a_bss;
            
            }
            
            table = object->bss_section;
            
            for (j = 0; j < table->capacity; ++j) {
            
                struct hashtab_entry *entry = &table->entries[j];
                
                if (entry != NULL && entry->key != NULL) {
                
                    if (entry->value != NULL) {
                    
                        struct section_symbol *symbol = (struct section_symbol *) entry->value;
                        fprintf (map_ofp, "               0x%08X                      %s\n", symbol->value, entry->key->chars);
                    
                    }
                
                }
            
            }
            
            fprintf (map_ofp, "\n");
        
        }
    
    }
    
    if (has_bss) {
        fprintf (map_ofp, ".bss                           0x%X\n", map_bss_start);
    }
    
    if (map_ofp != stdout) {
        fclose (map_ofp);
    }

}

void set_map_sections_size (uint32_t text_size, uint32_t data_size, uint32_t bss_size) {

    map_text_size = text_size;
    map_data_size = data_size;
    map_bss_size = bss_size;

}

void set_map_sections_start (uint32_t text_start, uint32_t data_start, uint32_t bss_start) {

    map_text_start = text_start;
    map_data_start = data_start;
    map_bss_start = bss_start;

}
