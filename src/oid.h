#ifndef INCLUDE_oid_h__
#define INCLUDE_oid_h__

/**
 * Compare the first ('len'*4) bits of two raw formatted oids.
 * This can be useful for internal use.
 * Return 0 if they match.
 */ 
int git_oid_match_raw(unsigned int len, const unsigned char *a, const unsigned char *b);

/**
 * Compare the first 'len' characters of two hex formatted oids.
 * Return 0 if they match.
 */
int git_oid_match_hex(unsigned int len, const unsigned char *a, const unsigned char *b);

#endif
