#ifndef INCLUDE_delta_apply_h__
#define INCLUDE_delta_apply_h__

/**
 * Apply a git binary delta to recover the original content.
 *
 * @param out the output buffer to receive the original data.
 *		Only out->data and out->len are populated, as this is
 *		the only information available in the delta.
 * @param base the base to copy from during copy instructions.
 * @param base_len number of bytes available at base.
 * @param delta the delta to execute copy/insert instructions from.
 * @param delta_len total number of bytes in the delta.
 * @return
 * - GIT_SUCCESS on a successful delta unpack.
 * - GIT_ERROR if the delta is corrupt or doesn't match the base.
 */
extern int git__delta_apply(
	git_obj *out,
	const unsigned char *base,
	size_t base_len,
	const unsigned char *delta,
	size_t delta_len);

#endif
