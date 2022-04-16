/* serial.h
 */
 
#ifndef SERIAL_H
#define SERIAL_H

int sr_memcasecmp(const void *a,const void *b,int c);
int sr_isearch(const char *haystack,int hc,const char *needle,int nc);

/* Compare (src) against (pat):
 *   *   Any amount of anything.
 *   #   One or more decimal digits.
 *   \N  One literal match (case-sensitive).
 * Letters are case-insensitive and whitespace collapses.
 */
int sr_pattern_match(const char *src,int srcc,const char *pat,int patc);

/* (size) for integer and fixed-point can be negative if the input is signed, we'll extend the sign bit.
 * Integers and fixed-point scalars go up to 4 bytes.
 */
int sr_intle_decode(int *dst,const void *src,int srcc,int size);
int sr_intbe_decode(int *dst,const void *src,int srcc,int size);
int sr_vlq_decode(int *dst,const void *src,int srcc);
int sr_vlq5_decode(int *dst,const void *src,int srcc);
int sr_utf8_decode(int *dst,const void *src,int srcc);
int sr_fixed_decode(double *dst,const void *src,int srcc,int size,int fractbitc);
int sr_fixedf_decode(float *dst,const void *src,int srcc,int size,int fractbitc);

/* (size) in 1..4 (or -1..-4 for compatibility with decode, but it's meaningless).
 * vlq and utf8 are never longer than 4 bytes; vlq5 never longer than 5.
 */
int sr_intle_encode(void *dst,int dsta,int src,int size);
int sr_intbe_encode(void *dst,int dsta,int src,int size);
int sr_vlq_encode(void *dst,int dsta,int src);
int sr_vlq5_encode(void *dst,int dsta,int src);
int sr_utf8_encode(void *dst,int dsta,int src);
int sr_fixed_encode(void *dst,int dsta,double src,int size,int fractbitc);
int sr_fixedf_encode(void *dst,int dsta,float src,int size,int fractbitc);

/* Measuring a token does not fully validate it.
 * Leading and trailing space are included in json, but not the others.
 */
#define SR_NUMBER_SIGN     0x0001
#define SR_NUMBER_BASE     0x0002
#define SR_NUMBER_FRACTION 0x0004
#define SR_NUMBER_EXPONENT 0x0008
#define SR_STRING_SIMPLE   0x0010 /* no escapes */
int sr_number_measure(const char *src,int srcc,int *flags);
int sr_string_measure(const char *src,int srcc,int *flags);
int sr_ident_measure(const char *src,int srcc);
int sr_json_measure(const char *src,int srcc);

/* Integer must not contain fraction or exponent, and returns:
 *   0: overflow with data loss
 *   1: overflow into sign bit (can read as unsigned without loss)
 *   2: success
 * Float must not contain base, even "0d" would be an error.
 */
int sr_int_eval(int *dst,const char *src,int srcc);
int sr_float_eval(double *dst,const char *src,int srcc);
int sr_string_eval(char *dst,int dsta,const char *src,int srcc);

/* decuint: (digitc>=0) to pad, or (digitc<0) to pad or truncate.
 * string_repr_x: Non-JSON. Uses '\xNN' instead of messing with UTF-8.
 */
int sr_decsint_repr(char *dst,int dsta,int src);
int sr_decuint_repr(char *dst,int dsta,int src,int digitc);
int sr_hexuint_repr(char *dst,int dsta,int src,int digitc,int prefix);
int sr_float_repr(char *dst,int dsta,double src);
int sr_string_repr(char *dst,int dsta,const char *src,int srcc);
int sr_string_repr_x(char *dst,int dsta,const char *src,int srcc);

/* Evaluate one JSON expression, to a C number or string.
 * Our policy is anything can decode as anything.
 * These fail only if the input is not a valid JSON expression (NB outer whitespace not permitted).
 * Sometimes the decision doesn't make sense, pay attention:
 *   JSON   null true false number ""  "NUMBER" "..." []  {}  [...] {...}
 *   int    0    1    0     n      0   eval     -1    0   0   -1    -1
 *   float  NAN  1.0  0.0   n      0.0 eval     -1.0  NAN NAN -1.0  -1.0
 *   string .....verbatim......... .......eval......  ....verbatim.......
 */
int sr_int_from_json(int *dst,const char *src,int srcc);
int sr_float_from_json(double *dst,const char *src,int srcc);
int sr_string_from_json(char *dst,int dsta,const char *src,int srcc);

static inline int sr_digit_eval(char src) {
  if ((src>='0')&&(src<='9')) return src-'0';
  if ((src>='a')&&(src<='z')) return src-'a'+10;
  if ((src>='A')&&(src<='Z')) return src-'A'+10;
  return -1;
}

static inline int sr_hexdigit_eval(char src) {
  if ((src>='0')&&(src<='9')) return src-'0';
  if ((src>='a')&&(src<='f')) return src-'a'+10;
  if ((src>='A')&&(src<='F')) return src-'A'+10;
  return -1;
}

static inline char sr_hexdigit_repr(int src) {
  return "0123456789abcdef"[src&15];
}

static inline int sr_isident(char src) {
  if ((src>='a')&&(src<='z')) return 1;
  if ((src>='A')&&(src<='Z')) return 1;
  if ((src>='0')&&(src<='9')) return 1;
  if (src=='_') return 1;
  return 0;
}

int sr_base64_encode(char *dst,int dsta,const void *src,int srcc);
int sr_base64_decode(void *dst,int dsta,const char *src,int srcc);

int sr_urlencode_encode(char *dst,int dsta,const char *src,int srcc);
int sr_urlencode_decode(char *dst,int dsta,const char *src,int srcc);

// Double the payload's length and output only [0-9a-f]. eg for representing hashes.
int sr_hexstring_encode(char *dst,int dsta,const void *src,int srcc);
int sr_hexstring_decode(void *dst,int dsta,const char *src,int srcc);

int sr_md5(void *dst,int dsta,const void *src,int srcc);
int sr_sha1(void *dst,int dsta,const void *src,int srcc);

#endif
