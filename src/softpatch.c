#include "softpatch.h"

#include "app.h"
#include "jparse.h"
#include "pd_api.h"
#include "userstack.h"
#include "utility.h"

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

    if (result != 0)
        return false;
    if (!stat.isdir)
        return false;
    return true;
}

struct ListPatchAcc
{
    SoftPatch* list;
    const char* patch_dir;
};

void list_patch_cb(const char* filename, void* ud)
{
    struct ListPatchAcc* acc = ud;

    int n = 0;
    for (SoftPatch* patch = acc->list; patch && patch->fullpath; ++patch, ++n)
        ;

    bool ips = endswithi(filename, ".ips");
    bool bps = endswithi(filename, ".bps");

    if (ips || bps)
    {
        // add new one
        acc->list = playdate->system->realloc(acc->list, sizeof(SoftPatch) * (n + 2));
        acc->list[n + 1].fullpath = NULL;  // terminal
        acc->list[n + 1].basename = NULL;  // terminal

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
    parse_json(listpath, &jv, kFileReadData);
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

                if (jbasename.type != kJSONString || jorder.type != kJSONInteger)
                    continue;

                // find matching patch
                for (SoftPatch* patch = acc.list; patch && patch->fullpath; ++patch)
                {
                    if (!strcmp(jbasename.data.stringval, patch->basename))
                    {
                        patch->state =
                            (jenabled.type == kJSONTrue) ? PATCH_ENABLED : PATCH_DISABLED;
                        patch->_order = jorder.data.intval;
                        nextorder = MAX(patch->_order + 1, nextorder);
                        break;
                    }
                }
            }
        }
    }

    free_json_data(jpatches);

    if (new_patch_count)
        *new_patch_count = 0;

    // assign an _order value to any unlisted patches
    size_t len = 0;
    for (SoftPatch* patch = acc.list; patch && patch->fullpath; ++patch)
    {
        if (patch->_order < 0)
        {
            patch->_order = nextorder++;
            if (new_patch_count)
            {
                (*new_patch_count)++;
            }
        }
        ++len;
    }

    // insertion-sort by _order value.
    if (len > 1)
    {
        for (size_t i = 1; i < len; i++)
        {
            size_t j = i;
            while (j > 0 && (acc.list[j]._order < acc.list[j - 1]._order))
            {
                memswap(&acc.list[j], &acc.list[j - 1], sizeof(SoftPatch));
                j--;
            }
        }
    }

    return acc.list;
}

void save_patches_state(const char* rom_path, SoftPatch* patches)
{
    if (!patches)
        return;

    // number of patches
    size_t len = 0;
    for (SoftPatch* patch = patches; patch->fullpath; ++patch, ++len)
        ;
    if (len == 0)
        return;

    JsonArray* jpatcharray = pgb_malloc(sizeof(JsonArray) + len * sizeof(json_value));
    if (!jpatcharray)
        return;
    jpatcharray->n = len;

    json_value jmanifest = json_new_table();
    json_value jpatches = {.type = kJSONArray};
    jpatches.data.arrayval = jpatcharray;
    json_set_table_value(&jmanifest, "patches", jpatches);

    for (size_t i = 0; i < len; ++i)
    {
        SoftPatch* patch = &patches[i];
        json_value jpatch = json_new_table();
        json_set_table_value(&jpatch, "basename", json_new_string(patch->basename));
        json_set_table_value(&jpatch, "n", json_new_int((int)i));
        if (patch->state == PATCH_ENABLED)
        {
            json_set_table_value(&jpatch, "enabled", json_new_bool(true));
        }

        jpatcharray->data[i] = jpatch;
    }

    char* dir = get_patches_directory(rom_path);
    playdate->file->mkdir(dir);
    char* plf = patch_list_file(dir);
    pgb_free(dir);
    printf("saving patches state to %s...\n", plf);
    write_json_to_disk(plf, jmanifest);
    pgb_free(plf);

    free_json_data(jmanifest);
}

void free_patches(SoftPatch* patchlist)
{
    if (!patchlist)
        return;

    for (SoftPatch* patch = patchlist; patch->fullpath; ++patch)
    {
        pgb_free(patch->fullpath);
        if (patch->basename)
            pgb_free(patch->basename);
    }
    pgb_free(patchlist);
}

unsigned read_bigendian(void* src, int bytes)
{
    unsigned x = 0;
    while (bytes-- > 0)
    {
        x <<= 8;
        x |= *(uint8_t*)(src++);
    }
    return x;
}

#define IPS_MAGIC "PATCH"
#define IPS_EOF 0x454F46 /* "EOF" */

static bool apply_ips_patch(void** rom, size_t* romsize, const SoftPatch* patch)
{
    size_t ips_len;
    void* ips =
        call_with_main_stack_3(pgb_read_entire_file, patch->fullpath, &ips_len, kFileReadData);
    if (!ips)
    {
        playdate->system->error("Unable to open IPS patch \"%s\"", patch->fullpath);
        return false;
    }

#define ADVANCE(X)          \
    do                      \
    {                       \
        unsigned __x = (X); \
        ips += __x;         \
        ips_len -= __x;     \
    } while (0)
#define CHECKLEN(l)        \
    do                     \
    {                      \
        if (ips_len < (l)) \
            goto err;      \
    } while (0)

    if (memcmp(ips, IPS_MAGIC, 5))
    {
        goto err;
    }
    ADVANCE(5);

    while (ips_len > 0)
    {
        CHECKLEN(3);
        unsigned offset = read_bigendian(ips, 3);
        ADVANCE(3);

        if (offset == IPS_EOF)
            break;

        bool rle = false;
        CHECKLEN(2);
        unsigned length = read_bigendian(ips, 2);
        ADVANCE(2);

        if (length == 0)
        {
            CHECKLEN(3);

            // RLE
            length = read_bigendian(ips, 2);
            rle = true;
        }

        if (length + offset >= *romsize)
        {
            *romsize = length + offset;
            *rom = pgb_realloc(*rom, *romsize);
            if (!*rom)
            {
                playdate->system->error(
                    "IPS patch requires ROM to be resized, but there was not enough memory."
                );
                pgb_free(ips);
                return false;
            }
        }

        if (rle)
        {
            // run-length encoded hunk
            CHECKLEN(1);
            unsigned v = read_bigendian(ips, 1);

            while (length-- > 0)
            {
                ((uint8_t*)*rom)[offset++] = v;
            }
        }
        else
        {
            // standard hunk
            CHECKLEN(length);
            while (length-- > 0)
            {
                unsigned v = read_bigendian(ips, 1);
                ADVANCE(1);

                ((uint8_t*)*rom)[offset++] = v;
            }
        }
    }

    // TODO: ips extension for cropping ROM length

    return true;

err:
    playdate->system->error("Error applying IPS patch \"%s\"", patch->fullpath);
    pgb_free(ips);
    return false;
}

bool patch_rom(void** rom, size_t* romsize, const SoftPatch* patchlist)
{
    bool success = true;
    for (const SoftPatch* patch = patchlist; patch && patch->fullpath; patch++)
    {
        if (patch->state != PATCH_ENABLED)
            continue;

        printf("Applying %s...\n", patch->fullpath);

        if (patch->ips)
        {
            success = success && apply_ips_patch(rom, romsize, patch);
        }
        else
        {
            success = false;
            playdate->system->error("BPS patches are currently unsupported.");
        }
    }

    return success;
}
