#pragma once

// Maps the small mz_* subset RipStop needs onto host zlib. Used when
// RIPSTOP_USE_ZLIB=ON so consumers that already ship miniz/zlib avoid LNK2005.
#include <zlib.h>

typedef uLong mz_ulong;

#define MZ_OK Z_OK
#define MZ_CRC32_INIT 0
#define mz_compressBound compressBound
#define mz_compress compress
#define mz_uncompress uncompress
#define mz_crc32 crc32
