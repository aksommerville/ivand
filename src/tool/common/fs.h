/* fs.h
 * Filesystem conveniences.
 */
 
#ifndef FS_H
#define FS_H

/* Read entire file at once.
 * We'll try first to measure it by seeking.
 * If that fails, we'll retry progressively.
 * (*dstpp) is always allocated on success and you must free it, even if length is zero.
 * If we report an error, we guarantee (*dstpp) was untouched.
 */
int file_read(void *dstpp,const char *path);

/* Write entire file at once.
 * Clobbers existing file, and deletes whatever's there if we fail midway.
 */
int file_write(const char *path,const void *src,int srcc);

/* Call (cb) for each file immediately under directory (path).
 * (type) is zero if dirent doesn't provide it, otherwise see file_get_type().
 * Terminates if (cb) returns nonzero, and returns the same.
 */
int dir_read(
  const char *path,
  int (*cb)(const char *path,const char *base,char type,void *userdata),
  void *userdata
);

/* Stat (path) and return one of:
 *   '!' Error, eg file not found
 *   'f' Regular file
 *   'd' Directory
 *   'c' Character device
 *   'b' Block device
 *   's' Socket
 *   'l' Link (not possible here; we use regular stat())
 *   '?' Something else (but file does exist)
 * File type zero is reserved for "not checked yet".
 */
char file_get_type(const char *path);

#endif
