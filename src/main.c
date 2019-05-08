/* ------------------------------------------------------------------
 * ZBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "zbox.h"

/**
 * Show program usage message
 */
static void show_usage ( void )
{
    fprintf ( stderr, "usage: zbox -{cxelth}[snb0..9] archive [path]\n"
        "\n"
        "version: " ZBOX_VERSION "\n"
        "\n"
        "options:\n"
        "  -c    create new archive\n"
        "  -x    extract archive\n"
        "  -e    extract archive, no paths\n"
        "  -l    list only files in archive\n"
        "  -t    check archive checksum\n"
        "  -h    show help message\n"
        "  -s    skip additional info\n"
        "  -n    turn off zlib compression\n"
        "  -b    use best compression ratio\n" "  -0..9 preset compression ratio\n" "\n" );
}

/** 
 * Check if flag is set
 */
static int check_flag ( const char *str, char flag )
{
    return strchr ( str, flag ) != NULL;
}

/**
 * Program entry point
 */
int main ( int argc, char *argv[] )
{
    int status = 0;
#ifndef EXTRACT_ONLY
    int level = 6;
#endif
    uint32_t options = OPTION_VERBOSE | OPTION_ZLIB;
    int flag_c;
    int flag_x;
    int flag_e;
    int flag_l;
    int flag_t;
    int flag_s;
    int flag_n;

    /* Validate arguments count */
    if ( argc < 3 )
    {
        show_usage (  );
        return 1;
    }

    /* Parse flags from arguments */
    flag_c = check_flag ( argv[1], 'c' );
    flag_x = check_flag ( argv[1], 'x' );
    flag_e = check_flag ( argv[1], 'e' );
    flag_l = check_flag ( argv[1], 'l' );
    flag_t = check_flag ( argv[1], 't' );
    flag_s = check_flag ( argv[1], 's' );
    flag_n = check_flag ( argv[1], 'n' );

    /* Validate selected tasks count */
    if ( flag_c + flag_x + flag_e + flag_l + flag_t != 1 )
    {
        show_usage (  );
        return 1;
    }

    /* Unset verbose if silent mode flag set */
    if ( flag_s )
    {
        options &= ~OPTION_VERBOSE;
    }

    /* Set list only option if needed */
    if ( flag_l )
    {
        options |= OPTION_LISTONLY;
    }

    /* Set test only option if needed */
    if ( flag_t )
    {
        options |= OPTION_TESTONLY;
    }

    /* Set no paths option if needed */
    if ( flag_e )
    {
        options |= OPTION_NOPATHS;
    }

    /* Set no paths option if needed */
    if ( flag_n )
    {
        options &= ~OPTION_ZLIB;
    }

#ifndef EXTRACT_ONLY
    /* Adjust compression level */
    if ( strchr ( argv[1], '0' ) )
    {
        level = 0;

    } else if ( strchr ( argv[1], '1' ) )
    {
        level = 1;

    } else if ( strchr ( argv[1], '2' ) )
    {
        level = 2;

    } else if ( strchr ( argv[1], '3' ) )
    {
        level = 3;

    } else if ( strchr ( argv[1], '4' ) )
    {
        level = 4;

    } else if ( strchr ( argv[1], '5' ) )
    {
        level = 5;

    } else if ( strchr ( argv[1], '6' ) )
    {
        level = 6;

    } else if ( strchr ( argv[1], '7' ) )
    {
        level = 7;

    } else if ( strchr ( argv[1], '8' ) )
    {
        level = 8;

    } else if ( strchr ( argv[1], '9' ) || strchr ( argv[1], 'b' ) )
    {
        level = 9;
    }
#endif

    /* Perform appriopriate action */
    if ( flag_c )
    {
        if ( argc < 4 )
        {
            show_usage (  );
            return 1;
        }
#ifndef EXTRACT_ONLY
        status =
            zbox_pack_archive ( argv[2], options, level, ( const char ** ) ( argv + 3 ), argc - 3 );
#else

        fprintf ( stderr, "archive create not enabled.\n" );
        status = -1;
#endif

    } else if ( flag_x || flag_e || flag_l || flag_t )
    {
        status = zbox_unpack_archive ( argv[2], options );
    }

    /* Show failure message if needed */
    if ( status < 0 )
    {
        fprintf ( stderr, "failure: %i\n", errno ? errno : -1 );
    }

    return status < 0;
}
