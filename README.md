edif is a GNU APL native function that allows the use of external editors
from within an APL session.

Usage:

	edif 'function_name'
    or
	edif2 'function_name'

Both of these will open an editor, typically vi or emacs, containing the
present definition of the specified function, or, if the function doesn't
exist, a boilerplate function header consisting of the function name.
After saving the edited definition and exiting the editor, the function
will appear in the APL workspace.  (In the case of a new function, the
boilerplate name can be edited as necessary:

    fubar

can be edited to

    z←x fubar y

and the right thing will be fixed in APL.  (Even existing functions can be
modified this way.))

The two versions of the function differ in that the first version, edif,
suspends the current APL session until the editor is closed, while the
second version, edif2, opens the editor in a separate process, in a
separate window, while the APL session remains active.  This allows
functions to be edited and tested concurrently without having to go
into and out of the editor.  edif2 also allows multiple editor windows
to be open simultaneously on different functions.

Both versions have a dyadic form:

	'editor' edif 'function_name'
    or
	'editor' edif2 'function_name'

where 'editor' is the editor to be invoked.  For example:

	'vi' edif 'fubar'

would open function fubar using the vi editor.

	'emacs' edif2 'fu'
	'emacs' edif2 'bar'

would open functions fu and bar in separate emacs windows.

edif will look for the environment variable EDIF and will use the string
specified by that variable as the command line to invoke the chosen editor.
For example:

   export EDIF="nano"

will cause edif to default to using the nano editor.  Similarly, the EDIF2
variable affects the default edif2 editor.

edif has been tested with emacs, vi, and nano; edif2 with emacs and gvim.
The default editor for edif is vi; the default for edif2 is

   "emacs --geometry=40x20  -background '#ffffcc' -font 'DejaVu Sans Mono-10'"

The dyadic forms remember the chosen editors and sets them as replacement
defaults for the duration of the session.

edif may be included in the workspace with:

	'libedif.so' ⎕fx 'edif'
	
and edif2 with:

	'libedif2.so' ⎕fx 'edif2'

Of course, you can use any function names you like and, as long as you use
different names, both versions can be used at the same time.

edif and edif2 can both be invoked with an option in what's usually used as the axis
specifier.  An option of [1] forces edif into lambda mode:

	    edif2 [1] 'fu'

would open a boilerplate lambda function consisting of just the assignment ("fu←") and
allowing the user to enter the body.  (E,g., "{⍺ + ⍵}")  (It's the user's responsibility
to ensure that the body meets the requirements of a lambda.)



So far as I can tell, edif doesn't interfere with Elias Mårtenson's 
emacs APL mode, but I haven't thoroughly tested that.

By the way, "edif" is short for "editor interface."



Implimentation note:

edif and edif2 work by storing an editable version of the specified
function in:

/var/run/user/&lt;uid&gt;/&lt;pid&gt;/&lt;name&gt;.apl  

where <uid> is the user's userid, <pid> is the process id of the APL 
session, and <name> is the function name.  This allows multiple users 
each to have multiple simultaneous APL sessions with workspaces with 
identical names.  No locking is done by edif and I've no idea if APL 
itself has any protection against a writable workspace being open in 
multiple simultaneous sessions, but it opens up the possibility that 
you can hose the workspace.  So while, as far as edif is concerned 
you can have multiple simultaneous sessions aimed at the same lib0 
workspace, you probably shouldn't do it.

Also, I've no idea if Windows or any Linux distribution other than 
Fedora has a /var directory, so using this directory may be non-portable.

