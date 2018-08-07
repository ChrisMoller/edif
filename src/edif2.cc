/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright (C) 2008-2013  Dr. Jürgen Sauermann

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

#define USE_KIDS

#include <error.h>
#include <fcntl.h>
#include <limits.h>
#include <malloc.h>
#include <mqueue.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>


#include<iostream>
#include<fstream>
#include<string>

#include "Macro.hh"

#include "edif2.hh"

#define APL_SUFFIX ".apl"
#define MQ_NAME "/WHATS_IT_TO_YOU"
#define MQ_SIGNAL (SIGRTMAX - 2)

using namespace std;

static pid_t watch_pid = -1;
#ifdef USE_KIDS
static pid_t *kids = NULL;
static int kids_nxt = 0;
static int kids_max = 0;
#define KIDS_INCR 8
#else
static pid_t group_pid = 0;
#endif
static char *dir = NULL;
static mqd_t mqd = -1;

#define EDIF2_DEFAULT \
  "emacs --geometry=60x20 -background '#ffffcc' -font 'DejaVu Sans Mono-10'"
char *edif2_default = NULL;

#ifdef USE_KIDS
static void
add_a_kid (pid_t kid)
{
  if (kids_max <= kids_nxt) {
    kids_max += KIDS_INCR;
    kids = (pid_t *)realloc (kids, kids_max * sizeof(pid_t));
  }
  kids[kids_nxt++] = kid;
}
#endif

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
      }
      closedir (path);
    } 

    rmdir (dir);
    free (dir);
    dir = NULL;
  }

  if (mqd != -1) mq_close (mqd);
  mq_unlink (MQ_NAME);


#ifdef USE_KIDS
  int i;
  for (i = 0; i < kids_nxt; i++) {
    if (kids[i] > 0) kill (kids[i], SIGTERM);
  }
#else
  if (group_pid > 0) {
    killpg (group_pid, SIGTERM);
    group_pid = 0;
  }
#endif

  if (watch_pid > 0) {
    kill (watch_pid, SIGTERM);
    watch_pid = 0;
  }
  
  if (edif2_default) free (edif2_default);
  return false;
}

/***
    base_name == apl function name
    fn = fully qualified file name
***/

static void
read_file (const char *base_name, const char *fn)
{
  ifstream tfile;
  tfile.open (fn, ios::in);
  UCS_string ucs;
  if (tfile.is_open ()) {
    string line;
    while (getline (tfile, line)) {
      ucs.append_utf8 (line.c_str ());
      ucs.append(UNI_ASCII_LF);
    }
    tfile.close ();
    int error_line = 0;
    UCS_string creator (base_name);
    creator.append (UNI_ASCII_COLON);
    creator.append_number (0);
    UTF8_string creator_utf8(creator);
    UserFunction::fix (ucs,		// text
		       error_line,	// err_line
		       false,		// keep_existing
		       LOC,		// loc
		       creator_utf8,	// creator
		       true);		// tolerant
  }
}


static void
enable_mq_notify ()
{
  union sigval sv;
  sv.sival_int = 0;
  struct sigevent sevp;
  sevp.sigev_notify = SIGEV_SIGNAL;
  sevp.sigev_value  = sv;
  sevp.sigev_signo  = MQ_SIGNAL;
  int rc = mq_notify (mqd, &sevp);
  //  if (rc == -1) perror ("mq_notify");  fixne
}

static void
handle_msg ()
{
  char bfr[NAME_MAX + 8];
  struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000};
  ssize_t sz;
  while (0 <= (sz = mq_timedreceive(mqd, bfr, NAME_MAX + 8, NULL, &ts))) {
    char *cpy = strdup (bfr);
    if (cpy) {
      char *suffix = &cpy[strlen (bfr) - strlen (APL_SUFFIX)];
      if (!strcmp (suffix, APL_SUFFIX)) {
	*suffix = 0;
	char *fn = NULL;
	asprintf (&fn, "%s/%s", dir, bfr);
	if (fn) {
	  read_file (cpy, fn);
	  free (fn);
	}
      }
      free (cpy);
    }
  }
  
  enable_mq_notify ();
}


static void
watch_chld_handler(int sig, siginfo_t *si, void *data)
{
  int wstatus;
  waitpid (si->si_pid, &wstatus, WNOHANG);
}


static void
edit_chld_handler(int sig, siginfo_t *si, void *data)
{
  int wstatus;
  waitpid (si->si_pid, &wstatus, 0 /* WNOHANG */);
#ifdef USE_KIDS
  int i;
  for (i = 0; i < kids_nxt; i++) {
    if (kids[i] == si->si_pid) kids[i] = -1;
  }
#endif
}

static void
msg_handler(int sig, siginfo_t *si, void *data)
{
  int wstatus;
  waitpid (si->si_pid, &wstatus, WNOHANG);
  handle_msg ();
}

Fun_signature
get_signature()
{
  asprintf (&dir, "/var/run/user/%d/%d",
	    (int)getuid (), (int)getpid ());
  mkdir (dir, 0700);
  char *ed2 = getenv ("EDIF2");
  edif2_default = strdup (ed2 ?: EDIF2_DEFAULT);

  struct sigaction msg_act;
  msg_act.sa_sigaction = msg_handler;
  sigemptyset (&msg_act.sa_mask);
  msg_act.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction (MQ_SIGNAL, &msg_act, NULL);

  struct sigaction chld_act;
  chld_act.sa_sigaction = watch_chld_handler;
  sigemptyset (&chld_act.sa_mask);
  chld_act.sa_flags = SA_SIGINFO | SA_RESTART;
  sigaction (SIGCHLD, &chld_act, NULL);

  struct mq_attr attr;
  attr.mq_maxmsg  = 8;	// no good reason
  attr.mq_msgsize = NAME_MAX + 1;
  mqd = mq_open (MQ_NAME, O_RDWR | O_CREAT | O_NONBLOCK,
		 0600, &attr);
  if (mqd == -1) {
    perror ("internal mq_open error in edif2");
    return SIG_NONE;
  }

  char bfr[NAME_MAX + 8];
  struct timespec ts = {.tv_sec = 0, .tv_nsec = 100000000};
  ssize_t sz;
  // empty the queue, just in case
  while (0 < (sz = mq_timedreceive(mqd, bfr, NAME_MAX + 8, NULL, &ts))) {}
  enable_mq_notify ();

#ifndef USE_KIDS
  group_pid = getpid ();
#endif
 
  pid_t pid = fork ();
  if (pid > 0) watch_pid = pid;
  else if (pid == 0) {		// child -- watch for file changes
    
    int inotify_fd = inotify_init ();
    FILE *inotify_fp = fdopen(inotify_fd, "r");
    int inotify_rc = inotify_add_watch (inotify_fd, dir,
					IN_CREATE | IN_MODIFY);
    // int inotify_rc = inotify_add_watch (inotify_fd, dir, IN_ALL_EVENTS);
    while (1) {
#define BUF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))
      char buf[BUF_LEN] __attribute__ ((aligned(8)));
      ssize_t sz = read (inotify_fd, buf, BUF_LEN);
      if (sz < 0) clearerr (inotify_fp);
      else {
	struct inotify_event *event = (struct inotify_event *)buf;
	if (event->len > 0 && strlen (event->name) > 0) {
	  int mrc = mq_send (mqd, event->name, event->len, 0);
	  if (mrc == -1) {
	    perror ("internal mq_send error in edif2");
	    return SIG_NONE;
	  }
	}
      }
    }
  }

  return SIG_Z_A_F2_B;
}

// export EDIF2="emacs --geometry=80x60 -background '#ffffcc' -font 'DejaVu Sans Mono-10'"

static void
get_fcn (const char *fn, const char *base, Value_P B)
{
  UCS_string symbol_name(*B.get());
  while (symbol_name.back() <= ' ')   symbol_name.pop_back();
  const Function * function = 0;
  if (symbol_name.size() != 0) {
    if (symbol_name[0] == UNI_MUE) {   // macro
      loop (m, Macro::MAC_COUNT) {
	const Macro * macro =
	  Macro::get_macro(static_cast<Macro::Macro_num>(m));
	if (symbol_name == macro->get_name()) {
	  function = macro;
	  break;
	}
      }
    }
    else {  // maybe user defined function
      NamedObject * obj = Workspace::lookup_existing_name(symbol_name);
      if (obj && obj->is_user_defined()) {
	function = obj->get_function();
	if (function && function->get_exec_properties()[0]) function = 0;
      }
    }
  }
  if (function != 0) {
    const UCS_string ucs = function->canonical(false);
    UCS_string_vector tlines;
    ucs.to_vector(tlines);
    ofstream tfile;
    tfile.open (fn, ios::out);
    loop(row, tlines.size()) {
      const UCS_string & line = tlines[row];
      UTF8_string utf (line);
      tfile << utf << endl;
    }
    tfile.flush ();
    tfile.close ();
  }
  else {
    ofstream tfile;
    tfile.open (fn, ios::out);
    tfile << base << endl;
    tfile.flush ();
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
      }
    }
    closedir (path);
  } 
  if (fn) free (fn);
}

static Token
eval_EB (const char *edif, Value_P B)
{
  if (B->is_char_string ()) {
    const UCS_string  ustr = B->get_UCS_ravel();
    UTF8_string base_name(ustr);
    char *fn = NULL;
    asprintf (&fn, "%s/%s%s", dir, base_name.c_str (), APL_SUFFIX);

    APL_Integer nc = Quad_NC::get_NC(ustr);
    switch (nc) {
    case NC_FUNCTION:
    case NC_UNUSED_USER_NAME:
      {
	if (watch_pid < 0) {
	  UCS_string ucs ("Internal failure.");
	  Value_P Z (ucs, LOC);
	  Z->check_value (LOC);
	  return Token (TOK_APL_VALUE1, Z);
	}
	else {
	  pid_t pid = fork ();
	  if (pid < 0) {
	    UCS_string ucs ("Editor process failed to fork.");
	    Value_P Z (ucs, LOC);
	    Z->check_value (LOC);
	    return Token (TOK_APL_VALUE1, Z);
	  }
	  else if (pid > 0) {		// parent
#ifdef USE_KIDS
	    add_a_kid (pid);
#else
	    int rc = setpgid (pid, group_pid);
	    if (rc == -1) {
	      UCS_string ucs ("Internal failure in edif2.");
	      Value_P Z (ucs, LOC);
	      Z->check_value (LOC);
	      return Token (TOK_APL_VALUE1, Z);
	    }
	    else if (group_pid == 0) group_pid = getpgid (pid);
#endif
	    struct sigaction chld_act;
	    chld_act.sa_sigaction = edit_chld_handler;
	    sigemptyset (&chld_act.sa_mask);
	    chld_act.sa_flags = SA_SIGINFO | SA_RESTART;
	    sigaction (SIGCHLD, &chld_act, NULL);
	  }
	  else if (pid == 0) {		// child
	    get_fcn (fn, base_name.c_str (), B);
	    char *buf;
	    asprintf (&buf, "%s %s", edif, fn);
	    int erc = execl("/bin/sh", "sh", "-c", buf, (char *) 0);
	    if (erc == -1) {
	      if (buf) free (buf);
	      UCS_string ucs ("Editor process failed to execute.");
	      Value_P Z (ucs, LOC);
	      Z->check_value (LOC);
	      return Token (TOK_APL_VALUE1, Z);
	    }
	  }
	  cleanup (dir, base_name, fn);
	}
      }
      break;
    case NC_VARIABLE:
      {
	UCS_string ucs ("Variable editing not yet implemented.");
	Value_P Z (ucs, LOC);
	Z->check_value (LOC);
	return Token (TOK_APL_VALUE1, Z);
      }
      break;
    default:
      {
	UCS_string ucs ("Unknown editing type requested.");
	Value_P Z (ucs, LOC);
	Z->check_value (LOC);
	return Token (TOK_APL_VALUE1, Z);
      }
      break;
    }

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
eval_B (Value_P B)
{
  return eval_EB (edif2_default, B);
}

static Token
eval_AB (Value_P A, Value_P B)
{
  if (A->is_char_string ()) {
    const UCS_string  ustr = A->get_UCS_ravel();
    UTF8_string edif (ustr);
    if (edif.c_str () && *(edif.c_str ())) {
      if (edif2_default) free (edif2_default);
      edif2_default = strdup (edif.c_str ());
      return eval_EB (edif2_default, B);
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

#if 0
static Token
eval_XB (Value_P A, Value_P B, const NativeFunction * caller)
{
  cerr << "in eval_XB()\n";
  return Token (TOK_APL_VALUE1, Str0_0 (LOC));
}

static Token
eval_AXB (Value_P A, Value_P X, Value_P B)
{
  cerr << "in eval_AXB()\n";
  return Token (TOK_APL_VALUE1, Str0_0 (LOC));
}
#endif
  
void *
get_function_mux (const char * function_name)
{
   if (!strcmp (function_name, "get_signature")) return (void *)&get_signature;
   if (!strcmp (function_name, "eval_B"))        return (void *)&eval_B;
   if (!strcmp (function_name, "eval_AB"))       return (void *)&eval_AB;
#if 0
   if (!strcmp (function_name, "eval_XB"))       return (void *)&eval_XB;
   if (!strcmp (function_name, "eval_AXB"))      return (void *)&eval_AXB;
#endif
   if (!strcmp (function_name, "eval_ident_Bx")) return (void *)&eval_ident_Bx;
   if (!strcmp (function_name, "eval_fill_B"))   return (void *)&eval_fill_B;
   if (!strcmp (function_name, "eval_fill_AB"))  return (void *)&eval_fill_AB;
   if (!strcmp (function_name, "close_fun"))
     return reinterpret_cast<void *>(&close_fun);
   return 0;
}
