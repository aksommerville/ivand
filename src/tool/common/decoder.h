/* decoder.h
 * General purpose decoder and encoder.
 */
 
#ifndef DECODER_H
#define DECODER_H

struct decoder;
struct encoder;

/* Decoder.
 * Cleanup is never necessary.
 * You can initialize inline, but must guarantee:
 *  - 0<=srcp<=srcc
 *  - (src) not null if (srcc>0)
 * Perfectly legal to copy a decoder directly, eg as a save point.
 ***************************************************************/
 
struct decoder {
  const void *src;
  int srcc;
  int srcp;
  int jsonctx; // 0,-1,'{','['
};

static inline int decoder_remaining(const struct decoder *decoder) {
  return decoder->srcc-decoder->srcp;
}

/* Binary scalars.
 * (size) for int or fixed is in bytes, negative to read signed.
 * "vlq5" is VLQ with 5-byte sequences permitted, possibly dropping some high bits.
 * Regular "vlq" is MIDI compatible.
 */
int decode_intle(int *dst,struct decoder *decoder,int size);
int decode_intbe(int *dst,struct decoder *decoder,int size);
int decode_vlq(int *dst,struct decoder *decoder);
int decode_vlq5(int *dst,struct decoder *decoder);
int decode_utf8(int *dst,struct decoder *decoder);
int decode_fixed(double *dst,struct decoder *decoder,int size,int fractbitc);
int decode_fixedf(float *dst,struct decoder *decoder,int size,int fractbitc);

/* "Decode" a chunk of raw data, optionally with a length prefix.
 * These allow null (dstpp), if you're just skipping chunks.
 */
int decode_raw(void *dstpp,struct decoder *decoder,int len);
int decode_intlelen(void *dstpp,struct decoder *decoder,int size);
int decode_intbelen(void *dstpp,struct decoder *decoder,int size);
int decode_vlqlen(void *dstpp,struct decoder *decoder);
int decode_vlq5len(void *dstpp,struct decoder *decoder);

// Skip a chunk if it matches (src) exactly, or fail. eg for signatures
int decode_assert(struct decoder *decoder,const void *src,int srcc);

/* Return through the next LF. (implicitly handles CRLF nice too).
 */
int decode_line(void *dstpp,struct decoder *decoder);

/* JSON structures.
 * "start" returns (jsonctx) on success, which you must give back at "end".
 * "next" returns 0 if complete, >0 if a value is ready to decode, or <0 for error.
 * When "next" reports a value present, you must decode or skip it.
 * You don't have to handle errors at next; they are sticky.
 * You MUST provide a key vector for objects and MUST NOT for arrays.
 * We don't evaluate keys, rather we assume they will always be simple strings and just lop off the quotes.
 * If an empty key is found, we report it with the quotes (because empty would look like end-of-object).
 */
int decode_json_object_start(struct decoder *decoder);
int decode_json_array_start(struct decoder *decoder);
int decode_json_object_end(struct decoder *decoder,int jsonctx);
int decode_json_array_end(struct decoder *decoder,int jsonctx);
int decode_json_next(void *kpp,struct decoder *decoder);

/* JSON primitives.
 * "raw" returns some portion of the input text, corresponding to one JSON expression.
 * "skip" is exactly the same as "raw" but no return value.
 * "save_for_later" pops one expression like "raw", and saves it in another decoder.
 * decode_json_string() is special: It DOES NOT consume the token if it reports a length >dsta.
 * Anything can evaluate as anything -- see sr_*_from_json() in serial.h.
 */
int decode_json_raw(void *dstpp,struct decoder *decoder);
int decode_json_skip(struct decoder *decoder);
int decode_json_save_for_later(struct decoder *later,struct decoder *decoder);
int decode_json_int(int *dst,struct decoder *decoder);
int decode_json_float(double *dst,struct decoder *decoder);
int decode_json_string(char *dst,int dsta,struct decoder *decoder);
int decode_json_string_to_encoder(struct encoder *dst,struct decoder *decoder);
char *decode_json_zstring(struct decoder *decoder);

/* Peek at the next token and describe it:
 *   0: Invalid or sticky error.
 *   '{': Object
 *   '[': Array
 *   '"': String
 *   '#': Number (integer or float, we don't check)
 *   't': true
 *   'f': false
 *   'n': null
 * We only examine the first character of the token; it's not guaranteed to decode.
 */
char decode_json_get_type(const struct decoder *decoder);

// Assert successful completion. (note that this also succeeds if you never started decoding)
int decode_json_done(const struct decoder *decoder);

/* Encoder.
 * Cleanup is necessary, but you can also safely yoink (v) and not clean up.
 ***************************************************************/
 
struct encoder {
  char *v;
  int c,a;
  int jsonctx; // 0,-1,'{','['
};

void encoder_cleanup(struct encoder *encoder);
int encoder_require(struct encoder *encoder,int addc);

int encoder_replace(struct encoder *encoder,int p,int c,const void *src,int srcc);

/* Append a chunk of raw data, optionally with a preceding length.
 * (srcc<0) to measure to the first NUL exclusive.
 */
int encode_null(struct encoder *encoder,int c);
int encode_raw(struct encoder *encoder,const void *src,int srcc);
int encode_intlelen(struct encoder *encoder,const void *src,int srcc,int size);
int encode_intbelen(struct encoder *encoder,const void *src,int srcc,int size);
int encode_vlqlen(struct encoder *encoder,const void *src,int srcc);
int encode_vlq5len(struct encoder *encoder,const void *src,int srcc);

/* Insert a word of the given type at the given position, value is from there to the end.
 * This is an alternative to encode_*len(), where the length is not known in advance.
 */
int encoder_insert_intlelen(struct encoder *encoder,int p,int size);
int encoder_insert_intbelen(struct encoder *encoder,int p,int size);
int encoder_insert_vlqlen(struct encoder *encoder,int p);
int encoder_insert_vlq5len(struct encoder *encoder,int p);

int encode_fmt(struct encoder *encoder,const char *fmt,...);

int encode_intle(struct encoder *encoder,int src,int size);
int encode_intbe(struct encoder *encoder,int src,int size);
int encode_vlq(struct encoder *encoder,int src);
int encode_vlq5(struct encoder *encoder,int src);
int encode_utf8(struct encoder *encoder,int src);
int encode_fixed(struct encoder *encoder,double src,int size,int fractbitc);
int encode_fixedf(struct encoder *encoder,float src,int size,int fractbitc);

int encode_base64(struct encoder *encoder,const void *src,int srcc);

// This does not interact with the JSON context; mostly it's used internally.
int encode_json_string_token(struct encoder *encoder,const char *src,int srcc);

/* JSON structures.
 * Same as decode, you must hold the (jsonctx) returned at "start", until the corresponding "end".
 * We pack JSON tightly. You are free to append spaces and newlines whenever you have control of the encoder.
 */
int encode_json_object_start(struct encoder *encoder,const char *k,int kc);
int encode_json_array_start(struct encoder *encoder,const char *k,int kc);
int encode_json_object_end(struct encoder *encoder,int jsonctx);
int encode_json_array_end(struct encoder *encoder,int jsonctx);

/* JSON primitives.
 * If immediately inside an object, you MUST provide a key.
 * Inside an array, or at global scope, you MUST NOT.
 * "preencoded" takes JSON text as the value and emits it verbatim -- keeping the format valid is up to you.
 */
int encode_json_preencoded(struct encoder *encoder,const char *k,int kc,const char *v,int vc);
int encode_json_null(struct encoder *encoder,const char *k,int kc);
int encode_json_boolean(struct encoder *encoder,const char *k,int kc,int v);
int encode_json_int(struct encoder *encoder,const char *k,int kc,int v);
int encode_json_float(struct encoder *encoder,const char *k,int kc,double v);
int encode_json_string(struct encoder *encoder,const char *k,int kc,const char *v,int vc);

// Assert completion (does not output anything).
int encode_json_done(const struct encoder *encoder);

#endif
