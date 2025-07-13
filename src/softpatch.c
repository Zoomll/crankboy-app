#include "softpatch.h"
#include "utility.h"
#include "pd_api.h"
#include "jparse.h"
#include "app.h"

char* get_patches_directory(const char* rom_path)
{
    char* bn = pgb_basename(rom_path, true);
    char* f = aprintf("%s/%s", PGB_patchesPath, bn);
    
    pgb_free(bn);
    return f;
}

bool patches_directory_exists(const char* rom_path)
{
    char* dir = get_patches_directory(rom_path);
    
    FileStat stat;
    int result = playdate->file->stat(dir, &stat);
    pgb_free(dir);
    
    if (result != 0) return false;
    if (!stat.isdir) return false;
    return true;
}

struct ListPatchAcc {
    SoftPatch* list;
    const char* patch_dir;
};

void list_patch_cb(const char* filename, void* ud)
{
    struct ListPatchAcc* acc = ud;
    
    int n = 0;
    for (SoftPatch* patch = acc->list; patch && patch->fullpath; ++patch, ++n);
    
    bool ips = endswithi(filename, ".ips");
    bool bps = endswithi(filename, ".bps");
    
    if (ips || bps)
    {
        // add new one
        acc->list = playdate->system->realloc(acc->list, sizeof(SoftPatch)*(n+2));
        acc->list[n+1].basename = NULL; // terminal
        
        SoftPatch* patch = &acc->list[n];
        patch->fullpath = aprintf("%s/%s", acc->patch_dir, filename);
        patch->basename = pgb_basename(filename, true);
        patch->ips = ips;
        patch->bps = bps;
        patch->state = PATCH_UNKNOWN;
        patch->_order = -1;
    }
}

static char* patch_list_file(const char* patch_dir)
{
    return aprintf("%s/%s", patch_dir, PATCH_LIST_FILE);
}

SoftPatch* list_patches(const char* rom_path, int* new_patch_count)
{
    char* patch_dir = get_patches_directory(rom_path);
    struct ListPatchAcc acc;
    acc.list = NULL;
    acc.patch_dir = patch_dir;
    
    playdate->file->listfiles(patch_dir, list_patch_cb, &acc, true);
    char* listpath = patch_list_file(patch_dir);
    pgb_free(patch_dir);
    
    json_value jv;
    parse_json(listpath, &jv, kFileRead);
    pgb_free(listpath);
    json_value jpatches = json_get_table_value(jv, "patches");
    int nextorder = 0;
    if (jpatches.type == kJSONArray)
    {
        JsonArray* arr = jpatches.data.arrayval;
        for (size_t i = 0; i < arr->n; ++i)
        {
            json_value entry = arr->data[i];
            if (entry.type == kJSONTable)
            {
                json_value jbasename = json_get_table_value(entry, "basename");
                json_value jorder = json_get_table_value(entry, "n");
                json_value jenabled = json_get_table_value(entry, "enabled");
                
                if (jbasename.type != kJSONString || jorder.type != kJSONInteger) continue;
                
                // find matching patch
                for (SoftPatch* patch = acc.list; patch && patch->basename; ++patch)
                {
                    if (!strcmp(jbasename.data.stringval, patch->basename))
                    {
                        patch->state = (jenabled.type == kJSONTrue)
                            ? PATCH_ENABLED
                            : PATCH_DISABLED;
                        patch->_order = jorder.data.intval;
                        nextorder = MAX(patch->_order + 1, nextorder);
                        break;
                    }
                }
            }
        }
    }
    
    free_json_data(jpatches);
    
    if (new_patch_count) *new_patch_count = 0;
    
    // assign an _order value to any unlisted patches
    size_t len = 0;
    for (SoftPatch* patch = acc.list; patch && patch->basename; ++patch)
    {
        if (patch->_order < 0)
        {
            patch->_order = nextorder++;
            *new_patch_count++;
        }
        ++len;
    }

    // inserion-sort by _order value.
    if (len > 1) {
        for (size_t i = 1; i < len; i++) {
            size_t j = i;
            while (j > 0 && (
                acc.list[j]._order < acc.list[j-1]._order
            ))
            {
                memswap(&acc.list[j], &acc.list[j-1], sizeof(SoftPatch));
                j--;
            }
        }
    }
    
    return acc.list;
}

void save_patches_state(const char* rom_path, SoftPatch* patches)
{
    if (!patches) return;
    
    // number of patches
    size_t len = 0;
    for (SoftPatch* patch = patches; patch->fullpath; ++patch, ++len);
    if (len == 0) return;
    
    JsonArray* jpatcharray = pgb_malloc(sizeof(JsonArray) + len*sizeof(json_value));
    if (!jpatcharray) return;
    jpatcharray->n = len;
    
    json_value jmanifest = json_new_table();
    json_value jpatches = {
        .type = kJSONArray,
    };
    jpatches.data.arrayval = jpatcharray;
    json_set_table_value(&jmanifest, "patches", jpatches);
    
    for (size_t i = 0; i < len; ++i)
    {
        SoftPatch* patch = &patches[i];
        json_value jpatch = json_new_table();
        json_set_table_value(
            &jpatch,
            "basename", 
            json_new_string(patch->basename)
        );
        json_set_table_value(
            &jpatch,
            "n", 
            json_new_int((int)i)
        );
        if (patch->state == PATCH_ENABLED)
        {
            json_set_table_value(
                &jpatch,
                "enabled", 
                json_new_bool(true)
            );
        }
        
        jpatcharray->data[i] = jpatch;
    }
    
    char* dir = get_patches_directory(rom_path);
    playdate->file->mkdir(dir);
    char* plf = patch_list_file(dir);
    pgb_free(dir);
    write_json_to_disk(plf, jmanifest);
    pgb_free(plf);
    
    free_json_data(jmanifest);
}

void free_patches(SoftPatch* patchlist)
{
    if (!patchlist) return;
    
    for (SoftPatch* patch = patchlist; patch->fullpath; ++patch)
    {
        pgb_free(patch->fullpath);
        if (patch->basename) pgb_free(patch->basename);
    }
    pgb_free(patchlist);
}