/* ------------------------------------------------------------------
 * ZBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "config.h"

#ifndef ZBOX_H
#define ZBOX_H

#define ZBOX_VERSION "1.0.16"

#define COMP_NONE 0
#define COMP_ZLIB 10

#define OPTION_VERBOSE 1
#define OPTION_NOPATHS 2
#define OPTION_LISTONLY 4
#define OPTION_TESTONLY 8
#define OPTION_ZLIB 16

struct header_t
{
    uint8_t magic[4];
    uint32_t comp;
    uint32_t nentity;
    uint32_t nameslen;
    uint32_t crc32;
    uint8_t reserved[44];
} __attribute__ ( ( packed ) );

struct entity_t
{
    uint32_t parent;
    uint32_t mode;
    union
    {
        uint32_t id;
        uint32_t size;
    };
} __attribute__ ( ( packed ) );

struct node_t
{
    char *name;
    struct node_t *next;
    struct node_t *sub;
    struct entity_t entity;
};

struct pack_context_t
{
    uint32_t options;
    struct ar_ostream *ostream;
    char path[PATH_LIMIT];
    unsigned char *workbuf;
    size_t workbuf_size;
};

struct unpack_context_t
{
    uint32_t options;
    struct ar_istream *istream;
    char path[PATH_LIMIT];
    unsigned char *workbuf;
    size_t workbuf_size;
};

struct scan_context_t
{
    uint32_t next_id;
    char path[PATH_LIMIT];
    char *filter;
};

struct unpack_parse_context
{
    const struct entity_t *entity_table;
    const char *name_table;
    const char *name_limit;
    uint32_t count;
    uint32_t position;
};

struct stream_base_context_t
{
    int fd;
    uint32_t crc32;
};

struct ar_stream
{
    struct stream_base_context_t *context;
};

struct ar_ostream
{
    struct stream_base_context_t *context;
    int ( *set_header ) ( struct ar_ostream *, const struct header_t * );
    int ( *write ) ( struct ar_ostream *, const void *, size_t );
    int ( *flush ) ( struct ar_ostream * );
    void ( *seed_crc32 ) ( struct ar_ostream *, const struct header_t * );
      uint32_t ( *finalize_crc32 ) ( struct ar_ostream * );
    void ( *close ) ( struct ar_ostream * );
};

struct ar_istream
{
    struct stream_base_context_t *context;
    int ( *get_header ) ( struct ar_istream *, struct header_t * );
    int ( *read ) ( struct ar_istream *, void *, size_t );
    void ( *seed_crc32 ) ( struct ar_istream *, const struct header_t * );
      uint32_t ( *finalize_crc32 ) ( struct ar_istream * );
    void ( *close ) ( struct ar_istream * );
};

/**
 * Append a new node at the begin of linked list
 */
extern struct node_t *node_insert ( struct node_t **head );

/**
 * Append a new node to the end of linked list
 */
extern struct node_t *node_append ( struct node_t **head );

/**
 * Concatenate paths together
 */
extern int path_concat ( char *path, size_t path_size, const char *name );

/**
 * Scan files tree for archive building
 */
extern int scan_files_tree ( const char *files[], size_t nfiles, struct node_t **root );

/**
 * Free node list from memory, also free node names if needed
 */
extern void free_files_tree ( struct node_t *root, int dynamic_names );

/**
 * Free node list from memory, do not free node names
 */
extern void free_node_list ( struct node_t *root );

/** 
 * Pack files to an archive
 */
extern int zbox_pack_archive ( const char *archive, uint32_t options, int level,
    const char *files[], size_t nfiles );

/** 
 * Unpack files from an archive
 */
extern int zbox_unpack_archive ( const char *archive, uint32_t options );

/**
 * Calculate archive metadata checksum
 */
extern int zbox_calculate_meta_crc32 ( int archivefd, uint32_t meta_size, uint32_t * crc32 );

/**
 * Show operation progress with current file path
 */
extern void show_progress ( char action, const char *path );

/**
 * Open generic output stream
 */
extern int generic_ostream_open ( struct stream_base_context_t *context, int fd );

/**
 * Open generic input stream
 */
extern int generic_istream_open ( struct stream_base_context_t *context, int fd );

/** 
 * Generic put archive header
 */
extern int generic_set_header ( struct ar_ostream *stream, const struct header_t *header );

/** 
 * Generic get archive header
 */
extern int generic_get_header ( struct ar_istream *stream, struct header_t *header );

/**
 * Write data to output stream
 */
extern int generic_write ( struct ar_ostream *stream, const void *data, size_t len );

/**
 * Read data from input stream
 */
extern int generic_read ( struct ar_istream *stream, void *data, size_t len );

/*
 * Finalize output stream
 */
extern int generic_flush ( struct ar_ostream *stream );

/*
 * Set crc32 checksum for stream
 */
extern void generic_seed_crc32 ( struct ar_stream *stream, const struct header_t *header );

/*
 * Get crc32 checksum from stream
 */
extern uint32_t generic_finalize_crc32 ( struct ar_stream *stream );

/*
 * Close stream
 */
extern void generic_close ( struct ar_stream *stream );

/**
 * Open plain output stream
 */
extern struct ar_ostream *plain_ostream_open ( int fd );

/**
 * Open plain input stream
 */
extern struct ar_istream *plain_istream_open ( int fd );

/**
 * Open zlib output stream
 */
extern struct ar_ostream *zlib_ostream_open ( int fd, int level );

/**
 * Open zlib input stream
 */
extern struct ar_istream *zlib_istream_open ( int fd );

/**
 * Calculate checksum of data
 */
extern uint32_t crc32b ( uint32_t crc, const uint8_t * buf, size_t len );

#endif
