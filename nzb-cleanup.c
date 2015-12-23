
#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>

#include <ftw.h>

#define kHash_mp4       0x007A530981FEBBAA
#define kHash_m4v       0x007A6D3158CF00AC
#define kHash_mkv       0x007A702376D830F6
#define kHash_mpeg      0xEC59D09162FE6667
#define kHash_mpg       0x007A530653BC5F32
#define kHash_avi       0x006D0241B8BE6BDE
#define kHash_wmv       0x0085D1C916AEEA68

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

typedef unsigned long tHash;
typedef struct stat tStat;

static char *myName;


/*
    Bob Jenkin's 'One at a Time' hash
*/
tHash hashStr( const char *key )
{
    register const char *p = key;
    unsigned long h = 0;

    while (*p != '\0')
    {
        h += tolower((unsigned char)*p++);
        h += (h << 10);
        h ^= (h >> 6);
    }

    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);

    if (h == 0)
        h = 0xdeadbeef; /* reserve zero as a flag value */

    return h;
}

tHash hashExt( const char *filename )
{
    int result = 0;
    const char *p;

    p = strrchr( filename, '.' );

    if (p != NULL)
        { result = hashStr(++p); }

    return result;
}

int isVisible( const char *filename )
{
    const char *p;

    p = strrchr( filename, '/' );

    if ( p != NULL ) { ++p; }
    else { p = filename; }

    return ( p[0] != '.' );
}

int isVideoFile( const char *filename )
{
    switch ( hashExt( filename ) )
    {
        case kHash_mp4:
        case kHash_m4v:
        case kHash_mkv:
        case kHash_mpeg:
        case kHash_mpg:
        case kHash_avi:
        case kHash_wmv:
            return 1;

        default:
            return 0;
    }
}

void printHashes( void )
{
static const char *exts[] = { "mp4", "m4v", "mkv", "mpeg", "mpg", "avi", "wmv", NULL };
    const char **ext = exts;

    printf("\n");
    while ( *ext != NULL )
    {
        printf( "#define kHash_%s\t0x%016lX\n", *ext, hashStr(*ext) );
        ext++;
    }

    ext = exts;
    while ( *ext != NULL )
    {
        printf( "\tcase kHash_%s:\n", *ext );
        ext++;
    }
}

void parseName( const char *path )
{
    char *p, *dir, *fn;
    if (path == NULL)
        return;

    p = strdup(path);
    dir = p;
    fn  = p;
    while (*p != '\0') 
    {
        if (*p == '/') { fn = p; }
        ++p;
    }

    if (dir != fn) { *fn++ = '\0'; }
    else { dir = "."; }

    printf("\tp: %s\n\tf: %s\n", dir, fn );
}

void processFile( const char *filename, const tStat * UNUSED(fileStat) )
{
    printf("     file: %s\n", filename);
    parseName( filename );
}

void processDirectory( const char *directory, const tStat * UNUSED(dirStat) )
{
    printf("directory: %s\n", directory);
    parseName( directory );
}

int processEntry( const char *path, const struct stat * UNUSED(fileStat), const int typeflag, struct FTW *ftwbuf )
{
    const char *typ;

    switch ( typeflag )
    {
    case FTW_F: // path is a regular file.
        typ = "File";
        //if ( isVisible( path ) && isVideoFile( path ) )
        //    { processFile( path, fileStat ); }
        break;

    case FTW_D: // path is a directory.
        typ = "Dir ";
        break;

    case FTW_DP: // path is a directory.
        typ = "DirP";
        //if ( isVisible( path ) )
        //    { processDirectory( path, fileStat ); }
        break;

    default: // everything else isn't useful
        typ = "othr";
        break;
    }

    printf("%s, depth %d, path[%3d] = \'%s\'\n", typ, ftwbuf->level, ftwbuf->base, &path[ftwbuf->base] );

    return (0);
}

int main( const int argc, const char *argv[] )
{
    char   *p;
    
    p = strdup( argv[0] );
    myName = strdup( basename( p ) );
    free( p );

    if (myName == NULL )
        { myName = "<unknown>"; }
        
//    printf( "my name is \'%s\'\n", myName );

    for ( int i = 1; i < argc; ++i )
    {
        nftw( argv[i], processEntry, 10, FTW_DEPTH | FTW_CHDIR | FTW_PHYS );         
    }

    return (0);
}
