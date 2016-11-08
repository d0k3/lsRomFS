#pragma once
#include "common.h"

typedef struct {
    u32 size_header;
    u32 offset_dirhash;
    u32 size_dirhash;
    u32 offset_dirmeta;
    u32 size_dirmeta;
    u32 offset_filehash;
    u32 size_filehash;
    u32 offset_filemeta;
    u32 size_filemeta;
    u32 offset_filedata;
} __attribute__((packed)) RomFsLv3Header;

typedef struct {
    u32 offset_parent;
    u32 offset_sibling;
    u32 offset_child;
    u32 offset_file;
    u32 offset_samehash;
    u32 name_len;
    u16 wname[256]; // 256 assumed to be max name length
} __attribute__((packed)) RomFsLv3DirMeta;

typedef struct {
    u32 offset_parent;
    u32 offset_sibling;
    u64 offset_data;
    u64 size_data;
    u32 offset_samehash;
    u32 name_len;
    u16 wname[256]; // 256 assumed to be max name length
} __attribute__((packed)) RomFsLv3FileMeta;
