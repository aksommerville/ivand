/* tinysd.h
 * Wrapper around SD lib with C linkage.
 */
 
#ifndef TINYSD_H
#define TINYSD_H

#ifdef __cplusplus
extern "C" {
#endif

int32_t tinysd_read(void *dst,int32_t dsta,const char *path);
int32_t tinysd_write(const char *path,const void *src,int32_t srcc);

#ifdef __cplusplus
}
#endif

#endif
