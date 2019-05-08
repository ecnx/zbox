/* ------------------------------------------------------------------
 * ZBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "zbox.h"
#include <zlib.h>

#ifdef ENABLE_ZLIB

#define CHUNK 32768

/**
 * Zlib capable stream structure
 */
struct stream_zlib_context_t
{
    int fd;
    uint32_t crc32;
    int strm_allocated;
    z_stream strm;
    unsigned char *unconsumed;
    size_t u_off;
    size_t u_len;
    size_t u_size;
};

/**
 * Write data to zlib output stream
 */
static int zlib_write ( struct ar_ostream *stream, const void *data, size_t len )
{
    size_t have;
    size_t inlen;
    struct stream_zlib_context_t *context = ( struct stream_zlib_context_t * ) stream->context;
    z_stream *strm = &context->strm;
    unsigned char out[CHUNK];

    stream->context->crc32 = crc32b ( stream->context->crc32, ( const unsigned char * ) data, len );

    while ( len )
    {
        inlen = len < CHUNK ? len : CHUNK;

        strm->avail_in = inlen;
        strm->next_in = ( void * ) data;

        strm->avail_out = sizeof ( out );
        strm->next_out = out;

        if ( deflate ( strm, Z_NO_FLUSH ) == Z_STREAM_ERROR )
        {
            return -1;
        }

        have = sizeof ( out ) - strm->avail_out;

        if ( have && ( size_t ) write ( stream->context->fd, out, have ) != have )
        {
            return -1;
        }

        have = inlen - strm->avail_in;
        data += have;
        len -= have;
    }

    return 0;
}

/**
 * Decompress data into context unconsumed data buffer
 */
static int decompress_data ( z_stream * strm, const unsigned char *in, size_t avail,
    struct stream_zlib_context_t *context )
{
    size_t have;
    unsigned char *u_backup;

    context->u_off = 0;
    context->u_len = 0;

    while ( avail )
    {
        if ( context->u_off + CHUNK > context->u_size )
        {
            u_backup = context->unconsumed;
            context->u_size <<= 1;
            if ( !( context->unconsumed =
                    ( unsigned char * ) realloc ( context->unconsumed, context->u_size ) ) )
            {
                perror ( "realloc" );
                free ( u_backup );
                return -1;
            }
        }

        strm->avail_in = avail;
        strm->next_in = ( unsigned char * ) in;
        strm->avail_out = CHUNK;
        strm->next_out = context->unconsumed + context->u_off;

        if ( inflate ( strm, Z_NO_FLUSH ) == Z_STREAM_ERROR )
        {
            return -1;
        }

        have = CHUNK - strm->avail_out;
        context->u_len += have;

        if ( strm->avail_in )
        {
            context->u_off += have;

            have = avail - strm->avail_in;
            in += have;
        }

        have = avail - strm->avail_in;
        avail -= have;
    }

    context->u_off = 0;

    return 0;
}

/**
 * Read data from zlib input stream
 */
static int zlib_read ( struct ar_istream *stream, void *data, size_t len )
{
    size_t have;
    size_t avail;
    struct stream_zlib_context_t *context = ( struct stream_zlib_context_t * ) stream->context;
    z_stream *strm = &context->strm;
    unsigned char in[CHUNK];

    if ( context->u_off < context->u_len )
    {
        have = context->u_len - context->u_off;
        if ( len < have )
        {
            have = len;
        }
        memcpy ( data, context->unconsumed + context->u_off, have );
        stream->context->crc32 = crc32b ( stream->context->crc32, ( unsigned char * ) data, have );
        context->u_off += have;
        data += have;
        len -= have;
    }

    while ( len )
    {
        if ( ( ssize_t ) ( avail = read ( stream->context->fd, in, sizeof ( in ) ) ) < 0 )
        {
            return -1;
        }

        if ( !avail )
        {
            errno = ENODATA;
            return -1;
        }

        if ( decompress_data ( strm, in, avail, context ) < 0 )
        {
            return -1;
        }

        have = context->u_len < len ? context->u_len : len;
        memcpy ( data, context->unconsumed, have );
        stream->context->crc32 = crc32b ( stream->context->crc32, ( unsigned char * ) data, have );
        context->u_off += have;
        data += have;
        len -= have;
    }

    return 0;
}

/*
 * Finalize zlib output stream
 */
static int zlib_flush ( struct ar_ostream *stream )
{
    struct stream_zlib_context_t *context = ( struct stream_zlib_context_t * ) stream->context;
    z_stream *strm = &context->strm;
    ssize_t len;
    unsigned char out[CHUNK];

    strm->avail_in = 0;
    strm->next_in = NULL;
    strm->avail_out = sizeof ( out );
    strm->next_out = out;

    if ( deflate ( strm, Z_FINISH ) == Z_STREAM_ERROR )
    {
        return -1;
    }

    len = sizeof ( out ) - strm->avail_out;;

    if ( write ( stream->context->fd, out, len ) != len )
    {
        return -1;
    }

    return 0;
}

/*
 * Close zlib stream
 */
static void zlib_close ( struct ar_stream *stream )
{
    struct stream_zlib_context_t *context = ( struct stream_zlib_context_t * ) stream->context;

    if ( context->unconsumed )
    {
        free ( context->unconsumed );
        context->unconsumed = NULL;
    }

    if ( context->strm_allocated )
    {
        deflateEnd ( &context->strm );
        context->strm_allocated = 0;
    }

    generic_close ( stream );
}

/**
 * Zlib stream memory allocate function
 */
static void *zcalloc ( void *opaque, unsigned int items, unsigned int size )
{
    UNUSED ( opaque );
    return malloc ( items * size );
}

/**
 * Zlib stream memory free function
 */
static void zcfree ( void *opaque, void *ptr )
{
    UNUSED ( opaque );
    free ( ptr );
}

/**
 * Open zlib output stream
 */
struct ar_ostream *zlib_ostream_open ( int fd, int level )
{
    struct ar_ostream *stream;
    struct stream_zlib_context_t *context;

    if ( !( stream = ( struct ar_ostream * ) malloc ( sizeof ( struct ar_ostream ) ) ) )
    {
        return NULL;
    }

    if ( !( context =
            ( struct stream_zlib_context_t * ) malloc ( sizeof ( struct
                    stream_zlib_context_t ) ) ) )
    {
        free ( stream );
        return NULL;
    }

    stream->context = ( struct stream_base_context_t * ) context;
    stream->set_header = generic_set_header;
    stream->write = zlib_write;
    stream->flush = zlib_flush;
    stream->seed_crc32 =
        ( void ( * )( struct ar_ostream *, const struct header_t * ) ) generic_seed_crc32;
    stream->finalize_crc32 = ( uint32_t ( * )( struct ar_ostream * ) ) generic_finalize_crc32;
    stream->close = ( void ( * )( struct ar_ostream * ) ) zlib_close;
    context->strm_allocated = 0;

    /* Unconsumed data buffer not allocated yet */
    context->unconsumed = NULL;

    if ( generic_ostream_open ( stream->context, fd ) < 0 )
    {
        stream->close ( stream );
        return NULL;
    }

    /* Allocate deflate state */
    memset ( &context->strm, '\0', sizeof ( context->strm ) );
    context->strm.zalloc = zcalloc;
    context->strm.zfree = zcfree;
    context->strm.opaque = Z_NULL;

    if ( deflateInit ( &context->strm, level ) != Z_OK )
    {
        stream->close ( stream );
        return NULL;
    }

    context->strm_allocated = 1;

    return stream;
}

/**
 * Open zlib input stream
 */
struct ar_istream *zlib_istream_open ( int fd )
{
    struct ar_istream *stream;
    struct stream_zlib_context_t *context;

    if ( !( stream = ( struct ar_istream * ) malloc ( sizeof ( struct ar_istream ) ) ) )
    {
        return NULL;
    }

    if ( !( context =
            ( struct stream_zlib_context_t * ) malloc ( sizeof ( struct
                    stream_zlib_context_t ) ) ) )
    {
        free ( stream );
        return NULL;
    }

    stream->context = ( struct stream_base_context_t * ) context;
    stream->get_header = generic_get_header;
    stream->read = zlib_read;
    stream->seed_crc32 =
        ( void ( * )( struct ar_istream *, const struct header_t * ) ) generic_seed_crc32;
    stream->finalize_crc32 = ( uint32_t ( * )( struct ar_istream * ) ) generic_finalize_crc32;
    stream->close = ( void ( * )( struct ar_istream * ) ) zlib_close;
    context->strm_allocated = 0;

    if ( generic_istream_open ( stream->context, fd ) < 0 )
    {
        stream->close ( stream );
        return NULL;
    }

    /* Unconsumed data buffer not allocated yet */
    context->unconsumed = NULL;

    /* Allocate inflate state */
    context->strm.zalloc = zcalloc;
    context->strm.zfree = zcfree;
    context->strm.opaque = Z_NULL;

    if ( inflateInit ( &context->strm ) != Z_OK )
    {
        stream->close ( stream );
        return NULL;
    }

    context->strm_allocated = 1;
    context->u_off = 0;
    context->u_len = 0;
    context->u_size = 4 * CHUNK;

    if ( !( context->unconsumed = ( unsigned char * ) malloc ( context->u_size ) ) )
    {
        stream->close ( stream );
        return NULL;
    }

    return stream;
}

#endif
