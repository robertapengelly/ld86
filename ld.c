/******************************************************************************
 * @file            ld.c
 *****************************************************************************/
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>

#include    "aout.h"
#include    "ar.h"
#include    "coff.h"
#include    "ld.h"
#include    "lib.h"
#include    "report.h"
#include    "types.h"

struct ld_state *state = 0;
const char *program_name = 0;

static uint32_t conv_dec (char *str, int32_t max) {

    uint32_t value = 0;
    
    while (*str != ' ' && max-- > 0) {
    
        value *= 10;
        value += *str++ - '0';
    
    }
    
    return value;

}

/*static int process_coff (void *obj, unsigned long sz, const char *fname, int quiet) {

    struct coff_header *hdr = obj;
    
    struct coff_object *data_obj;
    unsigned long i;
    
    if (hdr->Machine != IMAGE_FILE_MACHINE_I386) {
    
        if (!quiet) {
            report_at (program_name, 0, REPORT_ERROR, "'%s' is not a valid coff object", fname);
        }
        
        return 1;
    
    }
    
    for (i = 0; i < state->nb_coff_objs; ++i) {
    
        struct coff_object *obj_to_compare = state->coff_objs[i];
        
        if (obj_to_compare->size != sz) {
            continue;
        }
        
        if (memcmp (obj_to_compare->raw, obj, sz) == 0) {
            return 0;
        }
    
    }
    
    {
        unsigned int thiscpu = (hdr->Machine & 0xff) + 256 * ((hdr->Machine >> 8) & 0xff);
        report_at (__FILE__, __LINE__, REPORT_INTERNAL_ERROR, "thiscpu: %x", thiscpu);
        
        unsigned int symptr = hdr->PointerToSymbolTable;
        unsigned int numsyms = hdr->NumberOfSymbols;
        unsigned int hdr_sz = sizeof (*hdr);
        
        if (hdr->SizeOfOptionalHeader) {
        
            report_at (program_name, 0, REPORT_WARNING, "optional header discarded");
            hdr_sz += hdr->SizeOfOptionalHeader;
        
        }
        
        report_at (program_name, 0, REPORT_INTERNAL_ERROR, "header_size: 0x%x (%u)", hdr_sz, hdr_sz);
        report_at (program_name, 0, REPORT_INTERNAL_ERROR, "symptr: 0x%x (%u)", symptr, symptr);
        report_at (program_name, 0, REPORT_INTERNAL_ERROR, "num_syms: 0x%x (%u)", numsyms, numsyms);
    }
    
    if ((data_obj = malloc (sizeof (*data_obj))) == NULL) {
        return 1;
    }
    
    memset (data_obj, 0, sizeof (*data_obj));
    
    data_obj->filename = fname;
    data_obj->header = hdr;
    data_obj->raw = obj;
    data_obj->size = sz;
    
    dynarray_add (&state->coff_objs, &state->nb_coff_objs, data_obj);
    return 0;

}*/

static int process_aout (void *obj, unsigned long sz, const char *fname, int quiet) {

    struct aout_exec *hdr = obj;
    
    struct aout_object *data_obj;
    struct nlist *symtab;
    
    struct relocation_info *trelocs;
    struct relocation_info *drelocs;
    
    int32_t symtab_count, trelocs_count, drelocs_count;
    uint32_t symtab_off, strtab_off, trelocs_off, drelocs_off;
    
    char *strtab;
    long i;
    
    if ((GET_UINT32 (hdr->a_info) & 0xffff) != OMAGIC) {
    
        if (!quiet) {
            report_at (program_name, 0, REPORT_ERROR, "'%s' is not a valid a.out object", fname);
        }
        
        return 1;
    
    }
    
    for (i = 0; i < state->nb_aout_objs; ++i) {
    
        struct aout_object *obj_to_compare = state->aout_objs[i];
        
        if (obj_to_compare->size != sz) {
            continue;
        }
        
        if (memcmp (obj_to_compare->raw, obj, sz) == 0) {
            return 0;
        }
    
    }
    
    if (state->impure) {
    
        state->text_size    += GET_UINT32 (hdr->a_text);
        state->data_size    += GET_UINT32 (hdr->a_data);
        state->bss_size     += GET_UINT32 (hdr->a_bss);
    
    } else {
    
        
        if (state->format == LD_FORMAT_I386_PE) {
        
            state->text_size    += ALIGN_UP (GET_UINT32 (hdr->a_text),   FILE_ALIGNMENT);
            state->data_size    += ALIGN_UP (GET_UINT32 (hdr->a_data),   FILE_ALIGNMENT);
            state->bss_size     += ALIGN_UP (GET_UINT32 (hdr->a_bss),    FILE_ALIGNMENT);
        
        } else {
        
            state->text_size    += ALIGN_UP (GET_UINT32 (hdr->a_text),   SECTION_ALIGNMENT);
            state->data_size    += ALIGN_UP (GET_UINT32 (hdr->a_data),   SECTION_ALIGNMENT);
            state->bss_size     += ALIGN_UP (GET_UINT32 (hdr->a_bss),    SECTION_ALIGNMENT);
        
        }
    
    }
    
    trelocs_off     = sizeof (*hdr) + GET_UINT32 (hdr->a_text) + GET_UINT32 (hdr->a_data);
    drelocs_off     = trelocs_off + GET_UINT32 (hdr->a_trsize);
    symtab_off      = drelocs_off + GET_UINT32 (hdr->a_drsize);
    strtab_off      = symtab_off + GET_UINT32 (hdr->a_syms);
    
    trelocs_count   = GET_UINT32 (hdr->a_trsize) / sizeof (*trelocs);
    drelocs_count   = GET_UINT32 (hdr->a_drsize) / sizeof (*drelocs);
    symtab_count    = GET_UINT32 (hdr->a_syms) / sizeof (*symtab);
    
    symtab  = (void *) ((char *) obj + symtab_off);
    trelocs = (void *) ((char *) obj + trelocs_off);
    drelocs = (void *) ((char *) obj + drelocs_off);
    strtab  = (char *) obj + strtab_off;
    
    if ((data_obj = malloc (sizeof (*data_obj))) == NULL) {
        return 1;
    }
    
    memset (data_obj, 0, sizeof (*data_obj));
    
    data_obj->filename = fname;
    data_obj->header = hdr;
    data_obj->raw = obj;
    data_obj->size = sz;
    
    data_obj->trelocs = trelocs;
    data_obj->drelocs = drelocs;
    data_obj->symtab = symtab;
    data_obj->strtab = strtab;
    data_obj->trelocs_count = trelocs_count;
    data_obj->drelocs_count = drelocs_count;
    data_obj->symtab_count = symtab_count;
    
    dynarray_add (&state->aout_objs, &state->nb_aout_objs, data_obj);
    return 0;

}

static int process_archive (FILE *ar_file, const char *root_fname) {

    char *fname, *path;
    void *obj;
    
    if ((fname = malloc (17)) == NULL) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to allocation memory (memory full)");
        return 1;
    
    }
    
    if (fseek (ar_file, 8, SEEK_SET) != 0) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed whilst seeking '%s'", root_fname);
        return 1;
    
    }
    
    for (;;) {
    
        struct ar_header hdr;
        
        int err, i;
        uint32_t sz, sz_aligned;
        
        if (fread (&hdr, sizeof (hdr), 1, ar_file) != 1) {
        
            if (feof (ar_file)) {
                break;
            }
            
            report_at (program_name, 0, REPORT_ERROR, "failed whilst reading '%s'", root_fname);
            return 1;
        
        }
            
        sz = conv_dec (hdr.size, 10);
        sz_aligned = (sz % 2) ? (sz + 1) : sz;
        
        if (memcmp (hdr.name, "__.SYMDEF", 9) == 0) {
        
            fseek (ar_file, sz, SEEK_CUR);
            continue;
        
        }
        
        memcpy (fname, hdr.name, 16);
        
        for (i = 0; i < 16; ++i) {
        
            if (fname[i] == 0x20 || fname[i] == '/') {
            
                fname[i] = '\0';
                break;
            
            }
        
        }
        
        if ((obj = malloc (sz_aligned)) == NULL) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed to allocate memory (memory full)");
            return 1;
        
        }
        
        if (fread (obj, sz_aligned, 1, ar_file) != 1) {
        
            report_at (program_name, 0, REPORT_ERROR, "failed whilst reading '%s'", root_fname);
            free (obj);
            
            return 1;
        
        }
        
        if (fname) {
        
            unsigned long len = strlen (root_fname) + 1 + strlen (fname) + 2;
            
            if ((path = malloc (len)) == NULL) {
            
                report_at (program_name, 0, REPORT_ERROR, "failed to allocate memory (memory full)");
                free (obj);
                
                return 1;
            
            }
            
            memset (path, 0, len);
            sprintf (path, "%s(%s)", root_fname, fname);
            
            if ((err = process_aout (obj, sz, path, 1))) {
            
                /*if ((err = process_coff (obj, sz, path, 1))) {*/
                    free (obj);
                /*}*/
            
            }
        
        } else {
        
            if ((err = process_aout (obj, sz, "", 1))) {
            
                /*if ((err = process_coff (obj, sz, "", 1))) {*/
                    free (obj);
                /*}*/
            
            }
        
        }
        
        obj = NULL;
    
    }
    
    free (fname);
    return 0;

}

static int process_file (const char *fname) {

    FILE *ifp;
    void *obj;
    
    char *ar_magic[8];
    long obj_sz;
    
    int err;
    
    if ((ifp = fopen (fname, "rb")) == NULL) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to open '%s' for reading", fname);
        return 1;
    
    }
    
    if (fread (ar_magic, 8, 1, ifp) != 1) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed whilst reading '%s'", fname);
        fclose (ifp);
        
        return 1;
    
    }
    
    if (memcmp (ar_magic, "!<arch>\n", 8) == 0) {
    
        int err = process_archive (ifp, fname);
        fclose (ifp);
        
        if (err) {
            return EXIT_FAILURE;
        }
        
        return EXIT_SUCCESS;
    
    }
    
    if (fseek (ifp, 0, SEEK_END) != 0) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed whist seeking '%s'", fname);
        fclose (ifp);
        
        return 1;
    
    }
    
    if ((obj_sz = ftell (ifp)) == -1) {
    
        report_at (program_name, 0, REPORT_ERROR, "size of '%s' is -1", fname);
        fclose (ifp);
        
        return 1;
    
    }
    
    rewind (ifp);
    
    if ((obj = malloc (obj_sz)) == NULL) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed to allocation memory (memory full)");
        fclose (ifp);
        
        return 1;
    
    }
    
    if (fread (obj, obj_sz, 1, ifp) != 1) {
    
        report_at (program_name, 0, REPORT_ERROR, "failed whilst reading '%s'", fname);
        free (obj);
        
        fclose (ifp);
        return 1;
    
    }
    
    if ((err = process_aout (obj, obj_sz, fname, 1))) {
    
        /*if ((err = process_coff (obj, obj_sz, fname, 1))) {*/
        
            report_at (fname, 0, REPORT_ERROR, "file format not recognized");
            free (obj);
            
            fclose (ifp);
            return 1;
        
        /*}*/
    
    }
    
    fclose (ifp);
    return 0;

}

int main (int argc, char **argv) {

    int err = EXIT_SUCCESS;
    long i;
    
    if (argc && *argv) {
    
        char *p;
        program_name = *argv;
        
        if ((p = strrchr (program_name, '/'))) {
            program_name = (p + 1);
        }
    
    }
    
    state = xmalloc (sizeof (*state));
    parse_args (&argc, &argv, 1);
    
    if (state->nb_files == 0) {
    
        report_at (program_name, 0, REPORT_ERROR, "no input files provided");
        return EXIT_FAILURE;
    
    }
    
    if (state->format == LD_FORMAT_MSDOS) {
        state->code_offset = 0x0100;
    } else if (state->format == LD_FORMAT_I386_PE) {
        state->code_offset = 0x00400000;
    }
    
    if (state->format != LD_FORMAT_I386_AOUT) {
    
        if (state->format == LD_FORMAT_I386_PE) {
            state->impure = 0;
        } else {
            state->impure = 1;
        }
    
    }
    
    for (i = 0; i < state->nb_files; ++i) {
    
        if (process_file (state->files[i])) {
        
            err = EXIT_FAILURE;
            goto out;
        
        }
    
    }
    
    if (state->nb_coff_objs > 0) {
    
        report_at (program_name, 0, REPORT_INTERNAL_ERROR, "currently coff object support is unimplemented");
        
        err = EXIT_FAILURE;
        goto out;
    
    }
    
    if (state->nb_aout_objs > 0 && state->nb_coff_objs > 0) {
    
        report_at (program_name, 0, REPORT_ERROR, "mixing a.out and coff objects is unsupported");
        
        err = EXIT_FAILURE;
        goto out;
    
    }
    
    if (state->format == LD_FORMAT_I386_AOUT && state->nb_coff_objs > 0) {
    
        report_at (program_name, 0, REPORT_ERROR, "creating a a.out executable from coff objects is unsupported");
        
        err = EXIT_FAILURE;
        goto out;
    
    } else if ((state->format == LD_FORMAT_I386_COFF || state->format == LD_FORMAT_I386_PE) && state->nb_aout_objs > 0) {
    
        if (state->format == LD_FORMAT_I386_COFF) {
            report_at (program_name, 0, REPORT_ERROR, "creating a coff executable from a.out objects is unsupported");
        } else {
            report_at (program_name, 0, REPORT_ERROR, "creating a pe executable from a.out objects is unsupported");
        }
        
        err = EXIT_FAILURE;
        goto out;
    
    }
    
    if (state->nb_aout_objs > 0) {
    
        if ((err = create_executable_from_aout_objects ()) != EXIT_SUCCESS) {
        
            remove (state->outfile);
            goto out;
        
        }
    
    }
    
out:
    
    for (i = 0; i < state->nb_aout_objs; ++i) {
    
        struct aout_object *obj = state->aout_objs[i];
        
        if (obj == NULL) {
            continue;
        }
        
        if (obj->raw != NULL) {
            free (obj->raw);
        }
        
        free (obj);
    
    }
    
    for (i = 0; i < state->nb_coff_objs; ++i) {
    
        struct coff_object *obj = state->coff_objs[i];
        
        if (obj == NULL) {
            continue;
        }
        
        if (obj->raw != NULL) {
            free (obj->raw);
        }
        
        free (obj);
    
    }
    
    return err;

}
