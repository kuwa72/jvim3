/* vi:ts=4:sw=4
 *
 * utf8.h
 *
 * UTF-8 is the internal representation of buffer text. Everything outside the
 * buffer (files, pipes, the IME, the terminal, the GUI) is converted at the
 * boundary; see kanji.c for the legacy encodings.
 *
 * The byte classification mirrors what the old Shift-JIS code meant, so the
 * existing call sites keep their meaning:
 *
 *      UTF8_ASCII (0)  a character that is one byte
 *      UTF8_LEAD  (1)  first byte of a multi-byte character
 *      UTF8_TAIL  (2)  a continuation byte
 *
 * Widths follow the same idea: a lead byte reports the width of the whole
 * character, a continuation byte reports 0. A loop that walks bytes and sums
 * widths therefore still gets the right answer.
 */

#ifndef JVIM_UTF8_H
#define JVIM_UTF8_H

#define UTF8_ASCII	0
#define UTF8_LEAD	1
#define UTF8_TAIL	2

#define UTF8_MAXLEN	4			/* bytes in the longest sequence we accept */
#define UTF8_ERROR	(-1)		/* utf_decode() on a malformed sequence */

/* Cheap byte tests, safe on any int. */
#define UTF8_ISLEAD(c)	(((unsigned)(c) & 0xffU) >= 0xc2U \
									&& ((unsigned)(c) & 0xffU) <= 0xf4U)
#define UTF8_ISTAIL(c)	((((unsigned)(c) & 0xffU) & 0xc0U) == 0x80U)

/* proto/utf8.pro has the functions. */

#endif /* JVIM_UTF8_H */
