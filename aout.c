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

static struct aout_exec *aout_hdr;
static struct msdos_header *msdos_hdr;

static size_t header_size = 0, output_size = 0;
static void *data = 0, *output = 0, *text = 0;


struct gr {

    int relocations_count;
    int relocations_max;
    
    struct relocation_info *relocations;

};

static struct gr tgr = { 0, 64, NULL };
static struct gr dgr = { 0, 64, NULL };


static int get_symbol (struct aout_object **obj_out, int *index, const char *name, int quiet) {

    size_t object_i;
    int symbol_i;
    
    for (object_i = 0; object_i < state->nb_aout_objs; ++object_i) {
    
        struct aout_object *obj = state->aout_objs[object_i];
        
        for (symbol_i = 0; symbol_i < obj->symtab_count; symbol_i++) {
        
            struct nlist *sym = &obj->symtab[symbol_i];
            char *symname = obj->strtab + sym->n_strx;
            
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

static unsigned int get_entry (void) {

    if (!state->entry || strcmp (state->entry, "") == 0) {
        state->entry = "_start";
    }
    
    struct aout_object *symobj;
    int symidx;
    
    if (get_symbol (&symobj, &symidx, state->entry, 1)) {
    
        report_at (program_name, 0, REPORT_WARNING, "cannot find entry symbol %s; defaulting to 00000000", state->entry);
        return 0;
    
    }
    
    return symobj->symtab[symidx].n_value;

}

static void number_to_chars (unsigned char *p, unsigned long number, unsigned long size) {
    
    unsigned long i;
    
    for (i = 0; i < size; i++) {
        p[i] = (number >> (8 * i)) & 0xff;
    }

}

static void fix_offset (struct relocation_info r) {

    unsigned int zapdata = (text - output);
    unsigned int orig;
    
    unsigned char *p;
    int length;
    
    if (((r.r_symbolnum >> 24) & 0xff) != N_TEXT) {
        return;
    }
    
    p = text + r.r_address;
    length = (r.r_symbolnum & (3 << 25)) >> 25;
    
    memcpy (&orig, p, length);
    orig += zapdata;
    
    if (state->format == LD_FORMAT_I386_PE) {
    
        unsigned int data_offset = (data - output);
        
        if (orig >= data_offset) {
        
            orig += ALIGN_UP (zapdata, SECTION_ALIGNMENT);
            orig += ALIGN_UP (state->text_size, SECTION_ALIGNMENT);
            
            orig -= zapdata;
        
        } else {
            orig += ALIGN_UP (zapdata, SECTION_ALIGNMENT);
        }
        
        orig -= zapdata;
    
    }
    
    memcpy (p, &orig, length);

}

static void fix_offsets (void) {

    unsigned int zapdata = (text - output);
    unsigned char *p;
    
    int i, length;
    
    for (i = 0; i < tgr.relocations_count; i++) {
    
        struct relocation_info *r = &tgr.relocations[i];
        unsigned int orig = 0;
        
        if (((r->r_symbolnum >> 24) & 0xff) != N_TEXT) {
            continue;
        }
        
        p = text + r->r_address;
        length = (r->r_symbolnum & (3 << 25)) >> 25;
        
        if (*(p - 1) == 0x9A) {
        
            unsigned int temp = *(int *) p;
            
            orig = ((temp >> 16) & 0xffff) * 16;
            orig += temp & 0xffff;
        
        } else {
            memcpy (&orig, p, length);
        }
        
        orig += zapdata;
        
        if (state->format == LD_FORMAT_I386_PE) {
        
            unsigned int data_offset = (data - output);
            
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
        unsigned int orig = 0;
        
        if (((r->r_symbolnum >> 24) & 0xff) != N_TEXT) {
            continue;
        }
        
        length = (r->r_symbolnum & (3 << 25)) >> 25;
        
        memcpy (&orig, ((char *) data + r->r_address), length);
        orig += zapdata;
        
        if (state->format == LD_FORMAT_I386_PE) {
        
            unsigned int data_offset = (data - output);
            
            if (orig >= data_offset) {
            
                orig += ALIGN_UP (zapdata, SECTION_ALIGNMENT);
                orig += ALIGN_UP (state->text_size, SECTION_ALIGNMENT);
                
                orig -= zapdata;
            
            } else {
                orig += ALIGN_UP (zapdata, SECTION_ALIGNMENT);
            }
            
            orig -= zapdata;
        
        }
        
        memcpy ((char *) data + r->r_address, &orig, length);
    
    }

}


static int objtextsize = 0, objdatasize = 0, objbsssize = 0;
static unsigned int text_ptr = 0, data_ptr = 0, bss_ptr = 0;

static void apply_slides (struct aout_object *object) {

    int i;
    
    for (i = 0; i < object->symtab_count; i++) {
    
        struct nlist *symbol = &object->symtab[i];
        unsigned int final_slide = 0;
        
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
        
        symbol->n_value += final_slide;
        
        switch (symbol->n_type & N_TYPE) {
        
            case N_BSS:
            
                symbol->n_value -= object->header->a_data;
                /* fall through */
            
            case N_DATA:
            
                symbol->n_value -= object->header->a_text;
                break;
        
        }
    
    }
    
    for (i = 0; i < object->trelocs_count; i++) {
    
        struct relocation_info *rel = &object->trelocs[i];
        rel->r_address += object->text_slide;
    
    }
    
    for (i = 0; i < object->drelocs_count; i++) {
    
        struct relocation_info *rel = &object->drelocs[i];
        rel->r_address += state->text_size + object->data_slide;
    
    }

}

static void init_map (struct aout_object *object) {

    int symbol_i;
    
    add_map_object (object->filename, object->header->a_bss, object->header->a_data, object->header->a_text);
    
    for (symbol_i = 0; symbol_i < object->symtab_count; symbol_i++) {
    
        struct nlist *sym = &object->symtab[symbol_i];
        char *symname = object->strtab + sym->n_strx;
        
        if ((sym->n_type & N_TYPE) == N_UNDF) {
            continue;
        }
        
        if (strcmp (symname, ".text") == 0 || strcmp (symname, ".data") == 0 || strcmp (symname, ".bss") == 0) {
            continue;
        }
        
        if ((sym->n_type & N_TYPE) == N_TEXT) {
            add_map_text_symbol (object->filename, symname, sym->n_value);
        } else if ((sym->n_type & N_TYPE) == N_DATA) {
            add_map_data_symbol (object->filename, symname, sym->n_value);
        } else if ((sym->n_type & N_TYPE) == N_BSS) {
            add_map_bss_symbol (object->filename, symname, sym->n_value);
        }
    
    }

}

static void paste (struct aout_object *object) {

    struct aout_exec *header = object->header;
    
    char *obj_text, *obj_data;
    unsigned int obj_text_size, obj_data_size, obj_bss_size;
    
    object->text_slide = text_ptr;
    obj_text = (char *) object->raw + sizeof (*header);
    
    if (state->impure) {
        obj_text_size = header->a_text;
    } else {
    
        if (state->format == LD_FORMAT_I386_PE) {
            obj_text_size = ALIGN_UP (header->a_text, FILE_ALIGNMENT);
        } else {
            obj_text_size = ALIGN_UP (header->a_text, PAGE_SIZE);
        }
    
    }
    
    memcpy ((char *) text + text_ptr, obj_text, header->a_text);
    text_ptr += obj_text_size;
    
    object->data_slide = data_ptr;
    obj_data = (char *) object->raw + sizeof (*header) + header->a_text;
    
    if (state->impure) {
        obj_data_size = header->a_data;
    } else {
    
        if (state->format == LD_FORMAT_I386_PE) {
            obj_data_size = ALIGN_UP (header->a_data, FILE_ALIGNMENT);
        } else {
            obj_data_size = ALIGN_UP (header->a_data, PAGE_SIZE);
        }
    
    }
    
    memcpy ((char *) data + data_ptr, obj_data, header->a_data);
    data_ptr += obj_data_size;
    
    object->bss_slide = bss_ptr;
    
    if (state->impure) {
        obj_bss_size = header->a_bss;
    } else {
    
        if (state->format == LD_FORMAT_I386_PE) {
            obj_bss_size = ALIGN_UP (header->a_bss, FILE_ALIGNMENT);
        } else {
            obj_bss_size = ALIGN_UP (header->a_bss, PAGE_SIZE);
        }
    
    }
    
    bss_ptr += obj_bss_size;

}

static void undef_collect (struct aout_object *object) {

    int i, val;
    
    for (i = 0; i < object->symtab_count; i++) {
    
        struct nlist *sym = &object->symtab[i];
        char *symname = object->strtab + sym->n_strx;
        
        if ((sym->n_type & N_TYPE) != N_UNDF || sym->n_value == 0) {
            continue;
        }
        
        if (get_symbol (NULL, NULL, symname, 1)) {
            continue;
        }
        
        sym->n_type = N_BSS | N_EXT;
        val = sym->n_value;
        
        sym->n_value = state->text_size + state->data_size + state->bss_size;
        state->bss_size += val;
    
    }

}

static int remove_relocation (struct gr *gr, struct relocation_info r) {

    struct relocation_info *relocs;
    int i, j;
    
    if (gr->relocations == NULL || gr->relocations_count == 0) {
        return 0;
    }
    
    if ((relocs = malloc (gr->relocations_max * sizeof (*relocs))) == NULL) {
        return 1;
    }
    
    for (i = 0, j = 0; i < gr->relocations_count; ++i) {
    
        if (gr->relocations[i].r_address == r.r_address && gr->relocations[i].r_symbolnum == r.r_symbolnum) {
            continue;
        }
        
        memcpy (&relocs[j++], &gr->relocations[i], sizeof (*relocs));
    
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

static int relocate (struct aout_object *object, struct relocation_info *r, int offset, int is_data) {

    struct nlist *symbol;
    int result = 0, opcode;
    
    unsigned char *p;
    int dgroup = 0;
    
    int symbolnum = r->r_symbolnum & 0xffffff;
    int pcrel = (r->r_symbolnum & (1 << 24)) >> 24;
    int ext = (r->r_symbolnum & (1 << 27)) >> 27;
    int baserel = (r->r_symbolnum & (1 << 28)) >> 28;
    int jmptable = (r->r_symbolnum & (1 << 29)) >> 29;
    int rel = (r->r_symbolnum & (1 << 30)) >> 30;
    int copy = (r->r_symbolnum & (1 << 31)) >> 31;

    int length = (r->r_symbolnum & (3 << 25)) >> 25;
    
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
    
    if ((is_data && pcrel) || baserel || jmptable || rel || copy) {
    
        report_at (program_name, 0, REPORT_ERROR, "unsupported relocation type");
        return 1;
    
    }
    
    p = (unsigned char *) output + header_size + r->r_address;
    opcode = *(int *) (p - 1) & 0xff;
    
    symbol = &object->symtab[symbolnum];
    
    if (ext) {
    
        char *symname = object->strtab + symbol->n_strx;
        
        struct aout_object *symobj;
        int symidx;
        
        if (strcmp (symname, "DGROUP") == 0) {
        
            symbol->n_value = (data - output);
            ext = 0;
            
            dgroup = 1;
        
        } else if (!get_symbol (&symobj, &symidx, symname, 0)) {
            symbol = &symobj->symtab[symidx];
        } else {
            return 1;
        }
    
    }
    
    if (pcrel) {
        result = (int) symbol->n_value - (r->r_address + length);
    } else {
    
        if (dgroup || !ext || (symbol->n_type & N_TYPE) == N_BSS || (symbol->n_type & N_TYPE) == N_DATA || (symbol->n_type & N_TYPE) == N_TEXT) {
        
            struct relocation_info new_relocation;
            new_relocation.r_address = r->r_address;
            
            if (is_data) {
                new_relocation.r_address -= state->text_size;
            }
            
            new_relocation.r_symbolnum = r->r_symbolnum & (3 << 25);
            add_relocation (is_data ? &dgr : &tgr, &new_relocation);
        
        }
        
        if (opcode == 0x9A) {
        
            unsigned int temp = *(int *) ((char *) output + header_size + r->r_address);
            
            result = ((temp >> 16) & 0xffff) * 16;
            result += temp & 0xffff;
        
        } else {
            result = *(int *) ((char *) output + header_size + r->r_address);
        }
        
        result += offset;
        
        if (ext || dgroup) {
            result += symbol->n_value;
        } else {
        
            if ((symbolnum == 6) || (symbolnum == 8)) {
            
                result -= object->header->a_text;
                result += state->text_size;
            
            }
            
            if (symbolnum == 4) {
                result += objtextsize;
            }
            
            if (symbolnum == 6) {
                result += objdatasize;
            }
            
            if (symbolnum == 8) {
            
                result -= object->header->a_data;
                result += state->data_size;
                result += objbsssize;
            
            }
        
        }
    
    }
    
    if (opcode == 0x9A) {
    
        int i;
        result -= offset;
        
        /*if (result >= 32767) {*/
        if (result >= 65535) {
        
            if (state->format == LD_FORMAT_BINARY || state->format == LD_FORMAT_MSDOS) {
            
                report_at (object->filename, r->r_address, REPORT_ERROR, "call exceeds 65535 bytes");
                return 1;
            
            }
            
            number_to_chars (p, result % 16, 2);
            number_to_chars (p + 2, result / 16, 2);
            
            for (i = 0; i < tgr.relocations_count; ++i) {
            
                if (tgr.relocations[i].r_address == r->r_address) {
                    tgr.relocations[i].r_address += 2;
                }
            
            }
        
        } else {
        
            result -= header_size;
            
            if (state->format == LD_FORMAT_MSDOS_MZ) {
            
                *(p - 1) = 0x0E;
                
                p++;
                r->r_address++;
            
            }
            
            number_to_chars (p, result - r->r_address - 2, 2);
            *(p - 1) = 0xE8;
            
            memset (p + 2, 0x90, 1);
            
            for (i = 0; i < tgr.relocations_count; ++i) {
            
                if (tgr.relocations[i].r_address + 1 == r->r_address) {
                
                    tgr.relocations[i].r_address++;
                    
                    fix_offset (tgr.relocations[i]);
                    remove_relocation (&tgr, tgr.relocations[i]);
                
                }
            
            }
        
        }
    
    } else if (dgroup) {
    
        result -= header_size;
        number_to_chars (p, (result & 0xffff) / 16, 2);
    
    } else {
    
        if (state->format == LD_FORMAT_MSDOS_MZ) {
        
            unsigned int data_addr = (unsigned int) (data - output);
            result &= 0xffff;
            
            if ((result - offset) + state->text_size >= data_addr) {
            
                int i;
                result -= state->text_size;
                
                for (i = 0; i < tgr.relocations_count; ++i) {
                
                    if (tgr.relocations[i].r_address == r->r_address) {
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

    int i, err = 0;
    
    for (i = 0; i < object->trelocs_count; i++) {
    
        if (relocate (object, &object->trelocs[i], state->code_offset, 0)) {
            err = 1;
        }
    
    }
    
    for (i = 0; i < object->drelocs_count; i++) {
    
        if (relocate (object, &object->drelocs[i], state->code_offset, 1)) {
            err = 1;
        }
    
    }
    
    objtextsize += object->header->a_text;
    objdatasize += object->header->a_data;
    objbsssize  += object->header->a_bss;
    
    return err;

}


static int init_aout_object (void) {

    header_size = sizeof (*aout_hdr);
    
    if (!state->impure) {
        header_size = ALIGN_UP (header_size, PAGE_SIZE);
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

static int write_aout_object (FILE *ofp, unsigned int a_entry) {

    aout_hdr->a_info = state->impure ? OMAGIC : ZMAGIC;
    aout_hdr->a_text = state->text_size;
    aout_hdr->a_data = state->data_size;
    aout_hdr->a_bss = state->bss_size;
    aout_hdr->a_entry = a_entry;
    aout_hdr->a_trsize = tgr.relocations_count * sizeof (struct relocation_info);
    aout_hdr->a_drsize = dgr.relocations_count * sizeof (struct relocation_info);
    
    if (fwrite ((char *) output, output_size, 1, ofp) != 1) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to write data to '%s'", state->outfile);
        return 1;
    
    }
    
    if (tgr.relocations_count > 0) {
    
        if (fwrite (tgr.relocations, tgr.relocations_count * sizeof (struct relocation_info), 1, ofp) != 1) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to write data to '%s'", state->outfile);
            return 1;
        
        }
    
    }
    
    if (dgr.relocations_count > 0) {
    
        if (fwrite (dgr.relocations, dgr.relocations_count * sizeof (struct relocation_info), 1, ofp) != 1) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to write data to '%s'", state->outfile);
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

static int write_msdos_mz_object (FILE *ofp, unsigned int entry) {

    /*unsigned short ibss_addr = ((char *) output - (char *) text) + state->text_size;*/
    size_t ibss_addr = output_size;
    size_t ibss_size = state->bss_size;
    
    size_t stack_addr, stack_size;
    
    unsigned int reloc_sz = 0;
    int i;
    
    /*for (i = 0; i < tgr.relocations_count; ++i) {
    
        struct relocation_info *r = &tgr.relocations[i];
        
        if (((r->r_symbolnum >> 24) & 0xff) == N_ABS) {
            remove_relocation (&tgr, *r);
        }
    
    }*/
    
    reloc_sz = ALIGN_UP (tgr.relocations_count * 4, 16);
    ibss_addr += reloc_sz;
    
    stack_addr = ibss_addr + ibss_size;
    stack_size = state->stack_size;
    
    msdos_hdr->e_magic[0] = 'M';
    msdos_hdr->e_magic[1] = 'Z';
    
    msdos_hdr->e_cblp = (ibss_addr % 512);
    msdos_hdr->e_cp = ALIGN_UP (ibss_addr, 512) / 512;
    
    msdos_hdr->e_crlc = tgr.relocations_count;
    msdos_hdr->e_cparhdr = ((header_size + reloc_sz) / 16);
    
    msdos_hdr->e_minalloc = (ALIGN_UP (ibss_size + stack_size, 16) / 16);
    msdos_hdr->e_maxalloc = 0xFFFF;
    
    msdos_hdr->e_ss = (stack_addr / 16);
    msdos_hdr->e_sp = (stack_addr % 16 + stack_size);
    
    msdos_hdr->e_ip = entry % 16;
    msdos_hdr->e_cs = entry / 16;
    
    msdos_hdr->e_lfarlc = header_size;
    
    if (tgr.relocations_count > 0) {
    
        unsigned char *temp = xmalloc (output_size);
        char *relocs;
        
        memcpy (temp, (char *) output + header_size, output_size - header_size);
        
        output = xrealloc (output, output_size + reloc_sz);
        memcpy ((char *) output + header_size + reloc_sz, temp, output_size - header_size);
        
        free (temp);
        
        relocs = (char *) output + header_size;
        text = (char *) output + header_size + reloc_sz;
        
        memset (relocs, 0, reloc_sz);
        output_size += reloc_sz;
        
        for (i = 0; i < tgr.relocations_count; ++i) {
        
            unsigned int offset = (i * 4);
            
            number_to_chars ((unsigned char *) relocs + offset, tgr.relocations[i].r_address % 16, 2);
            number_to_chars ((unsigned char *) relocs + offset + 2, tgr.relocations[i].r_address / 16, 2);
        
        }
    
    }
    
    if (fwrite ((char *) output, output_size, 1, ofp) != 1) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to write data to '%s'", state->outfile);
        return 1;
    
    }
    
    return 0;

}


int create_executable_from_aout_objects (void) {

    FILE *ofp;
    size_t i;
    
    int err = 0;
    
    if (state->format == LD_FORMAT_I386_AOUT) {
    
        if ((err = init_aout_object ())) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to initialize a.out object");
            return err;
        
        }
    
    } else if (state->format == LD_FORMAT_BINARY || state->format == LD_FORMAT_MSDOS) {
    
        if (state->format == LD_FORMAT_MSDOS) {
        
            state->text_size = ALIGN_UP (state->text_size, 16);
            state->data_size = ALIGN_UP (state->data_size, 16);
        
        }
        
        output_size = state->text_size + state->data_size;
        
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
            state->bss_size = ALIGN_UP (state->bss_size, PAGE_SIZE);
        }
    
    }
    
    for (i = 0; i < state->nb_aout_objs; ++i) {
    
        if (glue (state->aout_objs[i])) {
            err = 1;
        }
    
    }
    
    if (err) { return EXIT_FAILURE; }
    
    for (i = 0; i < state->nb_aout_objs; ++i) {
        init_map (state->aout_objs[i]);
    }
    
    set_map_sections_size (state->text_size, state->data_size, state->bss_size);
    set_map_sections_start (0, state->text_size, state->text_size + state->data_size);
    
    generate_map ();
    
    if ((ofp = fopen (state->outfile, "wb")) == NULL) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to open '%s' for writing", state->outfile);
        return EXIT_FAILURE;
    
    }
    
    if (state->format == LD_FORMAT_I386_AOUT) {
    
        unsigned int a_entry = get_entry ();
        
        if ((err = write_aout_object (ofp, a_entry))) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to write a.out object");
            fclose (ofp);
            
            return err;
        
        }
    
    } else if (state->format == LD_FORMAT_BINARY || state->format == LD_FORMAT_MSDOS) {
    
        fix_offsets ();
        
        if (fwrite ((char *) output, state->text_size + state->data_size, 1, ofp) != 1) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to write data to '%s'", state->outfile);
            fclose (ofp);
            
            return EXIT_FAILURE;
        
        }
    
    } else if (state->format == LD_FORMAT_MSDOS_MZ) {
    
        /*fix_offsets ();*/
        
        if ((err = write_msdos_mz_object (ofp, get_entry ()))) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to write msdos object");
            fclose (ofp);
            
            return err;
        
        }
    
    }
    
    fclose (ofp);
    return EXIT_SUCCESS;

}
