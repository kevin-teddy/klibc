#ident "$Id: run-init.c,v 1.1 2004/06/08 06:39:44 hpa Exp $"
/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2004 H. Peter Anvin - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *   
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *   
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * ----------------------------------------------------------------------- */

/*
 * run-init.c
 *
 * Usage: exec run-init /real-root /sbin/init "$@"
 *
 * This program should be called as the last thing in a shell script
 * acting as /init in an initramfs; it does the following:
 *
 * - Delete all files in the initramfs;
 * - Remounts /real-root onto the root filesystem;
 * - Chroots;
 * - Spawns the specified init program (with arguments.)
 */

#include <alloca.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/vfs.h>

static const char *program;

static void __attribute__((noreturn)) die(const char *msg)
{
  fprintf(stderr, "%s: %s: %s\n", program, msg, strerror(errno));
  exit(1);
}

static int nuke(const char *what);

static int nuke_dirent(int len, const char *dir, const char *name, dev_t me)
{
  int bytes = len+strlen(name)+2;
  char path[bytes];
  int xlen;
  struct stat st;

  xlen = snprintf(path, bytes, "%s/%s", dir, name);
  assert(xlen < bytes);

  if ( stat(path, &st) )
    return ENOENT;		/* Return 0 since already gone? */

  if ( st.st_dev != me )
    return 0;			/* DO NOT recurse down mount points!!!!! */

  return nuke(path);
}

/* Wipe the contents of a directory, but not the directory itself */
static int nuke_dir(const char *what)
{
  int len = strlen(what);
  DIR *dir;
  struct dirent *d;
  int err = 0;
  struct stat st;

  if ( stat(what, &st) )
    return errno;
  
  if ( !S_ISDIR(st.st_mode) )
    return ENOTDIR;

  if ( !(dir = opendir(what)) ) {
    /* EACCES means we can't read it.  Might be empty and removable;
       if not, the rmdir() in nuke() will trigger an error. */
    return (errno == EACCES) ? 0 : errno;
  }
  
  while ( (d = readdir(dir)) ) {
    /* Skip . and .. */
    if ( d->d_name[0] == '.' &&
	 (d->d_name[1] == '\0' ||
	  (d->d_name[1] == '.' && d->d_name[2] == '\0')) )
      continue;
    
    err = nuke_dirent(len, what, d->d_name, st.st_dev);
    if ( err ) {
      closedir(dir);
      return err;
    }
  }
  
  closedir(dir);

  return 0;
}

static int nuke(const char *what)
{
  int rv;
  int err = 0;

  rv = unlink(what);
  if ( rv < 0 ) {
    if ( errno == EISDIR ) {
      /* It's a directory. */
      err = nuke_dir(what);
      if ( !err ) err = rmdir(what) ? errno : err;
    } else {
      err = errno;
    }
  }

  if ( err ) {
    errno = err;
    die(what);
  } else {
    return 0;
  }
}

#define RAMFS_MAGIC	0x858458f6
#define TMPFS_MAGIC	0x01021994

int main(int argc, char *argv[])
{
  struct statfs sfs;

  program = argv[0];

  if ( argc < 3 ) {
    fprintf(stderr, "Usage: exec %s /real-root /sbin/init [args]\n", program);
    exit(1);
  }

  /* Make sure we're in the root directory */
  if ( chdir("/") )
    die("cd /");
    
  /* Make sure we're on a ramfs - this avoids accidents */
  if ( statfs(".", &sfs) )
    die("statfs /");
  if ( sfs.f_type != RAMFS_MAGIC && sfs.f_type != TMPFS_MAGIC )
    die("rootfs not a ramfs or tmpfs");

  /* Delete rootfs contents */
  if ( nuke_dir(".") )
    die("nuking initramfs contents");

  /* Overmount the root */
  if ( mount(argv[2], "/", NULL, MS_BIND, NULL) )
    die("overmounting root");
  
  /* Chroot */
  if ( chroot(".") )
    die("chroot");

  /* Spawn init */
  execv(argv[3], argv+3);

  die(argv[3]);			/* Failed to spawn init */
}

