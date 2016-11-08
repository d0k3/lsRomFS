#include "romfs.h"

#define OFFSET_HEADER 0x1000
#define MAX_FILES   8192
#define MAX_DIRS    8192

static RomFsLv3Header hdr;
static u32 DirHashTable[MAX_DIRS];
static u32 FileHashTable[MAX_FILES];

// validate header by checking sizes
bool validateLv3Header(RomFsLv3Header* hdr) {
    return ((hdr->size_header == 0x28) &&
        (hdr->offset_dirhash  >= hdr->size_header) &&
        (hdr->offset_dirmeta  >= hdr->offset_dirhash + hdr->size_dirhash) &&
        (hdr->offset_filehash >= hdr->offset_dirmeta + hdr->size_dirmeta) &&
        (hdr->offset_filemeta >= hdr->offset_filehash + hdr->size_filehash) &&
        (hdr->offset_filedata >= hdr->offset_filemeta + hdr->size_filemeta));
}

// hash lvl3 path 
u32 hashLv3Path(u16* name, u32 name_len, u32 offset_parent) {
    u32 hash = offset_parent ^ 123456789;
    for (u32 i = 0; i < name_len; i++)
        hash = ((hash>>5) | (hash<<27)) ^ name[i];
    return hash;
}

u32 hashLv3PathByte(const char* name, u32 offset_parent) {
    u16 wname[256];
    u32 name_len = strnlen(name, 256);
    for (name_len = 0; name[name_len]; name_len++)
        wname[name_len] = name[name_len]; // poor mans UTF-8 -> UTF-16
    return hashLv3Path(wname, name_len, offset_parent);
}

u32 seekLv3Dir(const char* path, FILE* fp) {
    char lpath[256];
    u32 offset_parent = 0;
    u32 offset = 0;
    
    if (!*path) return 0; // root dir
    strncpy(lpath, path, 256);
    for (char* name = strtok(lpath, "/"); name != NULL; name = strtok(NULL, "/")) {
        // hashing, wide name
        u16 wname[256];
        u32 name_len = strnlen(name, 256);
        for (name_len = 0; name[name_len]; name_len++)
            wname[name_len] = name[name_len]; // poor mans UTF-8 -> UTF-16
        offset = DirHashTable[hashLv3Path(wname, name_len, offset_parent) % (hdr.size_dirhash * 4)];
        // read dirmeta
        while (true) {
            RomFsLv3DirMeta dirmeta;
            fseek(fp, OFFSET_HEADER + hdr.offset_dirmeta + offset, SEEK_SET);
            if ((!fread(&dirmeta, 1, sizeof(RomFsLv3DirMeta), fp)) ||
                (offset_parent != dirmeta.offset_parent))
                return (u32) -1; // slim chance of endless loop with broken lvl3 here
            if ((name_len == dirmeta.name_len) && (memcmp(wname, dirmeta.wname, name_len * 2) == 0))
                break;
            offset = dirmeta.offset_samehash;
            if (offset == (u32) -1) return (u32) -1;
        }
        // one level deeper
        offset_parent = offset;
    }
    
    return offset;
}

bool listLv3Dir(const char* path, FILE* fp) {
    u32 offset_parent = seekLv3Dir(path, fp);
    RomFsLv3DirMeta dirmeta_parent;
    u32 cnt_dir = 0;
    u32 cnt_file = 0;
    
    printf("\nDIRECTORY: %s\n\n", (*path) ? path : "ROOT");
    // get parent dirmeta
    fseek(fp, OFFSET_HEADER + hdr.offset_dirmeta + offset_parent, SEEK_SET);
    if (!fread(&dirmeta_parent, 1, sizeof(RomFsLv3DirMeta), fp))
        return false;
    // list dirs
    for (u32 offset = dirmeta_parent.offset_child; offset != (u32) -1; ) {
        RomFsLv3DirMeta dirmeta; // read dir meta
        fseek(fp, OFFSET_HEADER + hdr.offset_dirmeta + offset, SEEK_SET);
        if (!fread(&dirmeta, 1, sizeof(RomFsLv3DirMeta), fp))
            return false;
        char name[256] = { 0 };
        for (u32 i = 0; i < dirmeta.name_len / 2; i++)
            name[i] = (dirmeta.wname[i] & 0xFF); // poor mans UTF-16 -> UTF-8
        printf("%2u: %-48.48s [DIR]\n", cnt_dir, name);
        offset = dirmeta.offset_sibling;
        cnt_dir++;
    }
    // list files
    for (u32 offset = dirmeta_parent.offset_file; offset != (u32) -1; ) {
        RomFsLv3FileMeta filemeta; // read dir meta
        fseek(fp, OFFSET_HEADER + hdr.offset_filemeta + offset, SEEK_SET);
        if (!fread(&filemeta, 1, sizeof(RomFsLv3DirMeta), fp))
            return false;
        char name[256] = { 0 };
        for (u32 i = 0; i < filemeta.name_len / 2; i++)
            name[i] = (filemeta.wname[i] & 0xFF); // poor mans UTF-16 -> UTF-8
        printf("%2u: %-48.48s [%6u]\n", cnt_file, name, (u32) filemeta.size_data);
        offset = filemeta.offset_sibling;
        cnt_file++;
    }
    
    printf("\nTOTAL: %u dirs, %u files\n", cnt_dir, cnt_file); 
    return true;
}    

int main( int argc, char** argv ) {
    FILE* fp;
    const char* path = (argc >= 3) ? argv[2] : "";
    
    if (argc < 2) {
        printf("error: no romfs given");
        return 1;
    }
    
    fp = fopen(argv[1], "rb");
    if (!fp) {
        printf("error: cannot open %s", argv[1]);
        return 1;
    }
    
    // load header
    fseek(fp, OFFSET_HEADER, SEEK_SET);
    if (fread(&hdr, 1, sizeof(RomFsLv3Header), fp) != sizeof(RomFsLv3Header)) {
        printf("error: file too small");
        return 1;
    }
    
    // validate header
    if (!validateLv3Header(&hdr)) {
        printf("error: header not recognized");
        return 1;
    }
    
    // check hash table sizes 
    if ((hdr.size_dirhash > MAX_DIRS * sizeof(u32)) ||
        (hdr.size_filehash > MAX_FILES * sizeof(u32))) {
        printf("error: max # of files / dirs exceeded");
        return 1;
    }
    
    // load dir hash tables
    fseek(fp, OFFSET_HEADER + hdr.offset_dirhash, SEEK_SET);
    if (fread(DirHashTable, 1, hdr.size_dirhash, fp) != hdr.size_dirhash) {
        printf("error: file too small");
        return 1;
    }
    
    // load file hash tables
    fseek(fp, OFFSET_HEADER + hdr.offset_filehash, SEEK_SET);
    if (fread(FileHashTable, 1, hdr.size_filehash, fp) != hdr.size_filehash) {
        printf("error: file too small");
        return 1;
    }
    
    // list dir
    if (!listLv3Dir(path, fp)) {
        printf("error: failed listing files");
        return 1;
    }
    
    fclose(fp);
    return 0;
}