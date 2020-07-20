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
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>


#include<iostream>
#include<fstream>
#include<string>

#include "Macro.hh"
#include "Command.hh"

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


//#define DO_DEBUG
//#define DO_DEBUG2

#if defined (DO_DEBUG) || defined (DO_DEBUG2)
ofstream logfile ("/tmp/edif2.log");
#endif

static pthread_mutex_t *mutex;
static pthread_mutexattr_t mutexattr;
//static char *shared_block;

#define APL_SUFFIX ".apl"
#define LAMBDA_PREFIX "_lambda_"
#define MQ_SIGNAL (SIGRTMAX - 2)
static char *mq_name = NULL;

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

/***
    alloced in get_signature
    freed in close_fun
***/
static char *dir = NULL;

static mqd_t mqd = -1;
static bool is_lambda;
static bool force_lambda = false;
static const UCS_string WHITESPACE = " \n\t\r\f\v";

#define EDIF2_DEFAULT \
  "emacs --geometry=60x20 -background '#ffffcc' -font 'DejaVu Sans Mono-10'"
/***
    alloced in get_signature
    freed in close_fun
***/
static char *edif2_default = NULL;

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

const Function *
real_get_fcn (UCS_string symbol_name)
{
  const Function * function = 0;
  while (symbol_name.back() <= ' ')   symbol_name.pop_back();
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
      NamedObject *obj = Workspace::lookup_existing_name(symbol_name);
      if (obj && obj->is_user_defined()) {
	function = obj->get_function();
	if (function && function->get_exec_properties()[0]) function = 0;
      }
    }
  }
  return function;
}

static bool
close_fun (Cause cause, const NativeFunction * caller)
{
  pthread_mutex_lock (mutex);
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
  pthread_mutex_unlock (mutex);

  pthread_mutex_lock (mutex);
  if (mqd != -1) {
    mq_close (mqd);
    mqd = -1;
  }
  pthread_mutex_unlock (mutex);

  pthread_mutex_lock (mutex);
  if (mq_name) {
    mq_unlink (mq_name);
    free (mq_name);
    mq_name = NULL;
  }
  pthread_mutex_unlock (mutex);


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
  
  pthread_mutex_lock (mutex);
  if (edif2_default) {
    free (edif2_default);
    edif2_default = NULL;
  }
  pthread_mutex_unlock (mutex);
  return false;
}

/***
    base_name == apl function name
    fn = fully qualified file name
***/

static void
read_file (const char *base_name, const char *fn)
{
  if (*base_name == '.') return;
  ifstream tfile;
  tfile.open (fn, ios::in);
  UCS_string ucs;
  UCS_string lambda_ucs;
  if (tfile.is_open ()) {
    string line;
    bool is_lambda_local =
      (0 == strncmp (base_name, LAMBDA_PREFIX, strlen (LAMBDA_PREFIX)));
    int cnt = 0;
    while (getline (tfile, line)) {
      ucs.append_UTF8 (line.c_str ());
      ucs.append(UNI_ASCII_LF);
      if (cnt++ == 0) lambda_ucs.append_UTF8 (line.c_str ());
    }
    tfile.close ();
    if (is_lambda_local) {
      if (lambda_ucs.has_black ()) {
	if (lambda_ucs.back () != L'←') {
	  int len = 0;
	  size_t strt = lambda_ucs.find_first_not_of(WHITESPACE);
	   UCS_string cpy = UCS_string (lambda_ucs, strt, string::npos);
	  lambda_ucs = cpy;
	  UTF8_string lambda_utf (lambda_ucs);
	  UTF8_string base_utf (base_name + strlen (LAMBDA_PREFIX));
	  size_t base_len = base_utf.size ();
	  int cmp = lambda_utf.compare (0, base_len, base_utf);

	  UCS_string target_name;
	  if (cmp != 0) {
	    UCS_string larrow (UTF8_string ("←"));
	    size_t arrow_offset = lambda_ucs.find_first_of (larrow);
	    target_name =
	      (arrow_offset == string::npos) ?
	      lambda_ucs : UCS_string (lambda_ucs, arrow_offset, string::npos);
	  }
	  else target_name = base_name + strlen (LAMBDA_PREFIX);
	  
	  Function *function = (Function *)real_get_fcn (target_name);
	  if (function != NULL) {
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
	UserFunction::fix (ucs,			// text
			   error_line,		// err_line
			   false,		// keep_existing
			   LOC,			// loc
			   creator_utf8,	// creator
			   true);		// tolerant
      }
    }
  }
}


static bool
enable_mq_notify ()
{
  union sigval sv;
  sv.sival_int = 0;
  struct sigevent sevp;
  sevp.sigev_notify = SIGEV_SIGNAL;
  sevp.sigev_value  = sv;
  sevp.sigev_signo  = MQ_SIGNAL;
  int rc = mq_notify (mqd, &sevp);
  return (rc == -1) ? false : true;
}

typedef struct {
  pid_t pid;
  time_t tv_sec;
  long   tv_nsec;
} ts_s;

ts_s last_time = {0, 0, 0};

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
	char *fn = NULL;
	asprintf (&fn, "%s/%s", dir, bfr);
	if (fn) {
	  struct stat result;
	  bool do_read = true;
	  if (0 == stat (fn, &result)) {
	    if (last_time.pid == 0 || last_time.pid == getpid ()) {
	      do_read =
		(result.st_mtim.tv_sec > last_time.tv_sec) ||
		(result.st_mtim.tv_nsec > last_time.tv_nsec);
	      last_time.pid = getpid ();
	      last_time.tv_sec  = result.st_mtim.tv_sec;
	      last_time.tv_nsec = result.st_mtim.tv_nsec;
	    }
	  }
	  *suffix = 0;
	  if (do_read) read_file (cpy, fn);
	  free (fn);
	}
      }
      free (cpy);
    }
  }
  
  if (!enable_mq_notify ())
    fprintf (stderr, "internal mq_notify error in edif2");
}


static void
watch_chld_handler(int sig, siginfo_t *si, void *data)
{
  int wstatus;
  while (0 < waitpid (si->si_pid, &wstatus, WNOHANG));
}


static void
edit_chld_handler(int sig, siginfo_t *si, void *data)
{
  int wstatus;
#ifdef USE_KIDS
  int i;
#endif
  waitpid (si->si_pid, &wstatus, 0 /* WNOHANG */);
#ifdef USE_KIDS
  for (i = 0; i < kids_nxt; i++) {if (kids[i] == si->si_pid) kids[i] = -1;}
#endif
  while (0 < waitpid (si->si_pid, &wstatus, WNOHANG)) {
#ifdef USE_KIDS
    for (i = 0; i < kids_nxt; i++) {if (kids[i] == si->si_pid) kids[i] = -1;}
#endif
  }
}

static void
msg_handler(int sig, siginfo_t *si, void *data)
{
  int wstatus;
  waitpid (si->si_pid, &wstatus, WNOHANG);
    //  while (0 < waitpid (si->si_pid, &wstatus, WNOHANG))
    handle_msg ();
}

Fun_signature
get_signature()
{
  if (mqd > 0) return SIG_Z_A_F2_B;	// already fixed

  mutex = (pthread_mutex_t *)mmap (NULL,
				   sizeof(pthread_mutex_t),
				   PROT_READ | PROT_WRITE,
				   MAP_ANONYMOUS | MAP_SHARED, -1, 0);
  pthread_mutexattr_init (&mutexattr);
  pthread_mutexattr_setpshared (&mutexattr, PTHREAD_PROCESS_SHARED);
  pthread_mutex_init (mutex, &mutexattr);
  pthread_mutexattr_destroy (&mutexattr);

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
  asprintf (&mq_name, "/APLEDIF_%d", (int)getpid ());
  mq_unlink (mq_name);
  mqd = mq_open (mq_name, O_RDWR | O_CREAT | O_NONBLOCK | O_EXCL,
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
  if (!enable_mq_notify ()) {
    fprintf (stderr, "internal mq_notify error in edif2");
    return SIG_NONE;
  }

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
	resend:
	  int mrc = mq_send (mqd, event->name, event->len, 0);
	  if (mrc == -1) {
	    if (errno == EINTR || errno == EAGAIN) {
	      fprintf (stderr, "again\n");
	      goto resend;
	    }
	    else perror ("internal mq_send error in edif2");
	  }
	}
      }
    }
  }

  return SIG_Z_A_F2_B;
}

// export EDIF2="emacs --geometry=80x60 -background '#ffffcc' -font 'DejaVu Sans Mono-10'"

static char *
get_fcn (const char *fn, const char *base, Value_P B)
{
  char *mfn = NULL;
  UCS_string symbol_name(*B.get());
  const Function * function = real_get_fcn (symbol_name);
  if (function != 0) {
    is_lambda = force_lambda || function->is_lambda();
    if (is_lambda)
      asprintf (&mfn, "%s/%s%s%s", dir, LAMBDA_PREFIX, base, APL_SUFFIX);
    else
      asprintf (&mfn, "%s/%s%s", dir, base, APL_SUFFIX);
    
    if (mfn) {				// freed in eval_EB
      const UCS_string ucs = function->canonical(false);
      UCS_string_vector tlines;
      ucs.to_vector(tlines);
      ofstream tfile;
      tfile.open (mfn, ios::out);
      loop(row, tlines.size()) {
	const UCS_string & line = tlines[row];
	UTF8_string utf (line);
	if (is_lambda) {
	  if (row == 0) continue;		// skip header
	  else {
	    utf = UCS_string (utf, 2, string::npos);	// skip assignment
	     tfile << base << "←{" << utf << "}";
	     break;
	  }
	}
	else tfile << utf << endl;
      }
      tfile.flush ();
      tfile.close ();
    }
  }
  else {			// new fcn
    if (force_lambda)
      asprintf (&mfn, "%s/%s%s%s", dir, LAMBDA_PREFIX, base, APL_SUFFIX);
    else
      mfn = strdup (fn);		// freed in eval_EB
    if (mfn) {
      ofstream tfile;
      tfile.open (mfn, ios::out);
      if (force_lambda)
	tfile << base << "←";
      else
	tfile << base << endl;
      tfile.flush ();
      tfile.close ();
    }
  }
  return mfn;
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
	//free (lfn);
      }
    }
    closedir (path);
  } 
  //  if (fn) free (fn);
}

static Token
eval_EB (const char *edif, Value_P B, APL_Integer idx)
{
  force_lambda = false;
  switch(idx) {
  case 1: force_lambda = true; break;
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
  if (B->is_char_string ()) {
    const UCS_string  ustr = B->get_UCS_ravel();
    UTF8_string base_name(ustr);
    char *fn = NULL;
    asprintf (&fn, "%s/%s%s", dir, base_name.c_str (), APL_SUFFIX);
    char *mfn = get_fcn (fn, base_name.c_str (), B);
    if (mfn) {
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
	      char *buf;
	      asprintf (&buf, "%s %s", edif, mfn);
	      int erc = execl("/bin/sh", "sh", "-c", buf, (char *) 0);
	      if (erc == -1) {
		if (buf) free (buf);
		UCS_string ucs ("Editor process failed to execute.");
		Value_P Z (ucs, LOC);
		Z->check_value (LOC);
		return Token (TOK_APL_VALUE1, Z);
	      }
	    }
	    //	  cleanup (dir, base_name, mfn);
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
      free (mfn);
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
eval_XB(Value_P X, Value_P B)
{
  APL_Integer val =
    (X->is_numeric_scalar()) ? X->get_sole_integer() : 0;
  return eval_EB (edif2_default, B, val);
}

static Token
eval_B (Value_P B)
{
  Value_P X = IntScalar (0, LOC);
  return eval_XB (X, B);
}

static Token
eval_AXB(Value_P A, Value_P X, Value_P B)
{
  if (A->is_char_string ()) {
    APL_Integer val =
      (X->is_numeric_scalar())  ? X->get_sole_integer()  : 0;

    const UCS_string  ustr = A->get_UCS_ravel();
    UTF8_string edif (ustr);
    if (edif.c_str () && *(edif.c_str ())) {
      if (edif2_default) free (edif2_default);
      edif2_default = strdup (edif.c_str ());
      return eval_EB (edif2_default, B, val);
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
eval_AB (Value_P A, Value_P B)
{
  Value_P X = IntScalar (0, LOC);
  return eval_AXB (A, X, B);
}

  
void *
get_function_mux (const char * function_name)
{
   if (!strcmp (function_name, "get_signature")) return (void *)&get_signature;
   if (!strcmp (function_name, "eval_B"))        return (void *)&eval_B;
   if (!strcmp (function_name, "eval_XB"))       return (void *)&eval_XB;
   if (!strcmp (function_name, "eval_AB"))       return (void *)&eval_AB;
   if (!strcmp (function_name, "eval_AXB"))      return (void *)&eval_AXB;
   if (!strcmp (function_name, "eval_ident_Bx")) return (void *)&eval_ident_Bx;
   if (!strcmp (function_name, "eval_fill_B"))   return (void *)&eval_fill_B;
   if (!strcmp (function_name, "eval_fill_AB"))  return (void *)&eval_fill_AB;
   if (!strcmp (function_name, "close_fun"))
     return reinterpret_cast<void *>(&close_fun);
   return 0;
}
