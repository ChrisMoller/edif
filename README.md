# edif
A GNU APL extension that allows the use of external editors.

edif adds a function to GNU APL that allows the use to use external
editors from within an APL session.

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


