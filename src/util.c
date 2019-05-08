/* ------------------------------------------------------------------
 * ZBox - Simple Data Achive Utility
 * ------------------------------------------------------------------ */

#include "zbox.h"

/**
 * Show operation progress with current file path
 */
void show_progress ( char action, const char *path )
{
    printf ( " %c %s\n", action, path );
}

