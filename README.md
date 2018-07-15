edif is a GNU APL native function that allows the use of external editors
from within an APL session.

Usage:

	edif 'function_name'

This will open an editor, typically vi or emacs, containing the present
definition of the specified function, or, if the function doesn't exist,
a boilerplate function header consisting of the function name.  After saving
the edited definition and exiting the editor, the function will appear in
the APL workspace.  While the editor is open, APL is suspended.

edif will look for the environment variable EDIF and will use the string
specified by that variable as the command line to invoke the chosen editor.
For example:

   export EDIF="emacs --geometry=40x20  -background '#ffffcc' -font 'DejaVu Sans Mono-10'"

will invoke emacs with a fairly small window, a light yellow background, and
using the DejaVu Sans Mono-10 font.  (That's also the default if no EDIF
variable is found.)

edif has only been tested with emacs and vi.


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

