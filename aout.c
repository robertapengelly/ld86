/******************************************************************************
 * @file            aout.c
 *****************************************************************************/
#include    <stddef.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "aout.h"
#include    "ld.h"
#include    "lib.h"
#include    "map.h"
#include    "msdos.h"
#include    "report.h"
#include    "types.h"
#include    "write7x.h"

static struct aout_exec *aout_hdr;
static struct msdos_header *msdos_hdr;

static unsigned long header_size = 0, output_size = 0;
static void *data = 0, *output = 0, *text = 0;


struct gr {

    int32_t relocations_count;
    int32_t relocations_max;
    
    struct relocation_info *relocations;

};

static struct gr tgr = { 0, 64, NULL };
static struct gr dgr = { 0, 64, NULL };


static int get_symbol (struct aout_object **obj_out, int32_t *index, const char *name, int quiet) {

    long object_i;
    int32_t symbol_i;
    
    for (object_i = 0; object_i < state->nb_aout_objs; ++object_i) {
    
        struct aout_object *obj = state->aout_objs[object_i];
        
        for (symbol_i = 0; symbol_i < obj->symtab_count; symbol_i++) {
        
            struct nlist *sym = &obj->symtab[symbol_i];
            char *symname = obj->strtab + GET_INT32 (sym->n_strx);
            
            if ((sym->n_type & N_EXT) == 0) {
                continue;
            }
            
            if ((sym->n_type & N_TYPE) != N_TEXT && (sym->n_type & N_TYPE) != N_DATA && (sym->n_type & N_TYPE) != N_BSS && (sym->n_type & N_TYPE) != N_ABS) {
                continue;
            }
            
            if (strcmp (symname, name) == 0) {
            
                if (obj_out) {
                    *obj_out = obj;
                }
                
                if (index) {
                    *index = symbol_i;
                }
                
                return 0;
            
            }
        
        }
    
    }
    
    if (!quiet) {
        report_at (program_name, 0, REPORT_ERROR, "undefined symbol '%s'", name);
    }
    
    return 1;

}

static uint32_t get_entry (void) {

    struct aout_object *symobj;
    int32_t symidx;
    
    if (!state->entry || strcmp (state->entry, "") == 0) {
        state->entry = "_start";
    }
    
    if (get_symbol (&symobj, &symidx, state->entry, 1)) {
    
        report_at (program_name, 0, REPORT_WARNING, "cannot find entry symbol %s; defaulting to 00000000", state->entry);
        return 0;
    
    }
    
    return GET_UINT32 (symobj->symtab[symidx].n_value);

}

static void number_to_chars (unsigned char *p, unsigned long number, unsigned long size) {
    
    unsigned long i;
    
    for (i = 0; i < size; i++) {
        p[i] = (number >> (8 * i)) & 0xff;
    }

}

static unsigned long fix_offset (struct relocation_info r, unsigned long result) {

    uint32_t zapdata = ((char *) text - (char *) output);
    
    if (((GET_UINT32 (r.r_symbolnum) >> 24) & 0xff) != N_TEXT) {
        return result;
    }
    
    result += zapdata;
    
    if (state->format == LD_FORMAT_I386_PE) {
    
        uint32_t data_offset = ((char *) data - (char *) output);
        
        if (result >= data_offset) {
        
            result += ALIGN_UP (zapdata, SECTION_ALIGNMENT);
            result += ALIGN_UP (state->text_size, SECTION_ALIGNMENT);
            
            result -= zapdata;
        
        } else {
            result += ALIGN_UP (zapdata, SECTION_ALIGNMENT);
        }
        
        result -= zapdata;
    
    }
    
    return result;

}

static void fix_offsets (void) {

    uint32_t zapdata = ((char *) text - (char *) output);
    int32_t i, length;
    
    for (i = 0; i < tgr.relocations_count; i++) {
    
        struct relocation_info *r = &tgr.relocations[i];
        
        unsigned char *p = (unsigned char *) text + GET_INT32 (r->r_address);
        uint32_t orig = 0;
        
        if (((GET_UINT32 (r->r_symbolnum) >> 24) & 0xff) != N_TEXT) {
            continue;
        }
        
        length = (GET_UINT32 (r->r_symbolnum) & (3L << 25)) >> 25;
        
        switch (length) {
        
            case 0:
            
                length = 1;
                break;
            
            case 1:
            
                length = 2;
                break;
            
            case 2:
            
                length = 4;
                break;
        
        }
        
        if (*(p - 1) == 0x9A) {
        
            uint32_t temp = *(int32_t *) p;
            
            orig = ((temp >> 16) & 0xffff) * 16;
            orig += temp & 0xffff;
        
        } else {
            memcpy (&orig, p, length);
        }
        
        orig += zapdata;
        
        if (state->format == LD_FORMAT_I386_PE) {
        
            uint32_t data_offset = ((char *) data - (char *) output);
            
            if (orig >= data_offset) {
            
                orig += ALIGN_UP (zapdata, SECTION_ALIGNMENT);
                orig += ALIGN_UP (state->text_size, SECTION_ALIGNMENT);
                
                orig -= zapdata;
            
            } else {
                orig += ALIGN_UP (zapdata, SECTION_ALIGNMENT);
            }
            
            orig -= zapdata;
        
        }
        
        if (*(p - 1) == 0x9A) {
        
            number_to_chars (p, orig % 16, 2);
            number_to_chars (p + 2, orig / 16, 2);
        
        } else {
            memcpy (p, &orig, length);
        }
    
    }
    
    for (i = 0; i < dgr.relocations_count; i++) {
    
        struct relocation_info *r = &dgr.relocations[i];
        uint32_t orig = 0;
        
        if (((GET_UINT32 (r->r_symbolnum) >> 24) & 0xff) != N_TEXT) {
            continue;
        }
        
        length = (GET_UINT32 (r->r_symbolnum) & (3L << 25)) >> 25;
        
        switch (length) {
        
            case 0:
            
                length = 1;
                break;
            
            case 1:
            
                length = 2;
                break;
            
            case 2:
            
                length = 4;
                break;
        
        }
        
        memcpy (&orig, ((char *) data + GET_INT32 (r->r_address)), length);
        orig += zapdata;
        
        if (state->format == LD_FORMAT_I386_PE) {
        
            uint32_t data_offset = ((char *) data - (char *) output);
            
            if (orig >= data_offset) {
            
                orig += ALIGN_UP (zapdata, SECTION_ALIGNMENT);
                orig += ALIGN_UP (state->text_size, SECTION_ALIGNMENT);
                
                orig -= zapdata;
            
            } else {
                orig += ALIGN_UP (zapdata, SECTION_ALIGNMENT);
            }
            
            orig -= zapdata;
        
        }
        
        memcpy ((char *) data + GET_INT32 (r->r_address), &orig, length);
    
    }

}


static int32_t objtextsize = 0, objdatasize = 0, objbsssize = 0;
static uint32_t text_ptr = 0, data_ptr = 0, bss_ptr = 0;

static void apply_slides (struct aout_object *object) {

    int32_t i;
    
    for (i = 0; i < object->symtab_count; i++) {
    
        struct nlist *symbol = &object->symtab[i];
        uint32_t final_slide = 0, n_value = GET_UINT32 (symbol->n_value);
        
        if ((symbol->n_type & N_TYPE) != N_TEXT && (symbol->n_type & N_TYPE) != N_DATA && (symbol->n_type & N_TYPE) != N_BSS) {
            continue;
        }
        
        switch (symbol->n_type & N_TYPE) {
        
            case N_BSS:
            
                final_slide += state->text_size;
                final_slide += state->data_size;
                final_slide += object->bss_slide;
                
                break;
            
            case N_DATA:
            
                final_slide += state->text_size;
                final_slide += object->data_slide;
                
                break;
            
            case N_TEXT:
            
                final_slide += object->text_slide;
                break;
        
        }
        
        n_value += final_slide;
        
        switch (symbol->n_type & N_TYPE) {
        
            case N_BSS:
            
                n_value -= GET_UINT32 (object->header->a_data);
                /* fall through */
            
            case N_DATA:
            
                n_value -= GET_UINT32 (object->header->a_text);
                break;
        
        }
        
        write741_to_byte_array (symbol->n_value, n_value);
    
    }
    
    for (i = 0; i < object->trelocs_count; i++) {
    
        struct relocation_info *rel = &object->trelocs[i];
        
        int32_t r_address = GET_INT32 (rel->r_address);
        r_address += object->text_slide;
        
        write741_to_byte_array ((unsigned char *) rel->r_address, r_address);
    
    }
    
    for (i = 0; i < object->drelocs_count; i++) {
    
        struct relocation_info *rel = &object->drelocs[i];
        
        int32_t r_address = GET_INT32 (rel->r_address);
        r_address += state->text_size + object->data_slide;
        
        write741_to_byte_array ((unsigned char *) rel->r_address, r_address);
    
    }

}

static void init_map (struct aout_object *object) {

    int32_t symbol_i;
    
    add_map_object (object->filename, GET_UINT32 (object->header->a_bss), GET_UINT32 (object->header->a_data), GET_UINT32 (object->header->a_text));
    
    for (symbol_i = 0; symbol_i < object->symtab_count; symbol_i++) {
    
        struct nlist *sym = &object->symtab[symbol_i];
        char *symname = object->strtab + GET_INT32 (sym->n_strx);
        
        if ((sym->n_type & N_TYPE) == N_UNDF) {
            continue;
        }
        
        if (strcmp (symname, ".text") == 0 || strcmp (symname, ".data") == 0 || strcmp (symname, ".bss") == 0) {
            continue;
        }
        
        if ((sym->n_type & N_TYPE) == N_TEXT) {
            add_map_text_symbol (object->filename, xstrdup (symname), GET_UINT32 (sym->n_value));
        } else if ((sym->n_type & N_TYPE) == N_DATA) {
            add_map_data_symbol (object->filename, xstrdup (symname), GET_UINT32 (sym->n_value));
        } else if ((sym->n_type & N_TYPE) == N_BSS) {
            add_map_bss_symbol (object->filename, xstrdup (symname), GET_UINT32 (sym->n_value));
        }
    
    }

}

static void paste (struct aout_object *object) {

    struct aout_exec *header = object->header;
    
    char *obj_text, *obj_data;
    uint32_t obj_text_size, obj_data_size, obj_bss_size;
    
    object->text_slide = text_ptr;
    obj_text = (char *) object->raw + sizeof (*header);
    
    if (state->impure) {
        obj_text_size = GET_UINT32 (header->a_text);
    } else {
    
        if (state->format == LD_FORMAT_I386_PE) {
            obj_text_size = ALIGN_UP (GET_UINT32 (header->a_text), FILE_ALIGNMENT);
        } else {
            obj_text_size = ALIGN_UP (GET_UINT32 (header->a_text), SECTION_ALIGNMENT);
        }
    
    }
    
    memcpy ((char *) text + text_ptr, obj_text, GET_UINT32 (header->a_text));
    text_ptr += obj_text_size;
    
    object->data_slide = data_ptr;
    obj_data = (char *) object->raw + sizeof (*header) + GET_UINT32 (header->a_text);
    
    if (state->impure) {
        obj_data_size = GET_UINT32 (header->a_data);
    } else {
    
        if (state->format == LD_FORMAT_I386_PE) {
            obj_data_size = ALIGN_UP (GET_UINT32 (header->a_data), FILE_ALIGNMENT);
        } else {
            obj_data_size = ALIGN_UP (GET_UINT32 (header->a_data), SECTION_ALIGNMENT);
        }
    
    }
    
    memcpy ((char *) data + data_ptr, obj_data, GET_UINT32 (header->a_data));
    data_ptr += obj_data_size;
    
    object->bss_slide = bss_ptr;
    
    if (state->impure) {
        obj_bss_size = GET_UINT32 (header->a_bss);
    } else {
    
        if (state->format == LD_FORMAT_I386_PE) {
            obj_bss_size = ALIGN_UP (GET_UINT32 (header->a_bss), FILE_ALIGNMENT);
        } else {
            obj_bss_size = ALIGN_UP (GET_UINT32 (header->a_bss), SECTION_ALIGNMENT);
        }
    
    }
    
    bss_ptr += obj_bss_size;

}

static void undef_collect (struct aout_object *object) {

    int32_t i, val;
    
    for (i = 0; i < object->symtab_count; i++) {
    
        struct nlist *sym = &object->symtab[i];
        char *symname = object->strtab + GET_INT32 (sym->n_strx);
        
        if ((sym->n_type & N_TYPE) != N_UNDF || GET_UINT32 (sym->n_value) == 0) {
            continue;
        }
        
        if (get_symbol (NULL, NULL, symname, 1)) {
            continue;
        }
        
        sym->n_type = N_BSS | N_EXT;
        val = GET_UINT32 (sym->n_value);
        
        write741_to_byte_array (sym->n_value, state->text_size + state->data_size + state->bss_size);
        state->bss_size += val;
    
    }

}

static int remove_relocation (struct gr *gr, struct relocation_info r) {

    struct relocation_info *relocs;
    int32_t i, j;
    
    if (gr->relocations == NULL || gr->relocations_count == 0) {
        return 0;
    }
    
    if ((relocs = malloc (gr->relocations_max * sizeof (*relocs))) == NULL) {
        return 1;
    }
    
    for (i = 0, j = 0; i < gr->relocations_count; ++i) {
    
        if (GET_INT32 (gr->relocations[i].r_address) == GET_INT32 (r.r_address) && GET_UINT32 (gr->relocations[i].r_symbolnum) == GET_UINT32 (r.r_symbolnum)) {
            continue;
        }
        
        memcpy (&relocs[j++], &gr->relocations[i], sizeof (r));
    
    }
    
    free (gr->relocations);
    
    gr->relocations = relocs;
    gr->relocations_count = j;
    
    return 0;

}

static int add_relocation (struct gr *gr, struct relocation_info *r) {

    if (gr->relocations == NULL) {
    
        if ((gr->relocations = malloc (gr->relocations_max * sizeof (*r))) == NULL) {
            return 1;
        }
    
    }
    
    if (gr->relocations_count >= gr->relocations_max) {
    
        void *tmp;
        
        gr->relocations_max *= 2;
        
        if ((tmp = realloc (gr->relocations, gr->relocations_max * sizeof (*r))) == NULL) {
            return 1;
        }
        
        gr->relocations = tmp;
    
    }
    
    gr->relocations[gr->relocations_count] = *r;
    gr->relocations_count++;
    
    return 0;

}

static int relocate (struct aout_object *object, struct relocation_info *r, int is_data) {

    struct nlist *symbol;
    
    unsigned char *p;
    int dgroup = 0, _end = 0, _edata = 0;
    
    int32_t opcode, result = 0;
    uint32_t r_symbolnum = GET_UINT32 (r->r_symbolnum);
    
    int32_t symbolnum = r_symbolnum & 0xffffff;
    int32_t pcrel = (r_symbolnum & (1L << 24)) >> 24;
    int32_t ext = (r_symbolnum & (1L << 27)) >> 27;
    int32_t baserel = (r_symbolnum & (1L << 28)) >> 28;
    int32_t jmptable = (r_symbolnum & (1L << 29)) >> 29;
    int32_t rel = (r_symbolnum & (1L << 30)) >> 30;
    int32_t copy = (r_symbolnum & (1L << 31)) >> 31;

    int32_t length = (r_symbolnum & (3L << 25)) >> 25;
    
    switch (length) {
    
        case 0:
        
            length = 1;
            break;
        
        case 1:
        
            length = 2;
            break;
        
        case 2:
        
            length = 4;
            break;
    
    }
    
    symbol = &object->symtab[symbolnum];
    
    if ((is_data && pcrel) || baserel || jmptable || rel || copy) {
    
        report_at (program_name, 0, REPORT_ERROR, "unsupported relocation type");
        return 1;
    
    }
    
    p = (unsigned char *) output + header_size + GET_INT32 (r->r_address);
    opcode = *(int32_t *) (p - 1) & 0xff;
    
    if (ext) {
    
        char *symname = object->strtab + GET_INT32 (symbol->n_strx);
        char *temp = symname;
        
        struct aout_object *symobj;
        int32_t symidx;
        
        if (state->format != LD_FORMAT_I386_AOUT && strstart ("DGROUP", (const char **) &temp)) {
        
            if (strcmp (temp, "__end") == 0) {
            
            	int32_t temp = ((char *) data - (char *) output);
            	
            	temp += state->data_size;
            	temp += state->bss_size;
            	
            	write741_to_byte_array (symbol->n_value, temp % 16);
                _end = 1;
            
            } else if (strcmp (temp, "__edata") == 0) {
            
                int32_t temp = ((char *) data - (char *) output);
                temp += state->data_size;
                
            	write741_to_byte_array (symbol->n_value, temp % 16);
            	_edata = 1;
            
            } else {
            
                result = ((char *) data - (char *) output);
                result += state->code_offset;
                
                dgroup = 1;
            
            }
        
        } else if (!get_symbol (&symobj, &symidx, symname, 0)) {
            symbol = &symobj->symtab[symidx];
        } else {
            return 1;
        }
    
    }
    
    if (pcrel) {
    
        if (result == 0) {
            result = (long) GET_UINT32 (symbol->n_value) - (GET_INT32 (r->r_address) + length);
        }
    
    } else {
    
        if (dgroup || !ext || (symbol->n_type & N_TYPE) == N_BSS || (symbol->n_type & N_TYPE) == N_DATA || (symbol->n_type & N_TYPE) == N_TEXT) {
        
            struct relocation_info new_relocation;
            
            int32_t r_address;
            uint32_t r_symbolnum;
            
            r_address = GET_INT32 (r->r_address);
            
            if (is_data && /*state->format == LD_FORMAT_I386_AOUT*/ state->format != LD_FORMAT_MSDOS_MZ) {
                r_address -= state->text_size;
            }
            
            write741_to_byte_array ((unsigned char *) new_relocation.r_address, r_address);
            
            if (dgroup) {
                r_symbolnum = GET_UINT32 (r->r_symbolnum);
            } else {
                r_symbolnum = GET_UINT32 (r->r_symbolnum) & (3L << 25);
            }
            
            write741_to_byte_array (new_relocation.r_symbolnum, r_symbolnum);
            add_relocation (is_data ? &dgr : &tgr, &new_relocation);
        
        }
        
        if (opcode == 0x9A && symbolnum == 4) {
        
            uint32_t temp = *(int32_t *) ((char *) output + header_size + GET_INT32 (r->r_address));
            
            result = ((temp >> 16) & 0xffff) * 16;
            result += temp & 0xffff;
        
        } else if (result == 0) {
        
            int32_t r_address = GET_INT32 (r->r_address);
            
            if (length == 4) {
                result = *(int32_t *) ((char *) output + header_size + r_address);
            } else if (length == 2) {
                result = *(int16_t *) ((char *) output + header_size + r_address);
            } else if (length == 1) {
                result = *(int8_t *) ((char *) output + header_size + r_address);
            }
        
        }
        
        if (ext || dgroup) {
        
            if (ext) {
            
                symbolnum = (symbol->n_type & N_TYPE);
                result += GET_UINT32 (symbol->n_value);
                
                if (is_data && symbolnum == 4) {
                    result += state->code_offset;
                }
            
            }
        
        } else {
        
            if ((symbolnum == 6) || (symbolnum == 8)) {
            
                result -= GET_UINT32 (object->header->a_text);
                result += state->text_size;
            
            }
            
            if (symbolnum == 4) {
            
                result += objtextsize;
                result += state->code_offset;
            
            }
            
            if (symbolnum == 6) {
                result += objdatasize;
            }
            
            if (symbolnum == 8) {
            
                result -= GET_UINT32 (object->header->a_data);
                result += state->data_size;
                result += objbsssize;
            
            }
        
        }
    
    }
    
    if (opcode == 0x9A && symbolnum == 4) {
    
        int32_t i;
        
        if (result >= 65535) {
        
            int32_t r_address = GET_INT32 (r->r_address);
            
            if (state->format == LD_FORMAT_BINARY || state->format == LD_FORMAT_MSDOS) {
            
                report_at (object->filename, GET_INT32 (r->r_address), REPORT_ERROR, "call exceeds 65535 bytes");
                return 1;
            
            }
            
            number_to_chars (p, result % 16, 2);
            number_to_chars (p + 2, result / 16, 2);
            
            for (i = tgr.relocations_count - 1; i >= 0; --i) {
            
                if (GET_INT32 (tgr.relocations[i].r_address) == r_address) {
                    write741_to_byte_array ((unsigned char *) tgr.relocations[i].r_address, r_address + 2);
                }
            
            }
        
        } else {
        
            result -= header_size;
            
            /*if (state->format == LD_FORMAT_MSDOS_MZ) {*/
            
                int32_t r_address = GET_INT32 (r->r_address) + 1;
                write741_to_byte_array ((unsigned char *) r->r_address, r_address);
                
                *(p - 1) = 0x0E;
                
                for (i = tgr.relocations_count - 1; i >= 0; --i) {
                
                    if (GET_INT32 (tgr.relocations[i].r_address) + 1 == r_address) {
                    
                        write741_to_byte_array ((unsigned char *) tgr.relocations[i].r_address, r_address);
                        
                        result = fix_offset (tgr.relocations[i], result);
                        remove_relocation (&tgr, tgr.relocations[i]);
                    
                    }
                
                }
            
            /*}*/
            
            result -= GET_INT32 (r->r_address);
            while (length-- > 2) { result--; }
            
            number_to_chars (p + 1, result, 2);
            
            number_to_chars (p, 0xE8, 1);
            number_to_chars (p + 3, 0x90, 1);
        
        }
    
    } else if (dgroup) {
    
        result -= header_size;
        number_to_chars (p, result / 16, 2);
    
    } else {
    
        if (!_end && !_edata && state->format != LD_FORMAT_I386_AOUT) {
        
            int32_t data_addr = ((char *) data - (char *) output) - header_size;
            int32_t i, r_address = GET_INT32 (r->r_address);
            
            if (is_data && (symbolnum == 6 || symbolnum == 8)) {
            
                int32_t temp = data_addr;
                
                temp += state->code_offset;
                number_to_chars (p + 2, temp / 16, 2);
                
                if (result >= data_addr) {
                    result -= (data_addr & 0xfffffff0);
                }
                
                length = 2;
            
            } else {
            
                if ((symbolnum == 6 || symbolnum == 8) && result >= data_addr) {
                    result -= (data_addr & 0xfffffff0);
                }
                
                for (i = tgr.relocations_count - 1; i >= 0; --i) {
                
                    if (GET_INT32 (tgr.relocations[i].r_address) == r_address) {
                    
                        result = fix_offset (tgr.relocations[i], result);
                        remove_relocation (&tgr, tgr.relocations[i]);
                    
                    }
                
                }
            
            }
        
        }
        
        number_to_chars (p, result, length);
    
    }
    
    return 0;

}

static int glue (struct aout_object *object) {

    int32_t i, err = 0;
    
    for (i = 0; i < object->trelocs_count; i++) {
    
        if (relocate (object, &object->trelocs[i], 0)) {
            err = 1;
        }
    
    }
    
    for (i = 0; i < object->drelocs_count; i++) {
    
        if (relocate (object, &object->drelocs[i], 1)) {
            err = 1;
        }
    
    }
    
    objtextsize += GET_UINT32 (object->header->a_text);
    objdatasize += GET_UINT32 (object->header->a_data);
    objbsssize  += GET_UINT32 (object->header->a_bss);
    
    return err;

}


static int init_aout_object (void) {

    header_size = sizeof (*aout_hdr);
    
    if (!state->impure) {
        header_size = ALIGN_UP (header_size, SECTION_ALIGNMENT);
    }
    
    output_size = header_size + state->text_size + state->data_size;
    
    if ((output = malloc (output_size)) == NULL) {
        return 1;
    }
    
    memset (output, 0, output_size);
    aout_hdr = output;
    
    text = (void *) ((char *) output + header_size);
    data = (void *) ((char *) text + state->text_size);
    
    return 0;

}

static int write_aout_object (FILE *ofp, uint32_t a_entry) {

    write741_to_byte_array (aout_hdr->a_info, state->impure ? OMAGIC : ZMAGIC);
    write741_to_byte_array (aout_hdr->a_text, state->text_size);
    write741_to_byte_array (aout_hdr->a_data, state->data_size);
    write741_to_byte_array (aout_hdr->a_bss, state->bss_size);
    write741_to_byte_array (aout_hdr->a_entry, a_entry);
    write741_to_byte_array (aout_hdr->a_trsize, tgr.relocations_count * sizeof (struct relocation_info));
    write741_to_byte_array (aout_hdr->a_drsize, dgr.relocations_count * sizeof (struct relocation_info));
    
    if (fwrite ((char *) output, output_size, 1, ofp) != 1) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to write data to '%s'", state->outfile);
        return 1;
    
    }
    
    if (tgr.relocations_count > 0) {
    
        if (fwrite (tgr.relocations, tgr.relocations_count * sizeof (struct relocation_info), 1, ofp) != 1) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to write text relocations to '%s'", state->outfile);
            return 1;
        
        }
    
    }
    
    if (dgr.relocations_count > 0) {
    
        if (fwrite (dgr.relocations, dgr.relocations_count * sizeof (struct relocation_info), 1, ofp) != 1) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to write data relocations to '%s'", state->outfile);
            return 1;
        
        }
    
    }
    
    return 0;

}

static int init_msdos_mz_object (void) {

    state->text_size = ALIGN_UP (state->text_size, 16);
    state->data_size = ALIGN_UP (state->data_size, 16);
    
    header_size = ALIGN_UP (sizeof (*msdos_hdr), 16);
    output_size = header_size + state->text_size + state->data_size;
    
    if ((output = malloc (output_size)) == NULL) {
        return 1;
    }
    
    memset (output, 0, output_size);
    msdos_hdr = output;
    
    text = (void *) ((char *) output + header_size);
    data = (void *) ((char *) text + state->text_size);
    
    return 0;

}

static int write_msdos_mz_object (FILE *ofp, uint32_t entry) {

    /*unsigned short ibss_addr = ((char *) output - (char *) text) + state->text_size;*/
    unsigned long ibss_addr = output_size;
    unsigned long ibss_size = state->bss_size;
    
    unsigned long stack_addr, stack_size;
    
    uint32_t reloc_sz = 0, offset = 0;
    int32_t i;
    
    for (i = tgr.relocations_count - 1; i >= 0; --i) {
    
        struct relocation_info *r = &tgr.relocations[i];
        
        if (((GET_UINT32 (r->r_symbolnum) >> 24) & 0xff) == N_ABS) {
            remove_relocation (&tgr, *r);
        }
    
    }
    
    reloc_sz = ALIGN_UP (tgr.relocations_count + dgr.relocations_count * 4, 32);
    ibss_addr += reloc_sz;
    
    stack_addr = ibss_addr + ibss_size;
    stack_size = state->stack_size;
    
    msdos_hdr->e_magic[0] = 'M';
    msdos_hdr->e_magic[1] = 'Z';
    
    write721_to_byte_array (msdos_hdr->e_cblp, (ibss_addr % 512));
    write721_to_byte_array (msdos_hdr->e_cp, ALIGN_UP (ibss_addr, 512) / 512);
    
    write721_to_byte_array (msdos_hdr->e_crlc, tgr.relocations_count + dgr.relocations_count);
    write721_to_byte_array (msdos_hdr->e_cparhdr, ((header_size + reloc_sz) / 16));
    
    write721_to_byte_array (msdos_hdr->e_minalloc, (ALIGN_UP (ibss_size + stack_size, 16) / 16));
    write721_to_byte_array (msdos_hdr->e_maxalloc, 0xFFFF);
    
    write721_to_byte_array (msdos_hdr->e_ss, (stack_addr / 16));
    write721_to_byte_array (msdos_hdr->e_sp, ALIGN_UP (stack_addr % 16 + stack_size, 16));
    
    write721_to_byte_array (msdos_hdr->e_ip, entry % 16);
    write721_to_byte_array (msdos_hdr->e_cs, entry / 16);
    
    write721_to_byte_array (msdos_hdr->e_lfarlc, header_size);
    
    if (tgr.relocations_count > 0 || dgr.relocations_count > 0) {
    
        char *relocs;
        void *temp;
        
        if ((temp = malloc (output_size + reloc_sz)) == NULL) {
            return 1;
        }
        
        memcpy ((char *) temp, (char *) output, header_size);
        memcpy ((char *) temp + header_size + reloc_sz, (char *) output + header_size, output_size - header_size);
        
        free (output);
        output = temp;
        
        relocs = (char *) output + header_size;
        
        text = (char *) output + header_size + reloc_sz;
        data = (char *) text + state->text_size;
        
        memset (relocs, 0, reloc_sz);
        output_size += reloc_sz;
        
        for (i = 0; i < tgr.relocations_count; ++i) {
        
            struct relocation_info *r = &tgr.relocations[i];
            int32_t r_address = GET_INT32 (r->r_address);
            
            number_to_chars ((unsigned char *) relocs + offset, r_address % 16, 2);
            number_to_chars ((unsigned char *) relocs + offset + 2, r_address / 16, 2);
            
            offset += 4;
        
        }
        
        for (i = 0; i < dgr.relocations_count; ++i) {
        
            struct relocation_info *r = &dgr.relocations[i];
            
            int32_t data_addr = ((char *) data - (char *) output) - header_size - reloc_sz;
            int32_t r_address = GET_INT32 (r->r_address) + 2;
            
            number_to_chars ((unsigned char *) relocs + offset, r_address % 16, 2);
            number_to_chars ((unsigned char *) relocs + offset + 2, r_address / 16, 2);
            
            number_to_chars ((unsigned char *) text + r_address, data_addr / 16, 2);
            offset += 4;
        
        }
    
    }
    
    if (fwrite ((char *) output, output_size, 1, ofp) != 1) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to write data to '%s'", state->outfile);
        return 1;
    
    }
    
    return 0;

}


int create_executable_from_aout_objects (void) {

    struct aout_object *object;
    FILE *ofp;
    
    int err = 0;
    long i;
    
    uint32_t a_entry = 0;
    
    if (state->format == LD_FORMAT_I386_AOUT) {
    
        if ((err = init_aout_object ())) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to initialize a.out object");
            return err;
        
        }
    
    } else if (state->format == LD_FORMAT_BINARY || state->format == LD_FORMAT_MSDOS) {
    
        /*if (state->format == LD_FORMAT_MSDOS) {
        
            state->text_size = ALIGN_UP (state->text_size, 16);
            state->data_size = ALIGN_UP (state->data_size, 16);
        
        }*/
        
        /*state->text_size = ALIGN_UP (state->text_size, 16);
        state->data_size = ALIGN_UP (state->data_size, 16);*/
        
        output_size = state->text_size + state->data_size;
        
        if (state->format == LD_FORMAT_BINARY && state->include_bss) {
            output_size += state->bss_size;
        }
        
        if ((output = malloc (output_size)) == NULL) {
            return EXIT_FAILURE;
        }
        
        memset (output, 0, output_size);
        
        text = (void *) (char *) output;
        data = (void *) ((char *) text + state->text_size);
    
    } else if (state->format == LD_FORMAT_MSDOS_MZ) {
    
        if ((err = init_msdos_mz_object ())) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to initialize msdos object");
            return err;
        
        }
    
    }
    
    for (i = 0; i < state->nb_aout_objs; ++i) {
        paste (state->aout_objs[i]);
    }
    
    for (i = 0; i < state->nb_aout_objs; ++i) {
        apply_slides (state->aout_objs[i]);
    }
    
    for (i = 0; i < state->nb_aout_objs; ++i) {
        undef_collect (state->aout_objs[i]);
    }
    
    if (!state->impure) {
    
        if (state->format == LD_FORMAT_I386_PE) {
            state->bss_size = ALIGN_UP (state->bss_size, FILE_ALIGNMENT);
        } else {
            state->bss_size = ALIGN_UP (state->bss_size, SECTION_ALIGNMENT);
        }
    
    }
    
    for (i = 0; i < state->nb_aout_objs; ++i) {
    
        if (glue (state->aout_objs[i])) {
            err = 1;
        }
    
    }
    
    if (err) { return EXIT_FAILURE; }
    
    if (state->format == LD_FORMAT_I386_AOUT || state->format == LD_FORMAT_MSDOS_MZ) {
        a_entry = get_entry ();
    }
    
    for (i = 0; i < state->nb_aout_objs; i++) {
    
        if ((object = state->aout_objs[i]) == NULL) {
            return EXIT_FAILURE;
        }
        
        if (state->mapfile) {
            init_map (object);
        }
        
        free (object->raw);
        free (object);
    
    }
    
    state->nb_aout_objs = 0;
    
    if (state->mapfile) {
    
        set_map_sections_size (state->text_size, state->data_size, state->bss_size);
        set_map_sections_start (0, state->text_size, state->text_size + state->data_size);
        
        generate_map ();
    
    }
    
    if ((ofp = fopen (state->outfile, "wb")) == NULL) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to open '%s' for writing", state->outfile);
        return EXIT_FAILURE;
    
    }
    
    if (state->format == LD_FORMAT_I386_AOUT) {
    
        if ((err = write_aout_object (ofp, a_entry))) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to write a.out object");
            fclose (ofp);
            
            return err;
        
        }
    
    } else if (state->format == LD_FORMAT_BINARY || state->format == LD_FORMAT_MSDOS) {
    
        fix_offsets ();
        
        if (fwrite ((char *) output, output_size, 1, ofp) != 1) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to write data to '%s'", state->outfile);
            fclose (ofp);
            
            return EXIT_FAILURE;
        
        }
    
    } else if (state->format == LD_FORMAT_MSDOS_MZ) {
    
        /*fix_offsets ();*/
        
        if ((err = write_msdos_mz_object (ofp, a_entry))) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to write msdos object");
            fclose (ofp);
            
            return err;
        
        }
    
    }
    
    fclose (ofp);
    return EXIT_SUCCESS;

}
