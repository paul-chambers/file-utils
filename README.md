# file-utils

Some simple file manipulation utilities I've written to scratch an itch or two.

## if-newer

if-newer -i <input path> -o <output path> -e m4v <command> <arg1>...<argN>
    -i  input path
    -o  output path
    -e  output extension
    -v  be verbose

  <command>         the executable to be run for each file that's newer
  <arg1>...<argN>   arguments to pass to the command
                     %i will be replaced by the input path, and
                     %o will be replaced by the output path.

## match-ext




## nzb-cleanup


