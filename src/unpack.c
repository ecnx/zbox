/* ------------------------------------------------------------------
 * ZBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "zbox.h"

/**
 * Parse single entity from archive metadata
 */
static int unpack_parse_entity ( struct unpack_parse_context *context, struct node_t **head )
{
    struct node_t *node;

    if ( context->position >= context->count )
    {
        return 0;
    }

    if ( context->name_table >= context->name_limit )
    {
        return -1;
    }

    if ( !( node = node_append ( head ) ) )
    {
        return -1;
    }

    node->name = ( char * ) context->name_table;
    context->name_table += strlen ( node->name ) + 1;
    memcpy ( &node->entity, &context->entity_table[context->position], sizeof ( struct entity_t ) );
    context->position++;

    if ( node->entity.mode & S_IFDIR )
    {
        while ( context->entity_table[context->position].parent == node->entity.id )
        {
            unpack_parse_entity ( context, &node->sub );
        }
    }

    if ( context->position < context->count && !node->entity.parent &&
        !context->entity_table[context->position].parent )
    {
        return unpack_parse_entity ( context, &node->next );
    }

    return 0;
}

/** 
 * Parse archive nodes into memory
 */
static int zbox_unpack_parse ( const struct header_t *header, const struct entity_t *entity_table,
    const char *name_table, struct node_t **root )
{
    const char *name_limit;
    struct unpack_parse_context context;

    /* At least one name required */
    if ( !header->nameslen )
    {
        return -1;
    }

    name_limit = name_table + header->nameslen;

    /* Name table must ends with zero byte */
    if ( name_limit[-1] != '\0' )
    {
        return -1;
    }

    /* Prepare parse context */
    context.entity_table = entity_table;
    context.name_table = name_table;
    context.name_limit = name_limit;
    context.count = header->nentity;
    context.position = 0;

    /* Parse files tree */
    return unpack_parse_entity ( &context, root );
}

/**
 * Extract single archive iles
 */
static int zbox_extract_file ( struct unpack_context_t *context, struct entity_t *entity )
{
    int fd;
    ssize_t len;
    uint32_t left;

    /* Show only filename if list only mode selected */
    if ( context->options & OPTION_LISTONLY && ~entity->mode & S_IFDIR )
    {
        show_progress ( 'l', context->path );
        return 0;
    }

    /* Create a directory if needed */
    if ( entity->mode & S_IFDIR )
    {
        if ( context->options & OPTION_TESTONLY )
        {
            return 0;
        }
#ifndef WIN32_BUILD
        if ( mkdir ( context->path, entity->mode & ( ~S_IFMT ) ) < 0 && errno != EEXIST )
        {
            return -1;
        }
#else
        if ( mkdir ( context->path ) < 0 && errno != EEXIST )
        {
            return -1;
        }
#endif

        return 0;
    }

    /* Set needed data length */
    left = entity->size;

    /* Update archive checksum only if needed */
    if ( context->options & OPTION_TESTONLY )
    {
        while ( ( len = left > context->workbuf_size ? context->workbuf_size : left ) > 0 )
        {
            if ( context->istream->read ( context->istream, context->workbuf, len ) < 0 )
            {
                return -1;
            }

            left -= len;
        }

        if ( context->options & OPTION_VERBOSE )
        {
            show_progress ( 't', context->path );
        }

        return 0;
    }

    /* Open input file for reading */
    if ( ( fd =
            open ( context->path, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY,
                entity->mode & ( ~S_IFMT ) ) ) < 0 )
    {
        perror ( context->path );
        return -1;
    }

    /* Store file content into archive */
    while ( ( len = left > context->workbuf_size ? context->workbuf_size : left ) > 0 )
    {
        if ( context->istream->read ( context->istream, context->workbuf, len ) < 0 )
        {
            close ( fd );
            return -1;
        }

        if ( write ( fd, context->workbuf, len ) < 0 )
        {
            perror ( "write" );
            close ( fd );
            return -1;
        }

        left -= len;
    }

    /* Close file fd */
    close ( fd );

    /* All bytes must have been written to file */
    if ( left )
    {
        errno = ENODATA;
        return -1;
    }

    if ( context->options & OPTION_VERBOSE )
    {
        show_progress ( context->options & OPTION_NOPATHS ? 'e' : 'x', context->path );
    }

    return 0;
}

/**
 * Check if file path is forbidden
 */
static int check_forbidden ( const char *path )
{
    return !path[0] || strchr ( path, '/' ) || strchr ( path, '\\' ) || !strcmp ( path, ".." );
}

/** 
 * Manage archive extract process
 */
static int zbox_extract_next ( struct unpack_context_t *context, struct node_t *node )
{
    size_t path_len = 0;

    if ( !node )
    {
        return 0;
    }

    if ( check_forbidden ( node->name ) )
    {
        errno = EINVAL;
        perror ( node->name );
        return -1;
    }

    if ( ~context->options & OPTION_NOPATHS || ~node->entity.mode & S_IFDIR )
    {
        path_len = strlen ( context->path );

        if ( path_concat ( context->path, sizeof ( context->path ), node->name ) < 0 )
        {
            return -1;
        }

        if ( zbox_extract_file ( context, &node->entity ) < 0 )
        {
            return -1;
        }
    }

    if ( zbox_extract_next ( context, node->sub ) < 0 )
    {
        return -1;
    }

    if ( ~context->options & OPTION_NOPATHS || ~node->entity.mode & S_IFDIR )
    {
        context->path[path_len] = '\0';
    }

    return zbox_extract_next ( context, node->next );
}

/** 
 * Load metadata of archive files
 */
static int zbox_unpack_archive_load ( struct header_t *header, struct ar_istream *istream,
    uint32_t options )
{
    int status = 0;
    uint32_t crc32_backup;
    uint32_t crc32_recalc;
    size_t i;
    size_t entity_table_size;
    struct entity_t *entity_table;
    char *name_table;
    struct node_t *root = NULL;
    struct unpack_context_t context;

    /* Validate header magic field */
    if ( header->magic[0] != 'z' || header->magic[1] != 'b'
        || header->magic[2] != 'o' || header->magic[3] != 'x' )
    {
        fprintf ( stderr, "archive not recognized.\n" );
        errno = EINVAL;
        return -1;
    }

    /* At least one entity required */
    if ( !header->nentity )
    {
        return 0;
    }

    /* Seed archive stream checksum */
    crc32_backup = header->crc32;
    header->crc32 = 0;

    istream->seed_crc32 ( istream, header );

    /* Calculate entity table size */
    entity_table_size = header->nentity * sizeof ( struct entity_t );

    /* Allocate entity table */
    if ( !( entity_table = ( struct entity_t * ) malloc ( entity_table_size ) ) )
    {
        perror ( "malloc" );
        return -1;
    }

    /* Read entity table */
    if ( istream->read ( istream, entity_table, entity_table_size ) < 0 )
    {
        free ( entity_table );
        return -1;
    }

    /* Convert entities to host byte order */
    for ( i = 0; i < header->nentity; i++ )
    {
        entity_table[i].parent = ntohl ( entity_table[i].parent );
        entity_table[i].mode = ntohl ( entity_table[i].mode );
        entity_table[i].size = ntohl ( entity_table[i].size );
    }

    /* At least one name required */
    if ( !header->nameslen )
    {
        errno = EINVAL;
        free ( entity_table );
        return -1;
    }

    /* Allocate name table */
    if ( !( name_table = ( char * ) malloc ( header->nameslen ) ) )
    {
        perror ( "malloc" );
        free ( entity_table );
        return -1;
    }

    /* Read name table */
    if ( istream->read ( istream, name_table, header->nameslen ) < 0 )
    {
        free ( entity_table );
        free ( name_table );
        return -1;
    }

    /* Parse archive nodes into memory */
    if ( zbox_unpack_parse ( ( const struct header_t * ) header,
            ( const struct entity_t * ) entity_table, ( const char * ) name_table, &root ) < 0 )
    {
        free ( entity_table );
        free ( name_table );
        free_files_tree ( root, 0 );
        return -1;
    }

    /* Entity table no longer needed */
    free ( entity_table );

    /* Prepare path buffer */
    context.options = options;
    context.istream = istream;
    context.path[0] = '\0';

    /* Allocate work buffer */
    if ( !( context.workbuf = ( unsigned char * ) malloc ( WORKBUF_LIMIT ) ) )
    {
        perror ( "malloc" );
        free ( entity_table );
        free ( name_table );
        free_files_tree ( root, 0 );
        return -1;
    }

    context.workbuf_size = WORKBUF_LIMIT;

    /* Extract files */
    status = zbox_extract_next ( &context, root );

    /* Free work buffer */
    free ( context.workbuf );

    /* Free name tables */
    free ( name_table );

    /* Free files tree */
    free_files_tree ( root, 0 );

    /* Validate archive checksum if needed */
    if ( !status )
    {
        crc32_recalc = istream->finalize_crc32 ( istream );

        if ( ~options & OPTION_LISTONLY && crc32_backup != crc32_recalc )
        {
            fprintf ( stderr, "archive checksum: bad\n" );
            errno = EINVAL;
            status = -1;

        } else if ( options & OPTION_TESTONLY && crc32_backup == crc32_recalc )
        {
            printf ( "archive checksum: ok\n" );
        }
    }

    return status;
}

/** 
 * Unpack files from an archive
 */
int zbox_unpack_archive ( const char *archive, uint32_t options )
{
    int fd;
    int status;
    struct ar_istream *istream;
    struct header_t header;

    /* Open archive file for reading */
    if ( ( fd = open ( archive, O_RDONLY | O_BINARY ) ) < 0 )
    {
        perror ( archive );
        return -1;
    }

    /* Open archive stream */
    if ( !( istream = plain_istream_open ( fd ) ) )
    {
        close ( fd );
        return -1;
    }

    /* Get archive header */
    if ( istream->get_header ( istream, &header ) < 0 )
    {
        return -1;
    }

    /* Re-open archive stream if needed */
    if ( header.comp == COMP_ZLIB )
    {
        istream->close ( istream );
#ifdef ENABLE_ZLIB
        if ( !( istream = zlib_istream_open ( fd ) ) )
        {
            close ( fd );
            return -1;
        }
#else
        fprintf ( stderr, "zlib not enabled.\n" );
        errno = EINVAL;
        close ( fd );
        return -1;
#endif
    }

    /* Load archive metadata and unpack */
    status = zbox_unpack_archive_load ( &header, istream, options );

    /* Close archive stream */
    istream->close ( istream );
    close ( fd );

    return status;
}
