/* ------------------------------------------------------------------
 * ZBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "zbox.h"

/**
 * Append a new node at the begin of linked list
 */
struct node_t *node_insert ( struct node_t **head )
{
    struct node_t *node;

    if ( !( node = ( struct node_t * ) malloc ( sizeof ( struct node_t ) ) ) )
    {
        return NULL;
    }

    node->next = *head;
    node->sub = NULL;
    *head = node;

    return node;
}

/**
 * Append a new node to the end of linked list
 */
struct node_t *node_append ( struct node_t **head )
{
    struct node_t *node;
    struct node_t *last;

    if ( !( node = ( struct node_t * ) malloc ( sizeof ( struct node_t ) ) ) )
    {
        return NULL;
    }

    node->next = NULL;
    node->sub = NULL;

    if ( *head )
    {
        last = *head;
        while ( last->next )
        {
            last = last->next;
        }
        last->next = node;

    } else
    {
        *head = node;
    }

    return node;
}

/**
 * Concatenate paths together
 */
int path_concat ( char *path, size_t path_size, const char *name )
{
    size_t path_len;
    size_t name_len;

    path_len = strlen ( path );
    name_len = strlen ( name );

    if ( path_len )
    {
        if ( path_len + 1 + name_len >= path_size )
        {
            errno = ENAMETOOLONG;
            perror ( name );
            return -1;
        }

        path[path_len] = '/';
        memcpy ( path + path_len + 1, name, name_len + 1 );

    } else
    {
        if ( name_len >= path_size )
        {
            errno = ENAMETOOLONG;
            perror ( name );
            return -1;
        }

        memcpy ( path, name, name_len + 1 );
    }

    return 0;
}

/**
 * Find filesystem separator in path
 */
static char *findsep ( const char *path )
{
    return strchr ( path, '/' );
}

/**
 * Check if name equals with path parent node
 */
static int filter_check ( char *filter, const char *name )
{
    int ret;
    char *sep;
    char backup;

    if ( ( sep = findsep ( filter ) ) )
    {
        backup = *sep;
        *sep = '\0';
    }

    ret = !strcmp ( filter, name );

    if ( sep )
    {
        *sep = backup;
    }

    return ret;
}

/**
 * Scan input files tree
 */
static int scan_files ( const char *name, uint32_t parent_id, struct scan_context_t *context,
    struct node_t **head )
{
    int status = 0;
    size_t path_len;
    size_t name_len;
    DIR *dir;
    char *filter = NULL;
    struct dirent *entry;
    struct stat statbuf;
    struct node_t *node;

    path_len = strlen ( context->path );
    name_len = strlen ( name );

    if ( path_concat ( context->path, sizeof ( context->path ), name ) < 0 )
    {
        return -1;
    }

    if ( context->filter )
    {
        filter = context->filter;
        if ( !filter_check ( context->filter, name ) )
        {
            context->path[path_len] = '\0';
            return 0;
        }

        if ( ( context->filter = findsep ( context->filter ) ) )
        {
            context->filter++;
        }
    }

    if ( stat ( context->path, &statbuf ) < 0 )
    {
        perror ( context->path );

        if ( errno == ENOENT )
        {
            context->path[path_len] = '\0';

            if ( filter )
            {
                context->filter = filter;
            }

            return 0;
        }

        return -1;
    }

    if ( !( node = node_insert ( head ) ) )
    {
        return -1;
    }

    if ( !( node->name = ( char * ) malloc ( name_len + 1 ) ) )
    {
        return -1;
    }

    memcpy ( node->name, name, name_len + 1 );
    node->entity.parent = parent_id;
    node->entity.mode = statbuf.st_mode;

    if ( ~statbuf.st_mode & S_IFDIR )
    {
        node->entity.size = statbuf.st_size;
        context->path[path_len] = '\0';
        context->filter = filter;
        return 0;
    }

    node->entity.id = context->next_id++;

    if ( !( dir = opendir ( context->path ) ) )
    {
        perror ( context->path );
        return -1;
    }

    while ( ( entry = readdir ( dir ) ) )
    {
        if ( !strcmp ( entry->d_name, "." ) || !strcmp ( entry->d_name, ".." ) )
        {
            continue;
        }

        if ( scan_files ( entry->d_name, node->entity.id, context, &node->sub ) < 0 )
        {
            status = -1;
            break;
        }
    }

    closedir ( dir );

    context->path[path_len] = '\0';

    if ( filter )
    {
        context->filter = filter;
    }

    return status;
}

/**
 * Prepare input files tree scan (internal)
 */
static int begin_scan_files_in ( const char *path, uint32_t parent_id,
    struct scan_context_t *context, struct node_t **head )
{
    size_t len;
    char *sep;
    char filter[PATH_LIMIT];
    char first[PATH_LIMIT];
    struct stat statbuf;

    /* Ensure that file exists */
    if ( stat ( path, &statbuf ) < 0 )
    {
        perror ( path );
        return -1;
    }

    /* Validate filter buffer size */
    if ( ( len = strlen ( path ) ) >= sizeof ( filter ) )
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    /* Prepare scan filter path */
    memcpy ( filter, path, len + 1 );

    while ( len > 1 && filter[len - 1] == '/' )
    {
        filter[len - 1] = '\0';
        len = strlen(filter);
    }

    /* Reset current path */
    context->path[0] = '\0';

    /* Skip entity path if out of current directory */
    if ( filter[0] == '/' || strstr ( filter, ".." ) )
    {
        context->filter = NULL;
        return scan_files ( filter, parent_id, context, head );
    }

    /* Skip dot slash prefix */
    while ( path[0] == '.' && findsep ( path ) == path + 1 )
    {
        path += 2;
    }

    /* Prepare scan filter to preserve resource path */
    memcpy ( first, path, len + 1 );
    context->filter = filter;

    if ( ( sep = findsep ( first ) ) )
    {
        *sep = '\0';
    }

    return scan_files ( first, parent_id, context, head );
}

/**
 * Prepare input files tree scan
 */
static int begin_scan_files ( const char *path, uint32_t parent_id, struct scan_context_t *context,
    struct node_t **head )
{
    size_t i;
    size_t len;
    char spath[PATH_LIMIT];

    /* Validate filter buffer size */
    if ( ( len = strlen ( path ) ) >= sizeof ( spath ) )
    {
        errno = ENAMETOOLONG;
        perror ( path );
        return -1;
    }

    /* Replace each backslash with slash */
    for ( i = 0; i <= len; i++ )
    {
        spath[i] = path[i] == '\\' ? '/' : path[i];
    }

    return begin_scan_files_in ( spath, parent_id, context, head );
}

/**
 * Scan files tree for archive building (internal)
 */
int scan_files_tree_in ( const char *files[], size_t nfiles, struct scan_context_t *context,
    struct node_t **root )
{
    /* Stop if there are no files more to pack */
    if ( !nfiles )
    {
        return 0;
    }

    /* Scan input files tree */
    if ( begin_scan_files ( files[0], 0, context, root ) < 0 )
    {
        return -1;
    }

    /* Scan for next file tree */
    return scan_files_tree_in ( files + 1, nfiles - 1, context, &( *root )->next );
}

/**
 * Scan files tree for archive building
 */
int scan_files_tree ( const char *files[], size_t nfiles, struct node_t **root )
{
    struct scan_context_t context;

    /* Prepare scan context */
    context.next_id = 1;

    return scan_files_tree_in ( files, nfiles, &context, root );
}

/**
 * Free node list from memory, also free node names
 */
static void free_files_tree_names ( struct node_t *node )
{
    if ( !node )
    {
        return;
    }

    free_files_tree_names ( node->sub );
    free_files_tree_names ( node->next );

    if ( node->name )
    {
        free ( node->name );
    }

    free ( node );
}

/**
 * Free node list from memory
 */
static void free_files_tree_no_names ( struct node_t *node )
{
    if ( !node )
    {
        return;
    }

    free_files_tree_no_names ( node->sub );
    free_files_tree_no_names ( node->next );

    free ( node );
}

/**
 * Free node list from memory, also free node names if needed
 */
void free_files_tree ( struct node_t *root, int dynamic_names )
{
    if ( dynamic_names )
    {
        free_files_tree_names ( root );

    } else
    {
        free_files_tree_no_names ( root );
    }
}
