# file-utils

Some simple file manipulation utilities I've written to scratch an itch or two.

## if-newer

<pre>if-newer -i &lt;input path&gt; -o &lt;output path&gt; -e m4v &lt;command&gt; &lt;arg1&gt;...&lt;argN&gt;
    -i  input path
    -o  output path
    -e  output extension
    -v  be verbose

  &lt;command&gt;         the executable to run for each input file that's newer, to regenerate the output file
  &lt;arg1&gt;...&lt;argN&gt;   arguments to pass to the command
                     %i will be replaced by the input path, and
                     %o will be replaced by the output path.
</pre>

## match-ext




## nzb-cleanup


