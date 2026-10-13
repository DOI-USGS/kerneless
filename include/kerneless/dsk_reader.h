#ifndef KERNELESS_DSK_READER_H
#define KERNELESS_DSK_READER_H

/**
 * @file kerneless/dsk_reader.h
 * @brief Header-only, dependency-free reader for NAIF SPICE DSK type-02
 *        (triangular plate model) kernels stored in the DAS binary format.
 *
 * This reader parses `.bds` DSK files directly from disk. It links NO SPICE
 * toolkit: only <stdio.h>, <stdint.h> and <string.h> are required, so the
 * header can be dropped into any C or C++ project.
 *
 * The public functions deliberately mirror the cspice DAS/DSK read path so
 * that call sites read like SPICE code and could be swapped for the real
 * `*_c` wrappers with little change:
 *
 *      cspice                     this reader
 *      ----------------------     -----------------------
 *      dasopr_c / dascls_c        kdsk_dasopr  / kdsk_dascls
 *      dlabfs_c / dlafns_c        kdsk_dlabfs  / kdsk_dlafns
 *      dskgd_c                    kdsk_dskgd
 *      dskz02_c                   kdsk_dskz02   (NV, NP)
 *      dskv02_c                   kdsk_dskv02   (vertices)
 *      dskp02_c                   kdsk_dskp02   (plate vertex indices)
 *
 * DAS addresses are 1-based exactly as in SPICELIB. Plate vertex indices are
 * returned 1-based, matching dskp02_c; callers must subtract 1 for 0-based
 * mesh arrays.
 *
 * Intersection routines (dskx02_c / dskn02_c) are intentionally NOT ported:
 * this reader hands back plates and vertices, leaving ray/plate intersection to
 * the caller's own tracer.
 *
 * Deviation from cspice: SPICE identifies an open file by an integer handle
 * looked up in a global table. To stay header-only and free of global state,
 * this reader represents the open file as a caller-allocated ::kdsk_handle
 * passed by pointer. Everything else follows the SPICE semantics.
 *
 * Binary layout ground truth was validated byte-for-byte against
 * `bennu_20facets.bds` (NV=20, NP=36) and the cspice Fortran-to-C sources
 * dasa2l.c, dskb02.c, dskd02.c, dski02.c, dlabfs.c, dlafns.c.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════
 * DAS / DSK constants (see SpiceDLA.h, SpiceDSK.h, dsk02.inc)
 * ═══════════════════════════════════════════════════════════════════════ */

/** Physical DAS record size, in bytes. */
#define KDSK_RECLEN        1024

/** DAS data-type codes (1-based, as used by dasa2l). */
#define KDSK_TYPE_CHAR     1
#define KDSK_TYPE_DP       2
#define KDSK_TYPE_INT      3

/** File-record field byte offsets (0-based). */
#define KDSK_OFF_IDWORD    0    /* 8  bytes, e.g. "DAS/DSK" */
#define KDSK_OFF_NRESVR    68   /* int32: number of reserved records */
#define KDSK_OFF_NCOMR     76   /* int32: number of comment records */
#define KDSK_OFF_LOCFMT    84   /* 8  bytes: "LTL-IEEE" or "BIG-IEEE" */

/** DLA descriptor size (integers). */
#define KDSK_DLADSZ        8

/** DSK descriptor size (doubles) and coordinate-parameter count. */
#define KDSK_DSKDSZ        24
#define KDSK_NSYPAR        10

/** Coordinate system codes (SpiceDSK.h). */
#define KDSK_LATSYS        1
#define KDSK_CYLSYS        2
#define KDSK_RECSYS        3
#define KDSK_PDTSYS        4

/* ═══════════════════════════════════════════════════════════════════════
 * Public structures
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open DSK/DAS file handle.
 *
 * Fields after `fp` are cached bookkeeping resolved at open time. Treat this
 * as opaque; use the kdsk_* functions.
 */
typedef struct {
    FILE*   fp;         /**< Open file stream (read, binary). */
    int     swap;       /**< 1 if on-disk byte order differs from host. */
    int32_t first_dir;  /**< Record number (1-based) of first DAS directory. */
    int     ok;         /**< 1 if the handle is valid/open. */
} kdsk_handle;

/**
 * @brief DLA segment descriptor (mirrors SpiceDLADescr; 8 integers).
 *
 * Base addresses are the *predecessors* of the first element, i.e. the
 * component data occupy 1-based DAS addresses base+1 .. base+size.
 */
typedef struct {
    int32_t bwdptr;  /**< Backward pointer (DAS int addr; -1 = none). */
    int32_t fwdptr;  /**< Forward pointer  (DAS int addr; -1 = none). */
    int32_t ibase;   /**< Base DAS integer address. */
    int32_t isize;   /**< Integer component element count. */
    int32_t dbase;   /**< Base DAS double address. */
    int32_t dsize;   /**< Double component element count. */
    int32_t cbase;   /**< Base DAS character address. */
    int32_t csize;   /**< Character component element count. */
} kdsk_dladescr;

/**
 * @brief DSK segment descriptor (mirrors SpiceDSKDescr).
 *
 * On disk this is 24 doubles; the leading six ID codes are stored as doubles
 * and cast to integers here, matching dskgd_c.
 */
typedef struct {
    int32_t surfce;              /**< Surface ID code. */
    int32_t center;              /**< Center (body) ID code. */
    int32_t dclass;              /**< Data class code. */
    int32_t dtype;               /**< DSK data type (2 for plate models). */
    int32_t frmcde;             /**< Reference frame ID code. */
    int32_t corsys;             /**< Coordinate system code (KDSK_*SYS). */
    double  corpar[KDSK_NSYPAR];/**< Coordinate system parameters. */
    double  co1min, co1max;      /**< First coordinate bounds. */
    double  co2min, co2max;      /**< Second coordinate bounds. */
    double  co3min, co3max;      /**< Third coordinate bounds. */
    double  start,  stop;        /**< Coverage time bounds (s past J2000). */
} kdsk_dskdescr;

/* ═══════════════════════════════════════════════════════════════════════
 * Low-level helpers (internal)
 * ═══════════════════════════════════════════════════════════════════════ */

/** @return 1 if the host CPU is little-endian. */
static inline int kdsk__host_is_little(void) {
    const uint16_t one = 1u;
    return *(const uint8_t*)&one;
}

static inline uint32_t kdsk__bswap32(uint32_t v) {
    return (v >> 24) | ((v >> 8) & 0x0000FF00u) |
           ((v << 8) & 0x00FF0000u) | (v << 24);
}

static inline uint64_t kdsk__bswap64(uint64_t v) {
    return  (v >> 56) |
           ((v >> 40) & 0x000000000000FF00ull) |
           ((v >> 24) & 0x0000000000FF0000ull) |
           ((v >>  8) & 0x00000000FF000000ull) |
           ((v <<  8) & 0x000000FF00000000ull) |
           ((v << 24) & 0x0000FF0000000000ull) |
           ((v << 40) & 0x00FF000000000000ull) |
            (v << 56);
}

/** Read `nbytes` at absolute byte offset `off`. @return 0 on success. */
static inline int kdsk__read_at(kdsk_handle* h, long off, void* dst, size_t nbytes) {
    if (fseek(h->fp, off, SEEK_SET) != 0) return -1;
    return (fread(dst, 1, nbytes, h->fp) == nbytes) ? 0 : -1;
}

/** Read one int32 at a physical (record, word) pair (both 1-based). */
static inline int32_t kdsk__phys_geti(kdsk_handle* h, int32_t recno, int32_t wordno) {
    int32_t v = 0;
    long off = (long)(recno - 1) * KDSK_RECLEN + (long)(wordno - 1) * 4;
    if (kdsk__read_at(h, off, &v, 4) != 0) return -1;
    if (h->swap) v = (int32_t)kdsk__bswap32((uint32_t)v);
    return v;
}

/**
 * @brief Map a 1-based DAS address to a physical (record, word) location.
 *
 * Faithful port of cspice dasa2l.c (slow, fully general path): walks the DAS
 * directory doubly linked list to find the cluster containing `addr`, then
 * applies the record/word formula. Works for both segregated single-directory
 * files (the common DSK case) and multi-directory files.
 *
 * @return 0 on success, -1 if the directory chain is exhausted/corrupt.
 */
static inline int kdsk__addr_to_loc(kdsk_handle* h, int type, int32_t addr,
                                     int32_t* recno, int32_t* wordno) {
    static const int nw[3]     = { 1024, 128, 256 };  /* words per record   */
    static const int nxt[3]    = { 2, 3, 1 };         /* type successor      */
    static const int prv[3]    = { 3, 1, 2 };         /* type predecessor    */
    static const int rngloc[3] = { 3, 5, 7 };         /* low-addr word/type  */

    const int w = nw[type - 1];

    /* Locate the directory whose address range for this type covers addr. */
    int32_t nrec = h->first_dir;
    int32_t lo = kdsk__phys_geti(h, nrec, rngloc[type - 1]);
    int32_t hi = kdsk__phys_geti(h, nrec, rngloc[type - 1] + 1);
    while (hi < addr) {
        int32_t nxtrec = kdsk__phys_geti(h, nrec, 2);  /* forward pointer */
        if (nxtrec <= 0) return -1;
        nrec = nxtrec;
        lo = kdsk__phys_geti(h, nrec, rngloc[type - 1]);
        hi = kdsk__phys_geti(h, nrec, rngloc[type - 1] + 1);
    }

    /* Walk the run-length-encoded type descriptors of directory `nrec`,
     * accumulating cluster base/size until the cluster holding addr. */
    int32_t clbase = nrec + 1;
    int32_t clsize = 0;
    int32_t hiaddr = lo - 1;
    int firsttype = kdsk__phys_geti(h, nrec, 9);       /* word 9 */
    if (firsttype < 1 || firsttype > 3) return -1;
    int prvtyp = prv[firsttype - 1];
    int dscloc = 10;                                    /* word 10 (1-based) */

    while (hiaddr < addr) {
        if (dscloc > 256) return -1;                    /* corrupt directory */
        clbase += clsize;
        int32_t d = kdsk__phys_geti(h, nrec, dscloc);
        int curtyp = (d > 0) ? nxt[prvtyp - 1] : prv[prvtyp - 1];
        prvtyp = curtyp;
        int32_t cnt = (d < 0) ? -d : d;
        if (curtyp == type) hiaddr += (int32_t)w * cnt;
        clsize = cnt;
        ++dscloc;
    }

    *recno  = clbase + clsize - 1 - (hiaddr - addr) / w;
    *wordno = ((addr - 1) % w) + 1;
    return 0;
}

/**
 * @brief Read integers at 1-based DAS integer addresses a0..a1 (inclusive).
 * Reads in physical-record-sized chunks. @return 0 on success.
 */
static inline int kdsk__rdi(kdsk_handle* h, int32_t a0, int32_t a1, int32_t* buf) {
    int32_t addr = a0;
    size_t  bi = 0;
    while (addr <= a1) {
        int32_t recno, wordno;
        if (kdsk__addr_to_loc(h, KDSK_TYPE_INT, addr, &recno, &wordno) != 0) return -1;
        int navail = 256 - (wordno - 1);          /* rest of this record */
        int nneed  = (int)(a1 - addr + 1);
        int n = (navail < nneed) ? navail : nneed;
        long off = (long)(recno - 1) * KDSK_RECLEN + (long)(wordno - 1) * 4;
        if (kdsk__read_at(h, off, &buf[bi], (size_t)n * 4) != 0) return -1;
        if (h->swap)
            for (int k = 0; k < n; ++k)
                buf[bi + k] = (int32_t)kdsk__bswap32((uint32_t)buf[bi + k]);
        bi += (size_t)n;
        addr += n;
    }
    return 0;
}

/**
 * @brief Read doubles at 1-based DAS d.p. addresses a0..a1 (inclusive).
 * @return 0 on success.
 */
static inline int kdsk__rdd(kdsk_handle* h, int32_t a0, int32_t a1, double* buf) {
    int32_t addr = a0;
    size_t  bi = 0;
    while (addr <= a1) {
        int32_t recno, wordno;
        if (kdsk__addr_to_loc(h, KDSK_TYPE_DP, addr, &recno, &wordno) != 0) return -1;
        int navail = 128 - (wordno - 1);          /* rest of this record */
        int nneed  = (int)(a1 - addr + 1);
        int n = (navail < nneed) ? navail : nneed;
        long off = (long)(recno - 1) * KDSK_RECLEN + (long)(wordno - 1) * 8;
        if (kdsk__read_at(h, off, &buf[bi], (size_t)n * 8) != 0) return -1;
        if (h->swap) {
            for (int k = 0; k < n; ++k) {
                uint64_t u;
                memcpy(&u, &buf[bi + k], 8);
                u = kdsk__bswap64(u);
                memcpy(&buf[bi + k], &u, 8);
            }
        }
        bi += (size_t)n;
        addr += n;
    }
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Public API — DAS open/close (mirrors dasopr_c / dascls_c)
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open a DSK/DAS file for read access.
 *
 * Reads the file record, determines byte order from LOCFMT, and computes the
 * first directory record number (= NRESVR + NCOMR + 2).
 *
 * @param fname   Path to the `.bds` file.
 * @param handle  Caller-allocated handle to populate.
 * @return        0 on success; nonzero on error (handle->ok is set to 0).
 */
static inline int kdsk_dasopr(const char* fname, kdsk_handle* handle) {
    memset(handle, 0, sizeof(*handle));
    handle->fp = fopen(fname, "rb");
    if (!handle->fp) return -1;

    /* File record: read enough to cover IDWORD..LOCFMT. */
    char rec[KDSK_RECLEN];
    if (fread(rec, 1, sizeof(rec), handle->fp) != sizeof(rec)) {
        fclose(handle->fp);
        handle->fp = NULL;
        return -1;
    }

    /* Sanity: DAS files start with an idword beginning "DAS". */
    if (strncmp(rec + KDSK_OFF_IDWORD, "DAS", 3) != 0) {
        fclose(handle->fp);
        handle->fp = NULL;
        return -1;
    }

    /* Byte order from LOCFMT (ASCII, so order-independent). */
    int file_is_little = (strncmp(rec + KDSK_OFF_LOCFMT, "LTL-IEEE", 8) == 0);
    handle->swap = (file_is_little != kdsk__host_is_little());

    int32_t nresvr, ncomr;
    memcpy(&nresvr, rec + KDSK_OFF_NRESVR, 4);
    memcpy(&ncomr,  rec + KDSK_OFF_NCOMR,  4);
    if (handle->swap) {
        nresvr = (int32_t)kdsk__bswap32((uint32_t)nresvr);
        ncomr  = (int32_t)kdsk__bswap32((uint32_t)ncomr);
    }

    /* Record #1 is the file record; then NRESVR reserved + NCOMR comment
     * records; the first data directory is the next record. */
    handle->first_dir = nresvr + ncomr + 2;
    handle->ok = 1;
    return 0;
}

/**
 * @brief Close a DSK/DAS file opened with kdsk_dasopr.
 */
static inline void kdsk_dascls(kdsk_handle* handle) {
    if (handle && handle->fp) {
        fclose(handle->fp);
        handle->fp = NULL;
        handle->ok = 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 * Public API — DLA segment iteration (mirrors dlabfs_c / dlafns_c)
 * ═══════════════════════════════════════════════════════════════════════ */

/** Read the 8-integer DLA descriptor whose first integer is at DAS int `at`. */
static inline int kdsk__read_dladescr(kdsk_handle* h, int32_t at, kdsk_dladescr* d) {
    int32_t buf[KDSK_DLADSZ];
    if (kdsk__rdi(h, at, at + 7, buf) != 0) return -1;
    d->bwdptr = buf[0]; d->fwdptr = buf[1];
    d->ibase  = buf[2]; d->isize  = buf[3];
    d->dbase  = buf[4]; d->dsize  = buf[5];
    d->cbase  = buf[6]; d->csize  = buf[7];
    return 0;
}

/**
 * @brief Begin a forward search: fetch the first DLA segment descriptor.
 *
 * @param handle  Open DSK/DAS handle.
 * @param dladsc  Output descriptor (valid only if *found is set).
 * @param found   Set to 1 if the file contains at least one segment, else 0.
 */
static inline void kdsk_dlabfs(kdsk_handle* handle, kdsk_dladescr* dladsc, int* found) {
    *found = 0;
    /* DAS integer address 2 (LLBIDX) holds the pointer to the first segment. */
    int32_t first;
    if (kdsk__rdi(handle, 2, 2, &first) != 0) return;
    if (first == -1) return;                 /* SPICE_DLA_NULPTR: no segments */
    if (kdsk__read_dladescr(handle, first, dladsc) != 0) return;
    *found = 1;
}

/**
 * @brief Find the segment following `dladsc`.
 *
 * @param handle  Open DSK/DAS handle.
 * @param dladsc  Descriptor of the current segment.
 * @param nxtdsc  Output descriptor of the next segment (valid if *found).
 * @param found   Set to 1 if a following segment exists, else 0.
 */
static inline void kdsk_dlafns(kdsk_handle* handle, const kdsk_dladescr* dladsc,
                                kdsk_dladescr* nxtdsc, int* found) {
    *found = 0;
    int32_t fwd = dladsc->fwdptr;
    if (fwd == -1) return;                   /* end of the segment list */
    if (kdsk__read_dladescr(handle, fwd, nxtdsc) != 0) return;
    *found = 1;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Public API — DSK segment queries (mirror dskgd_c / dskz02_c / dskv02_c /
 *              dskp02_c)
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read a segment's DSK descriptor (24 doubles at dbase+1 .. dbase+24).
 * @return 0 on success.
 */
static inline int kdsk_dskgd(kdsk_handle* handle, const kdsk_dladescr* dladsc,
                              kdsk_dskdescr* dskdsc) {
    double b[KDSK_DSKDSZ];
    if (kdsk__rdd(handle, dladsc->dbase + 1, dladsc->dbase + KDSK_DSKDSZ, b) != 0)
        return -1;
    dskdsc->surfce = (int32_t)b[0];
    dskdsc->center = (int32_t)b[1];
    dskdsc->dclass = (int32_t)b[2];
    dskdsc->dtype  = (int32_t)b[3];
    dskdsc->frmcde = (int32_t)b[4];
    dskdsc->corsys = (int32_t)b[5];
    for (int i = 0; i < KDSK_NSYPAR; ++i) dskdsc->corpar[i] = b[6 + i];
    dskdsc->co1min = b[16]; dskdsc->co1max = b[17];
    dskdsc->co2min = b[18]; dskdsc->co2max = b[19];
    dskdsc->co3min = b[20]; dskdsc->co3max = b[21];
    dskdsc->start  = b[22]; dskdsc->stop   = b[23];
    return 0;
}

/**
 * @brief Fetch vertex and plate counts for a type-2 segment.
 *
 * Integer header: NV at ibase+1, NP at ibase+2.
 * @return 0 on success.
 */
static inline int kdsk_dskz02(kdsk_handle* handle, const kdsk_dladescr* dladsc,
                               int32_t* nv, int32_t* np) {
    int32_t buf[2];
    if (kdsk__rdi(handle, dladsc->ibase + 1, dladsc->ibase + 2, buf) != 0) return -1;
    *nv = buf[0];
    *np = buf[1];
    return 0;
}

/**
 * @brief Fetch a range of vertices from a type-2 segment.
 *
 * Vertices begin at DAS d.p. address dbase+35; vertex k (1-based) occupies
 * three consecutive doubles. Reads `room` vertices starting at `start`
 * (1-based), or fewer if the segment ends first.
 *
 * @param start    1-based index of first vertex to read.
 * @param room     Maximum number of vertices to read.
 * @param n        Output: number of vertices actually read.
 * @param vrtces   Output buffer, flat [x0,y0,z0, x1,y1,z1, ...]; 3*room doubles.
 * @return 0 on success.
 */
static inline int kdsk_dskv02(kdsk_handle* handle, const kdsk_dladescr* dladsc,
                               int32_t start, int32_t room, int32_t* n, double* vrtces) {
    int32_t nv, np;
    if (kdsk_dskz02(handle, dladsc, &nv, &np) != 0) return -1;
    if (start < 1 || start > nv) { *n = 0; return 0; }
    int32_t avail = nv - start + 1;
    int32_t count = (room < avail) ? room : avail;
    /* First double of vertex `start`: dbase + 35 + (start-1)*3. */
    int32_t a0 = dladsc->dbase + 35 + (start - 1) * 3;
    int32_t a1 = a0 + count * 3 - 1;
    if (kdsk__rdd(handle, a0, a1, vrtces) != 0) return -1;
    *n = count;
    return 0;
}

/**
 * @brief Fetch a range of plates from a type-2 segment.
 *
 * The plate array begins at DAS integer address ibase+11; plate k (1-based)
 * occupies three consecutive integers, each a 1-based vertex index (matching
 * dskp02_c). Callers building 0-based meshes must subtract 1.
 *
 * @param start   1-based index of first plate to read.
 * @param room    Maximum number of plates to read.
 * @param n       Output: number of plates actually read.
 * @param plates  Output buffer, flat [a0,b0,c0, a1,b1,c1, ...]; 3*room ints.
 * @return 0 on success.
 */
static inline int kdsk_dskp02(kdsk_handle* handle, const kdsk_dladescr* dladsc,
                               int32_t start, int32_t room, int32_t* n, int32_t* plates) {
    int32_t nv, np;
    if (kdsk_dskz02(handle, dladsc, &nv, &np) != 0) return -1;
    if (start < 1 || start > np) { *n = 0; return 0; }
    int32_t avail = np - start + 1;
    int32_t count = (room < avail) ? room : avail;
    /* First integer of plate `start`: ibase + 11 + (start-1)*3. */
    int32_t a0 = dladsc->ibase + 11 + (start - 1) * 3;
    int32_t a1 = a0 + count * 3 - 1;
    if (kdsk__rdi(handle, a0, a1, plates) != 0) return -1;
    *n = count;
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* KERNELESS_DSK_READER_H */
