/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2013  Dr. Jürgen Sauermann
    edif Copyright (C) 2020  Dr. C. H. L. Moller

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <malloc.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "config.h"	// this should pick up the apl src version

#include "Macro.hh"
#include "Quad_CR.hh"


#include<iostream>
#include<fstream>
#include<string>
#include<regex>


#include "Native_interface.hh"


#include "edif2.hh"
#include "gitversion.h"

#ifdef HAVE_CONFIG_H
#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_URL
#undef PACKAGE_VERSION
#undef VERSION
#include "../config.h"
#endif

#define EDIF_DEFAULT "vi"
static char *edif_default = NULL;

using namespace std;
static bool is_lambda = false;

static char *dir = NULL;
static const Function *apl_function = NULL;
static const UCS_string WHITESPACE = " \n\t\r\f\v";
static const string vname ("([A-Za-z∆⍙][A-Za-z0-9_¯∆⍙]*)");
static const string space ("[ \\t]+");
static const string optspace ("[ \\t]*");
static const string leftarrow ("←");
static const string assign ("("+vname+optspace+leftarrow+optspace+")?");
static const string locals  ("((;.*)*)");
static const regex niladic (assign+vname+optspace+locals);
static const regex monadic (assign+vname+space+vname+optspace+locals);
static const regex dyadic  (assign+vname+space+vname+space+
			    vname+optspace+locals);



class NativeFunction;

extern "C" void * get_function_mux(const char * function_name);

static bool
close_fun (Cause cause, const NativeFunction * caller)
{
  if (dir) {
    DIR *path;
    struct dirent *ent;
    if ((path = opendir (dir)) != NULL) {
      while ((ent = readdir (path)) != NULL) {
	char *lfn;
	asprintf (&lfn, "%s/%s", dir, ent->d_name);
	unlink (lfn);
	free (lfn);
	lfn = NULL;
      }
      closedir (path);
    } 

    rmdir (dir);
    free (dir);
    dir = NULL;
  }
  if (edif_default) {
    free (edif_default);
    edif_default = NULL;
  }
  return true;
}

Fun_signature
get_signature()
{
  asprintf (&dir, "/var/run/user/%d/%d",
	    (int)getuid (), (int)getpid ());
  mkdir (dir, 0700);
  char *ed = getenv ("EDIF");
  if (edif_default) {
    free (edif_default);
    edif_default = NULL;
  }
  edif_default = strdup (ed ?: EDIF_DEFAULT);

  return SIG_Z_A_F2_B;
}

// export EDIF="vi"

static bool
get_var (const char *fn, const char *base, Value_P B, Shape &shape,
	 bool &is_char)
{
  bool rc = false;
  NamedObject *obj = NULL;
  UCS_string symbol_name(*B.get());
  while (symbol_name.back() <= ' ') symbol_name.pop_back();
  if (symbol_name.size() != 0) {
    obj = (NamedObject *)Workspace::lookup_existing_name(symbol_name);
    if (obj) {
      Value_P val = obj->get_value();
      if (val->is_simple ()) {
	if (!val->is_empty ()) {
	  shape = val->get_shape ();
	  is_char = val->is_char_array ();
	  
#if 0
	  uRank rank = val->get_rank ();
	  cerr << "rhorho  = " << rank << endl;

	  cerr << "rho  = ";
	  loop (r, rank) cerr << val->get_shape_item (r) << " ";
	  cerr << endl;
	  cerr << "count = " << val->element_count () << endl;

	  if (val->is_scalar ())	cerr << "scalar\n";
	  if (val->is_simple_scalar ())	cerr << "simple scalar\n";
	  if (val->is_numeric_scalar ()) cerr << "numeric scalar\n";
	  if (val->is_character_scalar ()) cerr << "character scalar\n";
	  if (val->is_char_array ())	cerr << "character array\n";
	  if (val->is_char_string ())	cerr << "character string\n";
	  if (val->is_char_vector ())	cerr << "character vector\n";
	  if (val->is_char_scalar ())	cerr << "character scalar\n";
	  if (val->is_int_scalar ())	cerr << "int scalar\n";

	  /****

	  a←'abcdefghi'
	  rhorho  = 1
	  rho  = 9 
	  count = 9
	  character array
	  character string
	  character vector

	  b←3 3⍴'abcdefghi'
	  rhorho  = 2
	  rho  = 3 3 
	  count = 9
	  character array

	  v←⍳8
	  rhorho  = 1
	  rho  = 8 
	  count = 8

	  w←2 3 4⍴⍳24
	  rhorho  = 3
	  rho  = 2 3 4 
	  count = 24

	  c←8
	  rhorho  = 0
	  rho  = 
	  count = 1
	  scalar
	  simple scalar
	  numeric scalar
	  int scalar

	  d←6.7
	  rhorho  = 0
	  rho  = 
	  count = 1
	  scalar
	  simple scalar
	  numeric scalar

	  f←v÷3
	  rhorho  = 1
	  rho  = 8 
	  count = 8

	  e='e'
	  rhorho  = 0
	  rho  = 
	  count = 1
	  scalar
	  simple scalar
	  character scalar
	  character array
	  character string
	  character scalar


	  ****/

	  
#endif
	  PrintContext pctx = Workspace::get_PrintContext(PST_NONE);
	  //pctx.set_style(PR_APL);
	  PrintBuffer pb(*val, pctx, 0);
    
	  ofstream tfile;
	  tfile.open (fn, ios::out);
	  loop (l, pb.get_height ()) {
	    UCS_string line = pb.get_line (l);
	    /***
	      pad char = APL character:  ⎕ (U+EEFB)
	    ***/
	    line.map_pad ();
	    UTF8_string utf (line);
	    tfile << utf << endl;
	  }
	  tfile.close ();
	  rc = true;
	}
	else obj = NULL;
      }
      else cerr << "Nested variables are not supported\n";
    }
    if (!obj) {
      ofstream tfile;
      tfile.open (fn, ios::out);
      if (is_lambda)
	tfile << base << "←";
      else
	tfile << base << endl;
      tfile.close ();
      rc = true;
    }
  }
  return rc;
}


/***

    ⎕at

    1 ⎕at '⎕cr'			1 2 0
    1 ⎕at 'vi'			1 2 0
    1 ⎕at 'll'			1 1 0
    1 ⎕at 'mm'			0 1 0

    2 ⎕at '⎕cr'			1970 1 1 0 0 0 0
    2 ⎕at 'vi'			1970 1 1 0 0 0 0
    2 ⎕at 'll'			2020 8 14 18 16 38 476
    2 ⎕at 'mm'			2020 8 14 18 17 47 153    

    3 ⎕at '⎕cr'			1 1 1 0
    3 ⎕at 'vi'			1 1 1 0
    3 ⎕at 'll'			0 0 0 0
    3 ⎕at 'mm'			0 0 0 0

    4 ⎕at '⎕cr'			0 0
    4 ⎕at 'vi'			0 0
    4 ⎕at 'll'			0 0
    4 ⎕at 'mm'			0 0

***/
static void
get_fcn (const char *fn, const char *ifn, const char *base,
	 Value_P B, string locals)
{
  UCS_string symbol_name(*B.get());
  while (symbol_name.back() <= ' ')   symbol_name.pop_back();
  if (symbol_name.size() != 0) {
    if (symbol_name[0] == UNI_MUE) {   // macro
      loop (m, Macro::MAC_COUNT) {
	const Macro * macro =
	  Macro::get_macro(static_cast<Macro::Macro_num>(m));
	if (symbol_name == macro->get_name()) {
	  apl_function = macro;
	  break;
	}
      }
    }
    else {  // maybe user defined function
      NamedObject * obj
	= (NamedObject *)Workspace::lookup_existing_name(symbol_name);
      if (obj && obj->is_user_defined()) {
	apl_function = obj->get_function();
	if (apl_function && apl_function->get_exec_properties()[0]) 
	  apl_function = 0;
      }
    }
  }
  if (apl_function != 0) {
    is_lambda |= apl_function->is_lambda();
    const UCS_string ucs = apl_function->canonical(false);
    UCS_string_vector tlines;
    ucs.to_vector(tlines);
    ofstream tfile;
    
    tfile.open (fn, ios::out);
    char *semiloc = NULL;
    loop(row, tlines.size()) {
      const UCS_string & line = tlines[row];
      UTF8_string utf (line);
      if (is_lambda) {
	if (row == 0) {
	  const char *fuck = utf.c_str ();
	  if (semiloc) {
	    free (semiloc);
	    semiloc = NULL;
	  }
	  char *sl = index ((char *)fuck, ';');
	  if (sl) semiloc = strdup (sl);
	}
	else {
	  utf = UCS_string (utf, 2, string::npos);	// skip assignment
	  tfile << ifn << "←{"
		<< utf
		<< (semiloc ?: "")
		<< "}\n";
	  break;
	}
      }
      else tfile << utf << endl;
    }
    tfile.close ();
  }
  else {
    ofstream tfile;
    tfile.open (fn, ios::out);
    if (is_lambda)
      tfile << ifn << "←{ " << locals << "}";
    else
      tfile << base << endl;
    tfile.close ();
  }
}

static void
cleanup (char *dir, UTF8_string base_name, char *fn)
{
  DIR *path;
  struct dirent *ent;
  if ((path = opendir (dir)) != NULL) {
    while ((ent = readdir (path)) != NULL) {
      if (!strncmp (ent->d_name, base_name.c_str (),
		    base_name.size ())) {
	char *lfn;
	asprintf (&lfn, "%s/%s", dir, ent->d_name);
	unlink (lfn);
	free (lfn);
	lfn = NULL;
      }
    }
    closedir (path);
  } 
  if (fn) {
    free (fn);
    fn = NULL;
  }
}

static string
parse_header (UTF8_string base_name, string &locals)
{
  cmatch m;
  string fcn ;
  if (regex_match (base_name.c_str (), m, niladic)) {
    size_t len = m[3].second - m[3].first;
    string tgt (m[3].first);
    fcn = tgt.substr (0, len);
    len = m[4].second - m[4].first;
    string ltgt (m[4].first);
    locals = ltgt.substr (0, len);
  }
  else if (regex_match (base_name.c_str (), m, monadic)) {
    size_t len = m[3].second - m[3].first;
    string tgt (m[3].first);
    fcn = tgt.substr (0, len);
  }
  else if (regex_match (base_name.c_str (), m, dyadic)) {
    size_t len = m[4].second - m[4].first;
    string tgt (m[4].first);
    fcn = tgt.substr (0, len);
  }
  return fcn;
}


static Token
eval_EB (const char *edif, Value_P B, APL_Integer idx)
{
  is_lambda = false;
  switch(idx) {
  case 1: is_lambda = true; break;
  case 2: 
    {
      Value_P vers (UCS_string(PACKAGE_STRING), LOC);
      return Token(TOK_APL_VALUE1, vers);
    }
    break;
  case 3: 
    {
      Value_P vers (UCS_string(GIT_VERSION), LOC);
      return Token(TOK_APL_VALUE1, vers);
    }
    break;
  }

  apl_function = NULL;
  if (B->is_char_string ()) {
    const UCS_string  ustr = B->get_UCS_ravel();
    UTF8_string base_name(ustr);
    string locals;
    string parsed_fcn_name = parse_header (base_name, locals);

    char *fn = NULL;
#if 1
    char *ifn = (char *)parsed_fcn_name.c_str ();
    APL_Integer nc = Quad_NC::get_NC(UCS_string (ifn));
    asprintf (&fn, "%s/%s.apl", dir, ifn);
#else
    asprintf (&fn, "%s/%s.apl", dir, base_name.c_str ());
    APL_Integer nc = Quad_NC::get_NC(ustr);
#endif

    switch (nc) {
    case NC_FUNCTION:
    case NC_OPERATOR:
    case NC_UNUSED_USER_NAME:
      {
	get_fcn (fn, ifn, base_name.c_str (), B, locals);
	char *buf;
	asprintf (&buf, "%s %s", edif, fn);
	system (buf);
	if (buf) {
	  free (buf);
	  buf = NULL;
	}

	ifstream tfile;
	tfile.open (fn, ios::in);
	UCS_string ucs;
	UCS_string lambda_ucs;
	if (tfile.is_open ()) {
	  string line;
	  int cnt = 0;
	  while (getline (tfile, line)) {
	    ucs.append_UTF8 (line.c_str ());
	    ucs.append(UNI_LF);
	    if (cnt++ == 0) lambda_ucs.append_UTF8 (line.c_str ());
	  }
	  tfile.close ();
	  if (is_lambda) {
	    if (lambda_ucs.has_black ()) {
	      int len = 0;
	      size_t strt = lambda_ucs.find_first_not_of(WHITESPACE);
	      UCS_string cpy = UCS_string (lambda_ucs, strt, string::npos);
	      if (lambda_ucs.back () != L'←') {
		lambda_ucs = cpy;
		basic_string<Unicode> lambda_basic (lambda_ucs);
		int cmp = lambda_basic.compare (0, ustr.size (), ustr);
		UCS_string target_name;
		if (cmp != 0) {
		  UCS_string larrow (UTF8_string ("←"));
		  size_t arrow_offset = lambda_ucs.find_first_of (larrow);
		  target_name =
		    (arrow_offset == string::npos) ?
		    lambda_ucs : UCS_string (lambda_ucs, arrow_offset,
					     string::npos);
		}
		else target_name = ustr;

		NamedObject * obj =
		  (NamedObject *)Workspace::lookup_existing_name (target_name);
		if (obj) {
		  UCS_string erase_cmd(")ERASE ");
		  erase_cmd.append (target_name);
		  Bif_F1_EXECUTE::execute_command(erase_cmd);
		}
		Command::do_APL_expression (lambda_ucs);
	      }
	    }
	  }
	  else {
	    if (ucs.has_black ()) {
	      int error_line = 0;
	      UCS_string creator (base_name);
	      UTF8_string creator_utf8(creator);
	      UserFunction::fix (ucs,		// text
				 error_line,	// err_line
				 false,		// keep_existing
				 LOC,		// loc
				 creator_utf8,	// creator
				 true);		// tolerant
	    }
	  }
	}
	else {
	  UCS_string ucs ("Error opening working file.");
	  Value_P Z (ucs, LOC);
	  Z->check_value (LOC);
	  is_lambda = false;
	  return Token (TOK_APL_VALUE1, Z);
	}
	cleanup (dir, base_name, fn);
      }
      break;
    case NC_VARIABLE:
      {
	Shape shape;
	bool is_char;
	get_var (fn, base_name.c_str (), B, shape, is_char);

	char *buf;
	asprintf (&buf, "%s %s", edif, fn);
	system (buf);
	if (buf) {
	  free (buf);
	  buf = NULL;
	}
	
	ifstream tfile;
	tfile.open (fn, ios::in);
	if (tfile.is_open ()) {
	  UCS_string ucs (ustr);
	  ucs.append (UTF8_string ("←"));
	  uRank rank = shape.get_rank ();
	  if (rank > 0) {
	    loop (r, rank) {
	      ostringstream cval;
	      cval << shape.get_shape_item (r);
	      UTF8_string uuu (cval.str ().c_str ());
	      ucs.append (uuu);
	      ucs.append(UNI_SPACE);
	    }
	    ucs.append (UTF8_string ("⍴"));
	  }
	  if (is_char)ucs.append (UTF8_string ("'"));
	  string line;
	  while (getline (tfile, line)) {
	    ucs.append_UTF8 (line.c_str ());
	    ucs.append(UNI_SPACE);
	  }
	  if (is_char)ucs.append (UTF8_string ("'"));
	  tfile.close ();
	  Command::do_APL_expression (ucs);
	}
	else {
	  UCS_string ucs ("Error opening working file.");
	  Value_P Z (ucs, LOC);
	  Z->check_value (LOC);
	  return Token (TOK_APL_VALUE1, Z);
	}
	cleanup (dir, base_name, fn);
      }
      break;
    default:
      {
	UCS_string ucs ("Unknown editing type requested.");
	Value_P Z (ucs, LOC);
	Z->check_value (LOC);
	is_lambda = false;
	return Token (TOK_APL_VALUE1, Z);
      }
      break;
    }

    is_lambda = false;
    return Token(TOK_APL_VALUE1, Str0_0 (LOC));	// in case nothing works
  }
  else {
    UCS_string ucs ("Character string argument required.");
    Value_P Z (ucs, LOC);
    Z->check_value (LOC);
    return Token (TOK_APL_VALUE1, Z);
  }
}


static Token
eval_XB(Value_P X, Value_P B, const NativeFunction * caller)
{
  APL_Integer val =
    (X->is_numeric_scalar()) ? X->get_sole_integer() : 0;
  return eval_EB (edif_default, B, val);
}

static Token
eval_B (Value_P B, const NativeFunction * caller)
{
  Value_P X = IntScalar (0, LOC);
  return eval_XB (X, B, caller);
}


static Token
eval_AXB(Value_P A, Value_P X, Value_P B, const NativeFunction * caller)
{
  
  if (A->is_char_string ()) {
    APL_Integer val =
      (X->is_numeric_scalar())  ? X->get_sole_integer()  : 0;
    
    const UCS_string  ustr = A->get_UCS_ravel();
    UTF8_string edif (ustr);
    if (edif.c_str () && *(edif.c_str ())) {
      if (edif_default) {
	free (edif_default);
	edif_default = NULL;
      }
      edif_default = strdup (edif.c_str ());
      return eval_EB (edif_default, B, val);
    }
    else {
      UCS_string ucs ("Invalid editor specification.");
      Value_P Z (ucs, LOC);
      Z->check_value (LOC);
      return Token (TOK_APL_VALUE1, Z);
    }
  }
  else {
    UCS_string ucs ("The editor specification must be a string.");
    Value_P Z (ucs, LOC);
    Z->check_value (LOC);
    return Token (TOK_APL_VALUE1, Z);
  }
}

static Token
eval_AB (Value_P A, Value_P B, const NativeFunction * caller)
{
  Value_P X = IntScalar (0, LOC);
  return eval_AXB (A, X, B, caller);
}

void *
get_function_mux (const char * function_name)
{
   if (!strcmp (function_name, "get_signature")) return (void *)&get_signature;
   if (!strcmp (function_name, "eval_B"))        return (void *)&eval_B;
   if (!strcmp (function_name, "eval_XB"))       return (void *)&eval_XB;
   if (!strcmp (function_name, "eval_AB"))       return (void *)&eval_AB;
   if (!strcmp (function_name, "eval_AXB"))      return (void *)&eval_AXB;
   if (!strcmp (function_name, "close_fun"))
     return reinterpret_cast<void *>(&close_fun);
   return 0;
}
