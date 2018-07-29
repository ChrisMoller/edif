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
will appear in the APL workspace.

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

   export EDIF="emacs --geometry=40x20  -background '#ffffcc' -font 'DejaVu Sans Mono-10'"

will invoke emacs with a fairly small window, a light yellow background, and
using the DejaVu Sans Mono-10 font.  (That's also the default if no EDIF
variable is found.)

There's also a dyadic form:

	'editor' edif 'function_name'

that lets you specify the editor you want, for example:

	'vi' edif 'fubar'

will open function fubar in vi.  Any command-line arguments you provide will
also be passed to the editor, so:

	'gvim -bg red' edif 'fubar'

will do as you expect it to and give you eye strain.

The dyadic form is a one-shot thing--edif doesn't remember editors
specified this way and the monadic form will go back to using the
default or environment-specified editor.

edif has been tested only with emacs, vi, and gvim.


Future work may also allow edif to edit APL variables and operators, but no
guarantees I'll ever get around to it.

edif may be included in the workspace with:

	'libedif.so' ⎕fx 'edif'



Implimentation note:

edif works by storing an editable version of the specified function in:

/var/run/user/<uid>/<pid>/<name>.apl  

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

So far as I can tell, edif doesn't interfere with Elias Mårtenson's 
emacs APL mode, but I haven't thoroughly tested that.

