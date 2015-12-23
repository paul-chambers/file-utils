

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

#define kHash_mp4       33457
#define kHash_m4v       32503
#define kHash_mkv       33438
#define kHash_mpeg      569705
#define kHash_mpg       33508
#define kHash_avi       30144
#define kHash_wmv       36362

static int  verbose = 0;

static const char  *inputPath  = NULL;
static const char  *outputPath = NULL;
static const char  *outputExt  = NULL;

static char        *myName;
static int          childArgc;
static const char **childArgv;
static char         workDir[PATH_MAX];

typedef struct stat tStat;


int hashStr( const char *p )
{
    int result = 0;

    while ( *p != '\0' )
        { result = (result * 17) + tolower(*p++); }
    return result;
}

int hashExt( const char *filename )
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
    const char *p = strrchr( filename, '/' );
    return (p != NULL && p[1] != '.');
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

int makeMissingDirs( char *path )
{
    int     r;
    tStat   sb;
    char   *p, *q;

    if ( stat( path, &sb ) == -1 )
    { // stat failed - why?
        switch (errno)
        {
        case ENOENT:
            p = strdup( path );
            q = strrchr( p,'/' );
            r = 0;
            if ( q != NULL && p != q )
            {
                *q = '\0';  // truncate last element
                r = makeMissingDirs( p ); // try the parent element
                free( p );
                if ( r == 0) {
                    if (verbose)
                        { printf( "creating directory \'%s\'\n", path ); }

                    if ( mkdir( path, S_IRWXU | S_IRWXG ) == -1 ) // try to creat the next path element
                        { r = errno; }
                }
                if ( r != 0 )
                    { fprintf( stderr, "### %s: unable to create directory \'%s\' (%d: %s)",
                                       myName, path, r, strerror(r) ); }
            }
            return (r);

        default:
            fprintf( stderr, "### %s: can't create \'%s\' (%d: %s)\n", myName, path, errno, strerror(errno) );
            return ( errno );
        }
    }
    else
    { // stat succeeded
        if ( S_ISDIR( sb.st_mode ) )
        {
            return (0); // found an existing path element, so begin to unwind the recursion 
        }
    }
    return (EFAULT);
}

void printHashes( void )
{
 static const char *exts[] = { "mp4", "m4v", "mkv", "mpeg", "mpg", "avi", "wmv", NULL };
    const char **ext = exts;

    printf("\n");
    while ( *ext != NULL )
    {
        printf( "#define kHash_%s\t%d\n", *ext, hashStr(*ext) );
        ext++;
    }

    ext = exts;
    while ( *ext != NULL )
    {
        printf( "\tcase kHash_%s:\n", *ext );
        ext++;
    }
}

char *makePath( const char *path, const char *file, const char *ext )
{
    char    *result;
    char    *p;
    size_t  len;

    len = 1; // for the trailing null
    if ( path != NULL ) { len += strlen(path); }
    if ( file != NULL ) { len += strlen(file); }
    if (  ext != NULL ) { len += strlen(ext);  }

    result = NULL;
    if (len > 0) { result = malloc( len ); }

    if (result != NULL)
    {
        p = stpcpy( result, path );
        if ( *(p - 1) == '/' )
            { *(--p) = '\0'; }
        p = stpcpy( p, file );
        if ( ext != NULL )
        {
            for ( int i = 8; i > 0; ++i )
            {
                --p;
                if ( *p == '.' )
                { 
                    p = stpcpy( p, ext );
                    break;
                }
            }
        }
    }

    return result;
}

char **processArgv( const char *inputFile, const char *outputFile )
{
    char        **result;
    char        *q;
    const char  *p, *r;
    size_t      len;

    // we expect to modify argv, so make a copy to modify
    result = malloc( (childArgc + 1) * sizeof(char *) );

    if ( result != NULL )
    {
        // for each argument
        for ( int i = 0; i < childArgc; ++i )
        {
            // first figure out the final length of the string, after substitutions
            p = childArgv[i];
            len = strlen(p);
            do {
                if ( *p == '%' )
                {
                    switch ( *(++p) )
                    {
                        case 'i': len += strlen(  inputFile ) - 2; break;
                        case 'o': len += strlen( outputFile ) - 2; break;
                    }
                }
            } while ( *p++ != '\0' );

            // allocate the new string
            q = malloc( len + 1 ); // don't forget the trailing null
            result[i] = q; // replace the original in our copy with the block we malloc'd

            // copy the original string, inserting parameters as we hit the tags
            if ( q != NULL )
            {
                p = childArgv[i];  // grab the original as the template

                do {
                    if ( *p != '%' ) { *q++ = *p; }
                    else
                    {
                        switch ( *(++p) )
                        {
                            case 'i': r = inputFile;  break;
                            case 'o': r = outputFile; break;
                            default:
                                r = NULL;
                                *q++ = '%';
                                break;
                        }

                        if ( r != NULL )
                        {
                            q = stpcpy( q, r ); // insert the parameter
                        }
                    }
                } while ( *p++ != '\0' );
            }

        } // for
        result[ childArgc ] = NULL;
    }
    
    return result;
}

void invoke( const char *inputFile, const char *outputFile  )
{
    int   pid, status;
    char  **argv;
    
    pid = fork();
    switch (pid)
    {
    case 0: // in child process
        chdir( workDir );

        argv = processArgv( inputFile, outputFile );

        if ( execvp( argv[0], argv ) == -1 )
        {
            fprintf( stderr, "### %s: failed to exec %s (%d: %s)\n", myName, argv[0], errno, strerror(errno) );
            exit( errno );
        }
        exit( -1 );

    case -1: // fork failed (still in parent)
        fprintf( stderr, "### %s: fork failed (%d: %s)\n", myName, errno, strerror(errno) );
        break;

    default: // parent process
        waitpid( pid, &status, 0 );
        
        if ( WIFEXITED(status) )
        {
            /*  WEXITSTATUS returns the exit status of the child. This consists of the least
                significant 8 bits of the status argument that the child specified in a call
                to exit(3) or _exit(2) or as the argument for a return statement in main().
                This macro should only be employed if WIFEXITED returned true. */
            if ( verbose )
                { printf( "child returned an exit status of %d.\n", WEXITSTATUS(status) ); }
        }
        
        /* returns true if the child process was terminated by a signal. */
        if ( WIFSIGNALED(status) )
        {
            /*  WTERMSIG returns the number of the signal that caused the child process to terminate.
                WCOREDUMP returns true if the child produced a core dump. It is not specified in
                POSIX.1-2001 and is not available on some UNIX implementations (e.g., AIX, SunOS).
                Only use this enclosed in #ifdef WCOREDUMP ... #endif. */
            /*  Both macros should only be employed if WIFSIGNALED returned true. */
            fprintf( stderr, "### %s: child terminated by signal %d%s.\n",
                             myName, WTERMSIG(status), WCOREDUMP(status) ? " (core dumped)" : "" );
        }
        
        /*  returns true if the child process was stopped by delivery of a signal;
            this is only possible if the call was done using WUNTRACED or when the child is being
            traced (see ptrace(2)). */
        if ( WIFSTOPPED(status) )
        {
            /*  returns the number of the signal which caused the child to stop.
                This macro should only be employed if WIFSTOPPED returned true. */
            fprintf( stderr, "### %s: child terminated by signal %d.\n", myName, WSTOPSIG(status) );
        }

        /* (since Linux 2.6.10) returns true if the child process was resumed by delivery of SIGCONT. */
        if ( WIFCONTINUED(status) )
        {
            fprintf( stderr, "### %s: child received SIGCONT\n", myName );
        }
        break;
    }
}

void processFile( const char *filename, const tStat *inStat )
{
    tStat   outStat;
    int     status;
    char    *inputFile  = makePath(  inputPath, filename, NULL );
    char    *outputFile = makePath( outputPath, filename, outputExt );
    char    *p;

    if ( stat( outputFile, &outStat ) != -1 )
    {
        status = ( inStat->st_mtime > outStat.st_mtime );
        if ( verbose )
            { printf( "%s is %s than %s\n", outputFile, status ? "older" : "newer", inputFile ); }
    }
    else switch (errno)
    {
    case ENOENT: // output path doesn't point at an existing file - but that's OK
        status = 2;
        if ( verbose )
            { printf( "%s doesn't exist\n", outputFile ); }

        p = strdup( outputFile );
        makeMissingDirs( dirname( p ) );
        free( p );
        break;

    default:
        status = 0;
        fprintf( stderr, "### %s: stat failed on %s (%d: %s)\n",
                         myName, outputFile, errno, strerror(errno) );
        break;
    }

    if ( status > 0 )
    {
        invoke( inputFile, outputFile );
    }

    free( outputFile );
    free(  inputFile );
}

int processEntry( const char *path, const struct stat *fileStat, const int typeflag )
{
    switch ( typeflag )
    {
    case FTW_F: // path is a regular file.
        if ( isVisible( path ) && isVideoFile( path ) )
        {
            processFile( &path[1], fileStat );
        }
        break;

    default: // everything else isn't useful
        break;
    }

    return (0);
}

int main( const int argc, const char *argv[] )
{
    char   *p;

    inputPath  = NULL;
    outputPath = NULL;

    if ( getcwd( workDir, sizeof(workDir) ) == NULL)
    {
        fprintf( stderr, "### %s: unable to get the current working directory (%d: %s)",
                         myName, errno, strerror(errno) );
        exit (-2);
    }
    
    p = strdup( argv[0] );
    myName = strdup( basename( p ) );
    free( p );

    if (myName == NULL )
        { myName = "<unknown>"; }
        
//    printf( "my name is \'%s\'\n", myName );

    childArgv = malloc( (argc + 1) * sizeof(char *) );

    if (childArgv == NULL)
    {
        exit (-1);
    }

    verbose = 0;

    childArgc = 0;
    for ( int i = 1; i < argc; ++i )
    {
        if ( argv[i][0] == '-' && argv[i][2] == '\0' )
        {
            switch (argv[i][1])
            {
            case 'i':
                if ( inputPath == NULL && i < argc - 1 )
                     { inputPath = argv[++i]; }
                else { childArgv[childArgc++] = argv[i]; }
                break;

            case 'o':
                if ( outputPath == NULL && i < argc - 1 )
                     { outputPath = argv[++i]; }
                else { childArgv[childArgc++] = argv[i]; }
                break;

            case 'e':
                if ( outputExt == NULL && i < argc - 1 )
                     { outputExt = argv[++i]; }
                else { childArgv[childArgc++] = argv[i]; }
                break;

            case 'v':
                if ( verbose == 0 ) { verbose = 1; }   
                else { childArgv[childArgc++] = argv[i]; }
                break;

            default:
                fprintf( stderr, "### %s: unknown option: %s", myName, argv[i] );
                break;
            }
        }
        else
        {
            // fprintf(stderr, "%d: \'%s\'\n", childArgc, argv[i] );
            childArgv[childArgc++] = argv[i];
        }
    }
    childArgv[childArgc] = NULL;

    if ( inputPath == NULL )
    {
        fprintf( stderr, "### %s: input path is required\n", myName );    
        exit( -1 );
    }
    else if ( outputPath == NULL )
    {
        fprintf( stderr, "### %s: output path is required\n", myName );
        exit( -2 );
    }
    else if ( childArgc <= 0 )
    {
        fprintf( stderr, "### %s: executable not provided\n", myName );
        exit( -3 );
    }
    else
    {
        chdir( inputPath );
        ftw( ".", processEntry, 10 );
    }

//	printHashes();

    return (0);
}
