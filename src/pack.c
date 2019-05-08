/* ------------------------------------------------------------------
 * ZBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "zbox.h"

#ifndef EXTRACT_ONLY

/**
 * Calculate entities count
 */
static uint32_t calc_entities ( const struct node_t *node )
{
    if ( !node )
    {
        return 0;
    }

    return calc_entities ( node->sub ) + calc_entities ( node->next ) + 1;
}

/**
 * Calculate names length
 */
static uint32_t calc_nameslen ( const struct node_t *node )
{
    size_t i;
    size_t len;
    const char *basename;

    if ( !node )
    {
        return 0;
    }

    for ( i = 0, basename = node->name, len = strlen ( node->name ); i < len; i++ )
    {
        if ( node->name[i] == '/' )
        {
            basename = node->name + i + 1;
        }
    }

    return len =
        node->name + len - basename + 1 + calc_nameslen ( node->sub ) +
        calc_nameslen ( node->next );
}

/**
 * Store single entity data
 */
static int store_entity ( struct ar_ostream *ostream, const struct entity_t *entity )
{
    struct entity_t entity_net;

    entity_net.parent = htonl ( entity->parent );
    entity_net.mode = htonl ( entity->mode );
    entity_net.size = htonl ( entity->size );

    if ( ostream->write ( ostream, &entity_net, sizeof ( entity_net ) ) < 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Store file and directory information
 */
static int store_entity_table ( struct ar_ostream *ostream, const struct node_t *node )
{
    if ( !node )
    {
        return 0;
    }

    if ( store_entity ( ostream, &node->entity ) < 0 )
    {
        return -1;
    }

    if ( store_entity_table ( ostream, node->sub ) < 0 )
    {
        return -1;
    }

    return store_entity_table ( ostream, node->next );
}

/**
 * Store single file or directory names
 */
static int store_name ( struct ar_ostream *ostream, const char *name )
{
    size_t i;
    size_t len;
    const char *basename = name;

    for ( i = 0, len = strlen ( name ); i < len; i++ )
    {
        if ( name[i] == '/' )
        {
            basename = name + i + 1;
        }
    }

    len = name + len - basename + 1;

    if ( ostream->write ( ostream, basename, len ) < 0 )
    {
        return -1;
    }

    return 0;
}

/**
 * Store file and directory names
 */
static int store_name_table ( struct ar_ostream *ostream, const struct node_t *node )
{
    if ( !node )
    {
        return 0;
    }

    if ( store_name ( ostream, node->name ) < 0 )
    {
        return -1;
    }

    if ( store_name_table ( ostream, node->sub ) < 0 )
    {
        return -1;
    }

    return store_name_table ( ostream, node->next );
}

/**
 * Pack single file to an archive
 */
static int pack_file ( struct pack_context_t *context )
{
    int fd;
    ssize_t len;

    /* Open input file for reading */
    if ( ( fd = open ( context->path, O_RDONLY | O_BINARY ) ) < 0 )
    {
        perror ( context->path );
        return -1;
    }

    /* Store file content into archive */
    while ( ( len = read ( fd, context->workbuf, context->workbuf_size ) ) > 0 )
    {
        if ( context->ostream->write ( context->ostream, context->workbuf, len ) < 0 )
        {
            perror ( "write" );
            close ( fd );
            return -1;
        }
    }

    /* Close file fd */
    close ( fd );

    /* Check if an error occurred */
    if ( len < 0 )
    {
        perror ( context->path );
        return -1;
    }

    /* Show file add success message */
    if ( context->options & OPTION_VERBOSE )
    {
        show_progress ( 'a', context->path );
    }

    return 0;
}

/**
 * Pack multiple files to an archive (internal)
 */
static int pack_files_in ( struct pack_context_t *context, struct node_t *node )
{
    size_t path_len;

    if ( !node )
    {
        return 0;
    }

    path_len = strlen ( context->path );

    if ( path_concat ( context->path, sizeof ( context->path ), node->name ) < 0 )
    {
        return -1;
    }

    if ( ~node->entity.mode & S_IFDIR )
    {
        if ( pack_file ( context ) < 0 )
        {
            return -1;
        }
    }

    if ( pack_files_in ( context, node->sub ) < 0 )
    {
        return -1;
    }

    context->path[path_len] = '\0';

    return pack_files_in ( context, node->next );
}

/**
 * Pack multiple files to an archive
 */
static int pack_files ( uint32_t options, struct ar_ostream *ostream, struct node_t *head )
{
    int retval;
    struct pack_context_t context;

    /* Prepare path buffer */
    context.options = options;
    context.ostream = ostream;
    context.path[0] = '\0';

    /* Allocate work buffer */
    if ( !( context.workbuf = ( unsigned char * ) malloc ( WORKBUF_LIMIT ) ) )
    {
        perror ( "malloc" );
        return -1;
    }

    context.workbuf_size = WORKBUF_LIMIT;

    /* Pack the files */
    retval = pack_files_in ( &context, head );

    /* Free work buffer */
    free ( context.workbuf );

    return retval;
}

/** 
 * Pack files to an archive stream
 */
static int zbox_pack_archive_stream ( uint32_t options, struct ar_ostream *ostream,
    const char *files[], size_t nfiles )
{
    struct header_t header;
    struct node_t *root = NULL;

    /* Prepare archive header */
    memset ( &header, '\0', sizeof ( header ) );

    /* Set achive header identifier */
    header.magic[0] = 'z';
    header.magic[1] = 'b';
    header.magic[2] = 'o';
    header.magic[3] = 'x';

    /* Checksum must be set to zero before calculation */
    header.crc32 = 0;

    /* Set achive compression type */
    if ( options & OPTION_ZLIB )
    {
        header.comp = COMP_ZLIB;

    } else
    {
        header.comp = COMP_NONE;
    }

    /* Build files tree */
    if ( scan_files_tree ( files, nfiles, &root ) < 0 )
    {
        return -1;
    }

    /* Root must be specified */
    if ( !root )
    {
        return -1;
    }

    /* Calculate entities count */
    header.nentity = calc_entities ( root );

    /* Calculate names length */
    header.nameslen = calc_nameslen ( root );

    /* Seed archive stream checksum */
    ostream->seed_crc32 ( ostream, &header );

    /* Store file and directory information */
    if ( store_entity_table ( ostream, root ) < 0 )
    {
        free_files_tree ( root, 1 );
        return -1;
    }

    /* Store file and directory names */
    if ( store_name_table ( ostream, root ) < 0 )
    {
        free_files_tree ( root, 1 );
        return -1;
    }

    /* Store multiple files to an archive */
    if ( pack_files ( options, ostream, root ) < 0 )
    {
        free_files_tree ( root, 1 );
        return -1;
    }

    /* Flush archive stream */
    if ( ostream->flush ( ostream ) < 0 )
    {
        free_files_tree ( root, 1 );
        return -1;
    }

    /* Update archive header checksum */
    header.crc32 = ostream->finalize_crc32 ( ostream );

    /* Update archive header */
    if ( ostream->set_header ( ostream, &header ) < 0 )
    {
        free_files_tree ( root, 1 );
        return -1;
    }

    /* Free files tree */
    free_files_tree ( root, 1 );

    return 0;
}

/** 
 * Pack files to an archive
 */
int zbox_pack_archive ( const char *archive, uint32_t options, int level, const char *files[],
    size_t nfiles )
{
    int fd;
    int status;
    struct ar_ostream *ostream;

    /* Open archive file for writing */
    if ( ( fd = open ( archive, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0644 ) ) < 0 )
    {
        perror ( archive );
        return -1;
    }

    /* Open archive stream */
    if ( options & OPTION_ZLIB )
    {
#ifdef ENABLE_ZLIB
        ostream = zlib_ostream_open ( fd, level );
#else
        UNUSED ( level );
        fprintf ( stderr, "zlib not enabled.\n" );
        errno = EINVAL;
        close ( fd );
        return -1;
#endif
    } else
    {
        ostream = plain_ostream_open ( fd );
    }

    /* Check if an error occurred */
    if ( !ostream )
    {
        close ( fd );
        return -1;
    }

    /* Pack files into archive */
    status = zbox_pack_archive_stream ( options, ostream, files, nfiles );

    /* Close archive stream */
    ostream->close ( ostream );
    close ( fd );

    return status;
}

#endif
