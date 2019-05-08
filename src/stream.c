/* ------------------------------------------------------------------
 * ZBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "zbox.h"

/**
 * Open generic output stream
 */
int generic_ostream_open ( struct stream_base_context_t *context, int fd )
{
    context->fd = fd;

    if ( ftruncate ( context->fd, sizeof ( struct header_t ) ) < 0 )
    {
        return -1;
    }

    if ( lseek ( context->fd, sizeof ( struct header_t ), SEEK_SET ) < 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Open generic input stream
 */
int generic_istream_open ( struct stream_base_context_t *context, int fd )
{
    context->fd = fd;

    if ( lseek ( context->fd, sizeof ( struct header_t ), SEEK_SET ) < 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Archive header host to network byte order change
 */
static void header_hton ( const struct header_t *header, struct header_t *net_header )
{
    memset ( net_header, '\0', sizeof ( struct header_t ) );
    memcpy ( net_header->magic, header->magic, sizeof ( net_header->magic ) );
    net_header->comp = htonl ( header->comp );
    net_header->nentity = htonl ( header->nentity );
    net_header->nameslen = htonl ( header->nameslen );
    net_header->crc32 = htonl ( header->crc32 );
}

/**
 * Archive header network to host byte order change
 */
static void header_ntoh ( const struct header_t *net_header, struct header_t *header )
{
    memset ( header, '\0', sizeof ( struct header_t ) );
    memcpy ( header->magic, net_header->magic, sizeof ( header->magic ) );
    header->comp = ntohl ( net_header->comp );
    header->nentity = ntohl ( net_header->nentity );
    header->nameslen = ntohl ( net_header->nameslen );
    header->crc32 = ntohl ( net_header->crc32 );
}

/** 
 * Generic put archive header
 */
int generic_set_header ( struct ar_ostream *stream, const struct header_t *header )
{
    off_t offset_backup;
    struct header_t net_header;

    header_hton ( header, &net_header );

    if ( ( offset_backup = lseek ( stream->context->fd, 0, SEEK_CUR ) ) < 0 )
    {
        return -1;
    }

    if ( lseek ( stream->context->fd, 0, SEEK_SET ) < 0 )
    {
        return -1;
    }

    if ( write ( stream->context->fd, &net_header,
            sizeof ( net_header ) ) != sizeof ( net_header ) )
    {
        return -1;
    }

    if ( lseek ( stream->context->fd, offset_backup, SEEK_SET ) < 0 )
    {
        return -1;
    }

    return 0;
}

/** 
 * Generic get archive header
 */
int generic_get_header ( struct ar_istream *stream, struct header_t *header )
{
    off_t offset_backup;
    struct header_t net_header;

    if ( ( offset_backup = lseek ( stream->context->fd, 0, SEEK_CUR ) ) < 0 )
    {
        return -1;
    }

    if ( lseek ( stream->context->fd, 0, SEEK_SET ) < 0 )
    {
        return -1;
    }

    if ( read ( stream->context->fd, &net_header, sizeof ( net_header ) ) != sizeof ( net_header ) )
    {
        return -1;
    }

    if ( lseek ( stream->context->fd, offset_backup, SEEK_SET ) < 0 )
    {
        return -1;
    }

    header_ntoh ( &net_header, header );

    return 0;
}

/**
 * Write data to output stream
 */
int generic_write ( struct ar_ostream *stream, const void *data, size_t len )
{
    size_t ret;

    stream->context->crc32 = crc32b ( stream->context->crc32, ( const unsigned char * ) data, len );

    if ( ( ret = write ( stream->context->fd, data, len ) ) == ( size_t ) - 1 )
    {
        return -1;
    }

    if ( ret != len )
    {
        return -1;
    }

    return 0;
}

/**
 * Read data from input stream
 */
int generic_read ( struct ar_istream *stream, void *data, size_t len )
{
    size_t ret;

    if ( ( ret = read ( stream->context->fd, data, len ) ) == ( size_t ) - 1 )
    {
        return -1;
    }

    if ( ret != len )
    {
        return -1;
    }

    stream->context->crc32 = crc32b ( stream->context->crc32, ( unsigned char * ) data, len );

    return 0;
}

/*
 * Finalize output stream
 */
int generic_flush ( struct ar_ostream *stream )
{
    UNUSED ( stream );
    return 0;
}

/*
 * Set crc32 checksum for stream
 */
void generic_seed_crc32 ( struct ar_stream *stream, const struct header_t *header )
{
    struct header_t net_header;

    header_hton ( header, &net_header );
    stream->context->crc32 =
        crc32b ( 0xffffffff, ( unsigned char * ) &net_header, sizeof ( net_header ) );
}

/*
 * Get crc32 checksum from stream
 */
uint32_t generic_finalize_crc32 ( struct ar_stream *stream )
{
    return ~stream->context->crc32;
}

/*
 * Close stream
 */
void generic_close ( struct ar_stream *stream )
{
    if ( stream )
    {
        free ( stream->context );
        free ( stream );
    }
}

/**
 * Open plain output stream
 */
struct ar_ostream *plain_ostream_open ( int fd )
{
    struct ar_ostream *stream;

    if ( !( stream = ( struct ar_ostream * ) malloc ( sizeof ( struct ar_ostream ) ) ) )
    {
        return NULL;
    }

    if ( !( stream->context =
            ( struct stream_base_context_t * ) malloc ( sizeof ( struct
                    stream_base_context_t ) ) ) )
    {
        free ( stream );
        return NULL;
    }

    stream->set_header = generic_set_header;
    stream->write = generic_write;
    stream->flush = generic_flush;
    stream->seed_crc32 =
        ( void ( * )( struct ar_ostream *, const struct header_t * ) ) generic_seed_crc32;
    stream->finalize_crc32 = ( uint32_t ( * )( struct ar_ostream * ) ) generic_finalize_crc32;
    stream->close = ( void ( * )( struct ar_ostream * ) ) generic_close;

    if ( generic_ostream_open ( stream->context, fd ) < 0 )
    {
        stream->close ( stream );
        return NULL;
    }

    return stream;
}

/**
 * Open plain input stream
 */
struct ar_istream *plain_istream_open ( int fd )
{
    struct ar_istream *stream;

    if ( !( stream = ( struct ar_istream * ) malloc ( sizeof ( struct ar_istream ) ) ) )
    {
        return NULL;
    }

    if ( !( stream->context =
            ( struct stream_base_context_t * ) malloc ( sizeof ( struct
                    stream_base_context_t ) ) ) )
    {
        free ( stream );
        return NULL;
    }

    stream->get_header = generic_get_header;
    stream->read = generic_read;
    stream->seed_crc32 =
        ( void ( * )( struct ar_istream *, const struct header_t * ) ) generic_seed_crc32;
    stream->finalize_crc32 = ( uint32_t ( * )( struct ar_istream * ) ) generic_finalize_crc32;
    stream->close = ( void ( * )( struct ar_istream * ) ) generic_close;

    if ( generic_istream_open ( stream->context, fd ) < 0 )
    {
        stream->close ( stream );
        return NULL;
    }

    return stream;
}
