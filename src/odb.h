#ifndef INCLUDE_odb_h__
#define INCLUDE_odb_h__

/** First 4 bytes of a pack-*.idx file header.
 *
 * Note this header exists only in idx v2 and later.  The idx v1
 * file format does not have a magic sequence at the front, and
 * must be detected by the first four bytes *not* being this value
 * and the first 8 bytes matching the following expression:
 *
 *   uint32_t *fanout = ... the file data at offset 0 ...
 *   ntohl(fanout[0]) < ntohl(fanout[1])
 *
 * The value chosen here for PACK_TOC is such that the above
 * cannot be true for an idx v1 file.
 */
#define PACK_TOC 0xff744f63 /* -1tOc */

/** First 4 bytes of a pack-*.pack file header. */
#define PACK_SIG 0x5041434b /* PACK */

#endif
