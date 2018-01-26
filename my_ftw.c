/**
 * This is a program that traverses the file hierarchy
 *
 * @Author: YangGuang <sunshine>
 * @Date:   2018-01-26T20:24:13+08:00
 * @Email:  guang334419520@126.com
 * @Filename: my_ftw.c
 * @Last modified by:   sunshine
 * @Last modified time: 2018-01-26T23:28:23+08:00
 */

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <limits.h>
//#include <malloc.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

//#define NAME_MAX      256
#define ERR_RET(str)  fprintf(stderr, "error : %s\n", str)
#define ERR_QUIT(str) fprintf(stderr, "error : %s\n", str); \
                        exit(0)
#define ERR_SYS(str) fprintf(stderr, "error : %s\n", str);  \
                       exit(-1)

enum Ftw{
  FTW_ISFILE = 1,       /* is a file type */
  FTW_ISDIR,            /* is a directory type */
  FTW_DIR_NOT_OPEN,     /* this directory is not open */
  FTW_NOT_STAT          /* file that we can't stat */
};


typedef int FuncType(const char *, const struct stat *, enum Ftw);

static FuncType CountFtw;
static int MyFtw(const char *, FuncType *);
static int DoPath(FuncType *);

static char   *g_fullpath;    /* contains full path name for every file */
static size_t g_pathlen;
static long g_reg, g_dir, g_blk, g_chr, g_fifo, g_slink, g_sock, g_tot;



#ifdef PATH_MAX
  static int  pathmax = PATH_MAX;
#else
  static int  pathmax = 0;
#endif

#define SUSV3  200112L
  static long  posix_version = 0;
#define PATH_MAX_GUESS 1024


char *path_alloc(size_t *sizep)
{
  char    *ptr;
  int size;

  if(posix_version == 0)
  posix_version = sysconf(_SC_VERSION);

  if(pathmax == 0){
    errno = 0;
    if((pathmax = pathconf("/", _PC_PATH_MAX)) < 0)
    {
      if(errno == 0)
      pathmax = PATH_MAX_GUESS;
      else { ERR_SYS("pathconf error for _PC_PATH_MAX"); }
    } else {
      pathmax++;
    }
  }
  if(posix_version < SUSV3)
  size = pathmax + 1;
  else size = pathmax;
  if((ptr = malloc(size)) == NULL)
  {ERR_SYS("malloc error for pathname");}
  if(sizep != NULL)
  *sizep = size;
  return(ptr);
}


int main(int argc, char const *argv[])
{
  int ret;
  if(argc != 2)
    {ERR_QUIT("usage: ftw <starting-pathname>");}

  ret = MyFtw(argv[1], CountFtw);

  g_tot = g_reg + g_dir + g_blk + g_chr +
          g_fifo + g_slink + g_sock;

  if (g_tot == 0)
    g_tot = 1;
  printf("regular files   = %7ld, %5.2f %%\n", g_reg,
          g_reg * 100.0 / g_tot);

  printf("directories     = %7ld, %5.2f %%\n", g_dir,
          g_dir * 100.0 / g_tot);

  printf("block special   = %7ld, %5.2f %%\n", g_blk,
          g_blk * 100.0 / g_tot);

  printf("char special    = %7ld, %5.2f %%\n", g_chr,
          g_chr * 100.0 / g_tot);

  printf("FIFOs           = %7ld, %5.2f %%\n", g_fifo,
          g_fifo * 100.0 / g_tot);

  printf("symbolic links  = %7ld, %5.2f %%\n", g_slink,
          g_slink * 100.0 / g_tot);

  printf("sockets         = %7ld, %5.2f %%\n", g_sock,
          g_sock * 100.0 / g_tot);

  return EXIT_SUCCESS;
}

/**
 * This function is build the initial path
 * @param  pathname path name
 * @param  func     The callback function
 * @return          int
 */
static int MyFtw(const char *pathname, FuncType *func)
{
  g_fullpath = path_alloc(&g_pathlen);

  if (g_pathlen <= strlen(pathname)) {  // Reallocate memory
    g_pathlen = strlen(pathname) * 2;

    if( (g_fullpath = realloc(g_fullpath, g_pathlen)) == NULL)
        ERR_SYS("realloc failed");
  }

  strcpy(g_fullpath, pathname);
  return DoPath(func);
}


static int DoPath(FuncType* func)
{
  struct stat   statbuf;
  struct dirent *dirp;
  DIR           *dp;
  int           ret, cur_len;

  if (lstat(g_fullpath, &statbuf) < 0) { /* stat error */
    return func(g_fullpath, &statbuf, FTW_NOT_STAT);
  }

  if (S_ISDIR(statbuf.st_mode) == 0)  { /* not a directory */
    return func(g_fullpath, &statbuf, FTW_ISFILE);
  }

  /*
   * it's a directory. First call func() for the directory,
   * then process each filename in the directory.
   */

  if ( (ret = func(g_fullpath, &statbuf, FTW_ISDIR)) != 0) {
    return ret;
  }

  cur_len = strlen(g_fullpath);

  if (cur_len + NAME_MAX + 2 > g_pathlen) { /* expand path buffer */
    g_pathlen *= 2;

    if( (g_fullpath = realloc(g_fullpath, g_pathlen)) == NULL) {
      ERR_SYS("realloc error");
    }

  }

  g_fullpath[cur_len++] = '/';
  g_fullpath[cur_len] = 0;

  if ( (dp = opendir(g_fullpath)) == NULL) {
    return func(g_fullpath, &statbuf, FTW_DIR_NOT_OPEN);
  }

  while ( (dirp = readdir(dp)) != NULL) {
    if(!strcmp(dirp->d_name, ".") ||
        !strcmp(dirp->d_name, "..")) {

      continue;
    }

    strcpy(g_fullpath + cur_len, dirp->d_name);

    if( (ret = DoPath(func)) != 0) {
      break;
    }
  }

  g_fullpath[cur_len - 1] = 0;

  closedir(dp);

  return ret;

}


static int
CountFtw(const char *pathname, const struct stat *statptr, enum Ftw type)
{
  switch (type) {
    case FTW_ISFILE:
      switch (statptr->st_mode & S_IFMT) {
        case S_IFREG:
          g_reg++;  break;
        case S_IFBLK:
          g_blk++;  break;
        case S_IFCHR:
          g_chr++;  break;
        case S_IFIFO:
          g_fifo++; break;
        case S_IFLNK:
          g_slink++; break;
        case S_IFSOCK:
          g_sock++; break;
        case S_IFDIR:
          ERR_RET("directories should have type = FTW_DIR");
      }

      break;
    case FTW_ISDIR:
      g_dir++;
      break;
    case FTW_DIR_NOT_OPEN:
      ERR_RET(pathname);
      break;
    case FTW_NOT_STAT:
      ERR_RET(pathname);
    default:
      ERR_RET(pathname);
  }

  return 0;
}
