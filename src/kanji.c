/*******************************************************************************
 * vi:ts=4:sw=4
 *	original Ogasawara Hiroyuki (COR.)
 *  original Atsushi Nakamura
 ******************************************************************************/
#ifdef KANJI

#ifdef MSDOS
#include	<io.h>
#endif
#ifdef NT
# include <windows.h>
# undef DELETE
#endif
#include	"vim.h"
#include	"globals.h"
#include	"proto.h"
#include	"param.h"
#include	"ops.h"
#include	"kanji.h"
#include	"jptab.h"
#include	"utf8.h"

#define	JP_EUC_G2		0x8e
#define	JP_EUC_G3		0x8f
#define IS_X0212(_c)	(((_c) & 0xf0) == 0xf0)

static int_u	euctosjis		__ARGS((char_u, char_u));
static int_u	euctosjis3		__ARGS((char_u, char_u));
static int_u	sjistoeuc		__ARGS((char_u, char_u, char_u *));
static int_u	sjistoeuc3		__ARGS((char_u, char_u, char_u *));
static char_u *	kanjiin			__ARGS((int));
static char_u *	asciiin			__ARGS((int));
static char_u *	kanain			__ARGS((int));
static char_u *	JPdisp			__ARGS((int *, int, int));
static int		jisx0201rto0208	__ARGS((char_u, char_u, char_u *, char_u *));

/*
 * The internal representation is UTF-8; see utf8.c. The conversion engines
 * below still speak Shift-JIS, because that is the pivot the EUC/JIS/Shift-JIS
 * tables are built around, so the public kanjiconvsfrom()/kanjiconvsto() wrap
 * them with a Shift-JIS <-> UTF-8 step. UTF-8 and UCS-2 files skip the pivot
 * entirely, which is what keeps characters outside CP932 intact.
 */
static int		sjis_convsfrom __ARGS((char_u *, int, char_u *, int, char *,
											char, int *));
static char_u  *sjis_convsto __ARGS((char_u *, int, int));
static int		sjis_islead __ARGS((int));
static int		sjis_iskana __ARGS((int));
static int		sjis_isdisp __ARGS((int));
static int		sjis2cp __ARGS((char_u *, int));
static int		cp2sjis __ARGS((int, char_u *));
static int		sjis2utf8_n __ARGS((char_u *, int, char_u *, int));
static char_u  *utf82sjis __ARGS((char_u *));
static char_u  *utf82ucs2 __ARGS((char_u *, int));
static int		iskeycode __ARGS((char_u *, int, int *));

static char_u	kanji_map_sjis[]= {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0
};

static char_u	kanji_map_euc[]= {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,3,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0
};

static struct {
	char_u		sjis[2];
	char_u		alt[2];
} altconv[] = {
	{{0xFA, 0x40}, {0xEE, 0xEF}}, {{0xFA, 0x41}, {0xEE, 0xF0}},
	{{0xFA, 0x42}, {0xEE, 0xF1}}, {{0xFA, 0x43}, {0xEE, 0xF2}},
	{{0xFA, 0x44}, {0xEE, 0xF3}}, {{0xFA, 0x45}, {0xEE, 0xF4}},
	{{0xFA, 0x46}, {0xEE, 0xF5}}, {{0xFA, 0x47}, {0xEE, 0xF6}},
	{{0xFA, 0x48}, {0xEE, 0xF7}}, {{0xFA, 0x49}, {0xEE, 0xF8}},
	{{0xFA, 0x4A}, {0x87, 0x54}}, {{0xFA, 0x4B}, {0x87, 0x55}},
	{{0xFA, 0x4C}, {0x87, 0x56}}, {{0xFA, 0x4D}, {0x87, 0x57}},
	{{0xFA, 0x4E}, {0x87, 0x58}}, {{0xFA, 0x4F}, {0x87, 0x59}},
	{{0xFA, 0x50}, {0x87, 0x5A}}, {{0xFA, 0x51}, {0x87, 0x5B}},
	{{0xFA, 0x52}, {0x87, 0x5C}}, {{0xFA, 0x53}, {0x87, 0x5D}},
	{{0xFA, 0x54}, {0x81, 0xCA}}, {{0xFA, 0x55}, {0xEE, 0xFA}},
	{{0xFA, 0x56}, {0xEE, 0xFB}}, {{0xFA, 0x57}, {0xEE, 0xFC}},
	{{0xFA, 0x58}, {0x87, 0x8A}}, {{0xFA, 0x59}, {0x87, 0x82}},
	{{0xFA, 0x5A}, {0x87, 0x84}}, {{0xFA, 0x5B}, {0x81, 0xE6}},
	{{0xFA, 0x5C}, {0xED, 0x40}}, {{0xFA, 0x5D}, {0xED, 0x41}},
	{{0xFA, 0x5E}, {0xED, 0x42}}, {{0xFA, 0x5F}, {0xED, 0x43}},
	{{0xFA, 0x60}, {0xED, 0x44}}, {{0xFA, 0x61}, {0xED, 0x45}},
	{{0xFA, 0x62}, {0xED, 0x46}}, {{0xFA, 0x63}, {0xED, 0x47}},
	{{0xFA, 0x64}, {0xED, 0x48}}, {{0xFA, 0x65}, {0xED, 0x49}},
	{{0xFA, 0x66}, {0xED, 0x4A}}, {{0xFA, 0x67}, {0xED, 0x4B}},
	{{0xFA, 0x68}, {0xED, 0x4C}}, {{0xFA, 0x69}, {0xED, 0x4D}},
	{{0xFA, 0x6A}, {0xED, 0x4E}}, {{0xFA, 0x6B}, {0xED, 0x4F}},
	{{0xFA, 0x6C}, {0xED, 0x50}}, {{0xFA, 0x6D}, {0xED, 0x51}},
	{{0xFA, 0x6E}, {0xED, 0x52}}, {{0xFA, 0x6F}, {0xED, 0x53}},
	{{0xFA, 0x70}, {0xED, 0x54}}, {{0xFA, 0x71}, {0xED, 0x55}},
	{{0xFA, 0x72}, {0xED, 0x56}}, {{0xFA, 0x73}, {0xED, 0x57}},
	{{0xFA, 0x74}, {0xED, 0x58}}, {{0xFA, 0x75}, {0xED, 0x59}},
	{{0xFA, 0x76}, {0xED, 0x5A}}, {{0xFA, 0x77}, {0xED, 0x5B}},
	{{0xFA, 0x78}, {0xED, 0x5C}}, {{0xFA, 0x79}, {0xED, 0x5D}},
	{{0xFA, 0x7A}, {0xED, 0x5E}}, {{0xFA, 0x7B}, {0xED, 0x5F}},
	{{0xFA, 0x7C}, {0xED, 0x60}}, {{0xFA, 0x7D}, {0xED, 0x61}},
	{{0xFA, 0x7E}, {0xED, 0x62}}, {{0xFA, 0x80}, {0xED, 0x63}},
	{{0xFA, 0x81}, {0xED, 0x64}}, {{0xFA, 0x82}, {0xED, 0x65}},
	{{0xFA, 0x83}, {0xED, 0x66}}, {{0xFA, 0x84}, {0xED, 0x67}},
	{{0xFA, 0x85}, {0xED, 0x68}}, {{0xFA, 0x86}, {0xED, 0x69}},
	{{0xFA, 0x87}, {0xED, 0x6A}}, {{0xFA, 0x88}, {0xED, 0x6B}},
	{{0xFA, 0x89}, {0xED, 0x6C}}, {{0xFA, 0x8A}, {0xED, 0x6D}},
	{{0xFA, 0x8B}, {0xED, 0x6E}}, {{0xFA, 0x8C}, {0xED, 0x6F}},
	{{0xFA, 0x8D}, {0xED, 0x70}}, {{0xFA, 0x8E}, {0xED, 0x71}},
	{{0xFA, 0x8F}, {0xED, 0x72}}, {{0xFA, 0x90}, {0xED, 0x73}},
	{{0xFA, 0x91}, {0xED, 0x74}}, {{0xFA, 0x92}, {0xED, 0x75}},
	{{0xFA, 0x93}, {0xED, 0x76}}, {{0xFA, 0x94}, {0xED, 0x77}},
	{{0xFA, 0x95}, {0xED, 0x78}}, {{0xFA, 0x96}, {0xED, 0x79}},
	{{0xFA, 0x97}, {0xED, 0x7A}}, {{0xFA, 0x98}, {0xED, 0x7B}},
	{{0xFA, 0x99}, {0xED, 0x7C}}, {{0xFA, 0x9A}, {0xED, 0x7D}},
	{{0xFA, 0x9B}, {0xED, 0x7E}}, {{0xFA, 0x9C}, {0xED, 0x80}},
	{{0xFA, 0x9D}, {0xED, 0x81}}, {{0xFA, 0x9E}, {0xED, 0x82}},
	{{0xFA, 0x9F}, {0xED, 0x83}}, {{0xFA, 0xA0}, {0xED, 0x84}},
	{{0xFA, 0xA1}, {0xED, 0x85}}, {{0xFA, 0xA2}, {0xED, 0x86}},
	{{0xFA, 0xA3}, {0xED, 0x87}}, {{0xFA, 0xA4}, {0xED, 0x88}},
	{{0xFA, 0xA5}, {0xED, 0x89}}, {{0xFA, 0xA6}, {0xED, 0x8A}},
	{{0xFA, 0xA7}, {0xED, 0x8B}}, {{0xFA, 0xA8}, {0xED, 0x8C}},
	{{0xFA, 0xA9}, {0xED, 0x8D}}, {{0xFA, 0xAA}, {0xED, 0x8E}},
	{{0xFA, 0xAB}, {0xED, 0x8F}}, {{0xFA, 0xAC}, {0xED, 0x90}},
	{{0xFA, 0xAD}, {0xED, 0x91}}, {{0xFA, 0xAE}, {0xED, 0x92}},
	{{0xFA, 0xAF}, {0xED, 0x93}}, {{0xFA, 0xB0}, {0xED, 0x94}},
	{{0xFA, 0xB1}, {0xED, 0x95}}, {{0xFA, 0xB2}, {0xED, 0x96}},
	{{0xFA, 0xB3}, {0xED, 0x97}}, {{0xFA, 0xB4}, {0xED, 0x98}},
	{{0xFA, 0xB5}, {0xED, 0x99}}, {{0xFA, 0xB6}, {0xED, 0x9A}},
	{{0xFA, 0xB7}, {0xED, 0x9B}}, {{0xFA, 0xB8}, {0xED, 0x9C}},
	{{0xFA, 0xB9}, {0xED, 0x9D}}, {{0xFA, 0xBA}, {0xED, 0x9E}},
	{{0xFA, 0xBB}, {0xED, 0x9F}}, {{0xFA, 0xBC}, {0xED, 0xA0}},
	{{0xFA, 0xBD}, {0xED, 0xA1}}, {{0xFA, 0xBE}, {0xED, 0xA2}},
	{{0xFA, 0xBF}, {0xED, 0xA3}}, {{0xFA, 0xC0}, {0xED, 0xA4}},
	{{0xFA, 0xC1}, {0xED, 0xA5}}, {{0xFA, 0xC2}, {0xED, 0xA6}},
	{{0xFA, 0xC3}, {0xED, 0xA7}}, {{0xFA, 0xC4}, {0xED, 0xA8}},
	{{0xFA, 0xC5}, {0xED, 0xA9}}, {{0xFA, 0xC6}, {0xED, 0xAA}},
	{{0xFA, 0xC7}, {0xED, 0xAB}}, {{0xFA, 0xC8}, {0xED, 0xAC}},
	{{0xFA, 0xC9}, {0xED, 0xAD}}, {{0xFA, 0xCA}, {0xED, 0xAE}},
	{{0xFA, 0xCB}, {0xED, 0xAF}}, {{0xFA, 0xCC}, {0xED, 0xB0}},
	{{0xFA, 0xCD}, {0xED, 0xB1}}, {{0xFA, 0xCE}, {0xED, 0xB2}},
	{{0xFA, 0xCF}, {0xED, 0xB3}}, {{0xFA, 0xD0}, {0xED, 0xB4}},
	{{0xFA, 0xD1}, {0xED, 0xB5}}, {{0xFA, 0xD2}, {0xED, 0xB6}},
	{{0xFA, 0xD3}, {0xED, 0xB7}}, {{0xFA, 0xD4}, {0xED, 0xB8}},
	{{0xFA, 0xD5}, {0xED, 0xB9}}, {{0xFA, 0xD6}, {0xED, 0xBA}},
	{{0xFA, 0xD7}, {0xED, 0xBB}}, {{0xFA, 0xD8}, {0xED, 0xBC}},
	{{0xFA, 0xD9}, {0xED, 0xBD}}, {{0xFA, 0xDA}, {0xED, 0xBE}},
	{{0xFA, 0xDB}, {0xED, 0xBF}}, {{0xFA, 0xDC}, {0xED, 0xC0}},
	{{0xFA, 0xDD}, {0xED, 0xC1}}, {{0xFA, 0xDE}, {0xED, 0xC2}},
	{{0xFA, 0xDF}, {0xED, 0xC3}}, {{0xFA, 0xE0}, {0xED, 0xC4}},
	{{0xFA, 0xE1}, {0xED, 0xC5}}, {{0xFA, 0xE2}, {0xED, 0xC6}},
	{{0xFA, 0xE3}, {0xED, 0xC7}}, {{0xFA, 0xE4}, {0xED, 0xC8}},
	{{0xFA, 0xE5}, {0xED, 0xC9}}, {{0xFA, 0xE6}, {0xED, 0xCA}},
	{{0xFA, 0xE7}, {0xED, 0xCB}}, {{0xFA, 0xE8}, {0xED, 0xCC}},
	{{0xFA, 0xE9}, {0xED, 0xCD}}, {{0xFA, 0xEA}, {0xED, 0xCE}},
	{{0xFA, 0xEB}, {0xED, 0xCF}}, {{0xFA, 0xEC}, {0xED, 0xD0}},
	{{0xFA, 0xED}, {0xED, 0xD1}}, {{0xFA, 0xEE}, {0xED, 0xD2}},
	{{0xFA, 0xEF}, {0xED, 0xD3}}, {{0xFA, 0xF0}, {0xED, 0xD4}},
	{{0xFA, 0xF1}, {0xED, 0xD5}}, {{0xFA, 0xF2}, {0xED, 0xD6}},
	{{0xFA, 0xF3}, {0xED, 0xD7}}, {{0xFA, 0xF4}, {0xED, 0xD8}},
	{{0xFA, 0xF5}, {0xED, 0xD9}}, {{0xFA, 0xF6}, {0xED, 0xDA}},
	{{0xFA, 0xF7}, {0xED, 0xDB}}, {{0xFA, 0xF8}, {0xED, 0xDC}},
	{{0xFA, 0xF9}, {0xED, 0xDD}}, {{0xFA, 0xFA}, {0xED, 0xDE}},
	{{0xFA, 0xFB}, {0xED, 0xDF}}, {{0xFA, 0xFC}, {0xED, 0xE0}},
	{{0xFB, 0x40}, {0xED, 0xE1}}, {{0xFB, 0x41}, {0xED, 0xE2}},
	{{0xFB, 0x42}, {0xED, 0xE3}}, {{0xFB, 0x43}, {0xED, 0xE4}},
	{{0xFB, 0x44}, {0xED, 0xE5}}, {{0xFB, 0x45}, {0xED, 0xE6}},
	{{0xFB, 0x46}, {0xED, 0xE7}}, {{0xFB, 0x47}, {0xED, 0xE8}},
	{{0xFB, 0x48}, {0xED, 0xE9}}, {{0xFB, 0x49}, {0xED, 0xEA}},
	{{0xFB, 0x4A}, {0xED, 0xEB}}, {{0xFB, 0x4B}, {0xED, 0xEC}},
	{{0xFB, 0x4C}, {0xED, 0xED}}, {{0xFB, 0x4D}, {0xED, 0xEE}},
	{{0xFB, 0x4E}, {0xED, 0xEF}}, {{0xFB, 0x4F}, {0xED, 0xF0}},
	{{0xFB, 0x50}, {0xED, 0xF1}}, {{0xFB, 0x51}, {0xED, 0xF2}},
	{{0xFB, 0x52}, {0xED, 0xF3}}, {{0xFB, 0x53}, {0xED, 0xF4}},
	{{0xFB, 0x54}, {0xED, 0xF5}}, {{0xFB, 0x55}, {0xED, 0xF6}},
	{{0xFB, 0x56}, {0xED, 0xF7}}, {{0xFB, 0x57}, {0xED, 0xF8}},
	{{0xFB, 0x58}, {0xED, 0xF9}}, {{0xFB, 0x59}, {0xED, 0xFA}},
	{{0xFB, 0x5A}, {0xED, 0xFB}}, {{0xFB, 0x5B}, {0xED, 0xFC}},
	{{0xFB, 0x5C}, {0xEE, 0x40}}, {{0xFB, 0x5D}, {0xEE, 0x41}},
	{{0xFB, 0x5E}, {0xEE, 0x42}}, {{0xFB, 0x5F}, {0xEE, 0x43}},
	{{0xFB, 0x60}, {0xEE, 0x44}}, {{0xFB, 0x61}, {0xEE, 0x45}},
	{{0xFB, 0x62}, {0xEE, 0x46}}, {{0xFB, 0x63}, {0xEE, 0x47}},
	{{0xFB, 0x64}, {0xEE, 0x48}}, {{0xFB, 0x65}, {0xEE, 0x49}},
	{{0xFB, 0x66}, {0xEE, 0x4A}}, {{0xFB, 0x67}, {0xEE, 0x4B}},
	{{0xFB, 0x68}, {0xEE, 0x4C}}, {{0xFB, 0x69}, {0xEE, 0x4D}},
	{{0xFB, 0x6A}, {0xEE, 0x4E}}, {{0xFB, 0x6B}, {0xEE, 0x4F}},
	{{0xFB, 0x6C}, {0xEE, 0x50}}, {{0xFB, 0x6D}, {0xEE, 0x51}},
	{{0xFB, 0x6E}, {0xEE, 0x52}}, {{0xFB, 0x6F}, {0xEE, 0x53}},
	{{0xFB, 0x70}, {0xEE, 0x54}}, {{0xFB, 0x71}, {0xEE, 0x55}},
	{{0xFB, 0x72}, {0xEE, 0x56}}, {{0xFB, 0x73}, {0xEE, 0x57}},
	{{0xFB, 0x74}, {0xEE, 0x58}}, {{0xFB, 0x75}, {0xEE, 0x59}},
	{{0xFB, 0x76}, {0xEE, 0x5A}}, {{0xFB, 0x77}, {0xEE, 0x5B}},
	{{0xFB, 0x78}, {0xEE, 0x5C}}, {{0xFB, 0x79}, {0xEE, 0x5D}},
	{{0xFB, 0x7A}, {0xEE, 0x5E}}, {{0xFB, 0x7B}, {0xEE, 0x5F}},
	{{0xFB, 0x7C}, {0xEE, 0x60}}, {{0xFB, 0x7D}, {0xEE, 0x61}},
	{{0xFB, 0x7E}, {0xEE, 0x62}}, {{0xFB, 0x80}, {0xEE, 0x63}},
	{{0xFB, 0x81}, {0xEE, 0x64}}, {{0xFB, 0x82}, {0xEE, 0x65}},
	{{0xFB, 0x83}, {0xEE, 0x66}}, {{0xFB, 0x84}, {0xEE, 0x67}},
	{{0xFB, 0x85}, {0xEE, 0x68}}, {{0xFB, 0x86}, {0xEE, 0x69}},
	{{0xFB, 0x87}, {0xEE, 0x6A}}, {{0xFB, 0x88}, {0xEE, 0x6B}},
	{{0xFB, 0x89}, {0xEE, 0x6C}}, {{0xFB, 0x8A}, {0xEE, 0x6D}},
	{{0xFB, 0x8B}, {0xEE, 0x6E}}, {{0xFB, 0x8C}, {0xEE, 0x6F}},
	{{0xFB, 0x8D}, {0xEE, 0x70}}, {{0xFB, 0x8E}, {0xEE, 0x71}},
	{{0xFB, 0x8F}, {0xEE, 0x72}}, {{0xFB, 0x90}, {0xEE, 0x73}},
	{{0xFB, 0x91}, {0xEE, 0x74}}, {{0xFB, 0x92}, {0xEE, 0x75}},
	{{0xFB, 0x93}, {0xEE, 0x76}}, {{0xFB, 0x94}, {0xEE, 0x77}},
	{{0xFB, 0x95}, {0xEE, 0x78}}, {{0xFB, 0x96}, {0xEE, 0x79}},
	{{0xFB, 0x97}, {0xEE, 0x7A}}, {{0xFB, 0x98}, {0xEE, 0x7B}},
	{{0xFB, 0x99}, {0xEE, 0x7C}}, {{0xFB, 0x9A}, {0xEE, 0x7D}},
	{{0xFB, 0x9B}, {0xEE, 0x7E}}, {{0xFB, 0x9C}, {0xEE, 0x80}},
	{{0xFB, 0x9D}, {0xEE, 0x81}}, {{0xFB, 0x9E}, {0xEE, 0x82}},
	{{0xFB, 0x9F}, {0xEE, 0x83}}, {{0xFB, 0xA0}, {0xEE, 0x84}},
	{{0xFB, 0xA1}, {0xEE, 0x85}}, {{0xFB, 0xA2}, {0xEE, 0x86}},
	{{0xFB, 0xA3}, {0xEE, 0x87}}, {{0xFB, 0xA4}, {0xEE, 0x88}},
	{{0xFB, 0xA5}, {0xEE, 0x89}}, {{0xFB, 0xA6}, {0xEE, 0x8A}},
	{{0xFB, 0xA7}, {0xEE, 0x8B}}, {{0xFB, 0xA8}, {0xEE, 0x8C}},
	{{0xFB, 0xA9}, {0xEE, 0x8D}}, {{0xFB, 0xAA}, {0xEE, 0x8E}},
	{{0xFB, 0xAB}, {0xEE, 0x8F}}, {{0xFB, 0xAC}, {0xEE, 0x90}},
	{{0xFB, 0xAD}, {0xEE, 0x91}}, {{0xFB, 0xAE}, {0xEE, 0x92}},
	{{0xFB, 0xAF}, {0xEE, 0x93}}, {{0xFB, 0xB0}, {0xEE, 0x94}},
	{{0xFB, 0xB1}, {0xEE, 0x95}}, {{0xFB, 0xB2}, {0xEE, 0x96}},
	{{0xFB, 0xB3}, {0xEE, 0x97}}, {{0xFB, 0xB4}, {0xEE, 0x98}},
	{{0xFB, 0xB5}, {0xEE, 0x99}}, {{0xFB, 0xB6}, {0xEE, 0x9A}},
	{{0xFB, 0xB7}, {0xEE, 0x9B}}, {{0xFB, 0xB8}, {0xEE, 0x9C}},
	{{0xFB, 0xB9}, {0xEE, 0x9D}}, {{0xFB, 0xBA}, {0xEE, 0x9E}},
	{{0xFB, 0xBB}, {0xEE, 0x9F}}, {{0xFB, 0xBC}, {0xEE, 0xA0}},
	{{0xFB, 0xBD}, {0xEE, 0xA1}}, {{0xFB, 0xBE}, {0xEE, 0xA2}},
	{{0xFB, 0xBF}, {0xEE, 0xA3}}, {{0xFB, 0xC0}, {0xEE, 0xA4}},
	{{0xFB, 0xC1}, {0xEE, 0xA5}}, {{0xFB, 0xC2}, {0xEE, 0xA6}},
	{{0xFB, 0xC3}, {0xEE, 0xA7}}, {{0xFB, 0xC4}, {0xEE, 0xA8}},
	{{0xFB, 0xC5}, {0xEE, 0xA9}}, {{0xFB, 0xC6}, {0xEE, 0xAA}},
	{{0xFB, 0xC7}, {0xEE, 0xAB}}, {{0xFB, 0xC8}, {0xEE, 0xAC}},
	{{0xFB, 0xC9}, {0xEE, 0xAD}}, {{0xFB, 0xCA}, {0xEE, 0xAE}},
	{{0xFB, 0xCB}, {0xEE, 0xAF}}, {{0xFB, 0xCC}, {0xEE, 0xB0}},
	{{0xFB, 0xCD}, {0xEE, 0xB1}}, {{0xFB, 0xCE}, {0xEE, 0xB2}},
	{{0xFB, 0xCF}, {0xEE, 0xB3}}, {{0xFB, 0xD0}, {0xEE, 0xB4}},
	{{0xFB, 0xD1}, {0xEE, 0xB5}}, {{0xFB, 0xD2}, {0xEE, 0xB6}},
	{{0xFB, 0xD3}, {0xEE, 0xB7}}, {{0xFB, 0xD4}, {0xEE, 0xB8}},
	{{0xFB, 0xD5}, {0xEE, 0xB9}}, {{0xFB, 0xD6}, {0xEE, 0xBA}},
	{{0xFB, 0xD7}, {0xEE, 0xBB}}, {{0xFB, 0xD8}, {0xEE, 0xBC}},
	{{0xFB, 0xD9}, {0xEE, 0xBD}}, {{0xFB, 0xDA}, {0xEE, 0xBE}},
	{{0xFB, 0xDB}, {0xEE, 0xBF}}, {{0xFB, 0xDC}, {0xEE, 0xC0}},
	{{0xFB, 0xDD}, {0xEE, 0xC1}}, {{0xFB, 0xDE}, {0xEE, 0xC2}},
	{{0xFB, 0xDF}, {0xEE, 0xC3}}, {{0xFB, 0xE0}, {0xEE, 0xC4}},
	{{0xFB, 0xE1}, {0xEE, 0xC5}}, {{0xFB, 0xE2}, {0xEE, 0xC6}},
	{{0xFB, 0xE3}, {0xEE, 0xC7}}, {{0xFB, 0xE4}, {0xEE, 0xC8}},
	{{0xFB, 0xE5}, {0xEE, 0xC9}}, {{0xFB, 0xE6}, {0xEE, 0xCA}},
	{{0xFB, 0xE7}, {0xEE, 0xCB}}, {{0xFB, 0xE8}, {0xEE, 0xCC}},
	{{0xFB, 0xE9}, {0xEE, 0xCD}}, {{0xFB, 0xEA}, {0xEE, 0xCE}},
	{{0xFB, 0xEB}, {0xEE, 0xCF}}, {{0xFB, 0xEC}, {0xEE, 0xD0}},
	{{0xFB, 0xED}, {0xEE, 0xD1}}, {{0xFB, 0xEE}, {0xEE, 0xD2}},
	{{0xFB, 0xEF}, {0xEE, 0xD3}}, {{0xFB, 0xF0}, {0xEE, 0xD4}},
	{{0xFB, 0xF1}, {0xEE, 0xD5}}, {{0xFB, 0xF2}, {0xEE, 0xD6}},
	{{0xFB, 0xF3}, {0xEE, 0xD7}}, {{0xFB, 0xF4}, {0xEE, 0xD8}},
	{{0xFB, 0xF5}, {0xEE, 0xD9}}, {{0xFB, 0xF6}, {0xEE, 0xDA}},
	{{0xFB, 0xF7}, {0xEE, 0xDB}}, {{0xFB, 0xF8}, {0xEE, 0xDC}},
	{{0xFB, 0xF9}, {0xEE, 0xDD}}, {{0xFB, 0xFA}, {0xEE, 0xDE}},
	{{0xFB, 0xFB}, {0xEE, 0xDF}}, {{0xFB, 0xFC}, {0xEE, 0xE0}},
	{{0xFC, 0x40}, {0xEE, 0xE1}}, {{0xFC, 0x41}, {0xEE, 0xE2}},
	{{0xFC, 0x42}, {0xEE, 0xE3}}, {{0xFC, 0x43}, {0xEE, 0xE4}},
	{{0xFC, 0x44}, {0xEE, 0xE5}}, {{0xFC, 0x45}, {0xEE, 0xE6}},
	{{0xFC, 0x46}, {0xEE, 0xE7}}, {{0xFC, 0x47}, {0xEE, 0xE8}},
	{{0xFC, 0x48}, {0xEE, 0xE9}}, {{0xFC, 0x49}, {0xEE, 0xEA}},
	{{0xFC, 0x4A}, {0xEE, 0xEB}}, {{0xFC, 0x4B}, {0xEE, 0xEC}},
	{{0x00, 0x00}, {0x00, 0x00}}
};

static struct {
	char_u		sjis[2];
	char_u		ss3[3];
} ss3conv[] = {
	{{0xFA, 0x40}, {0x8F, 0xF3, 0xF3}}, {{0xFA, 0x41}, {0x8F, 0xF3, 0xF4}},
	{{0xFA, 0x42}, {0x8F, 0xF3, 0xF5}}, {{0xFA, 0x43}, {0x8F, 0xF3, 0xF6}},
	{{0xFA, 0x44}, {0x8F, 0xF3, 0xF7}}, {{0xFA, 0x45}, {0x8F, 0xF3, 0xF8}},
	{{0xFA, 0x46}, {0x8F, 0xF3, 0xF9}}, {{0xFA, 0x47}, {0x8F, 0xF3, 0xFA}},
	{{0xFA, 0x48}, {0x8F, 0xF3, 0xFB}}, {{0xFA, 0x49}, {0x8F, 0xF3, 0xFC}},
	{{0xFA, 0x4A}, {0x8F, 0xF3, 0xFD}}, {{0xFA, 0x4B}, {0x8F, 0xF3, 0xFE}},
	{{0xFA, 0x4C}, {0x8F, 0xF4, 0xA1}}, {{0xFA, 0x4D}, {0x8F, 0xF4, 0xA2}},
	{{0xFA, 0x4E}, {0x8F, 0xF4, 0xA3}}, {{0xFA, 0x4F}, {0x8F, 0xF4, 0xA4}},
	{{0xFA, 0x50}, {0x8F, 0xF4, 0xA5}}, {{0xFA, 0x51}, {0x8F, 0xF4, 0xA6}},
	{{0xFA, 0x52}, {0x8F, 0xF4, 0xA7}}, {{0xFA, 0x53}, {0x8F, 0xF4, 0xA8}},
	{{0xFA, 0x54}, {0x00, 0xA2, 0xCC}}, {{0xFA, 0x55}, {0x8F, 0xA2, 0xC3}},
	{{0xFA, 0x56}, {0x8F, 0xF4, 0xA9}}, {{0xFA, 0x57}, {0x8F, 0xF4, 0xAA}},
	{{0xFA, 0x58}, {0x8F, 0xF4, 0xAB}}, {{0xFA, 0x59}, {0x8F, 0xA2, 0xF1}},
	{{0xFA, 0x59}, {0x8F, 0xF4, 0xAC}}, {{0xFA, 0x5A}, {0x8F, 0xF4, 0xAD}},
	{{0xFA, 0x5B}, {0x00, 0xA2, 0xE8}}, {{0xFA, 0x5C}, {0x8F, 0xD4, 0xE3}},
	{{0xFA, 0x5D}, {0x8F, 0xDC, 0xDF}}, {{0xFA, 0x5E}, {0x8F, 0xE4, 0xE9}},
	{{0xFA, 0x5F}, {0x8F, 0xE3, 0xF8}}, {{0xFA, 0x60}, {0x8F, 0xD9, 0xA1}},
	{{0xFA, 0x61}, {0x8F, 0xB1, 0xBB}}, {{0xFA, 0x62}, {0x8F, 0xF4, 0xAE}},
	{{0xFA, 0x63}, {0x8F, 0xC2, 0xAD}}, {{0xFA, 0x64}, {0x8F, 0xC3, 0xFC}},
	{{0xFA, 0x65}, {0x8F, 0xE4, 0xD0}}, {{0xFA, 0x66}, {0x8F, 0xC2, 0xBF}},
	{{0xFA, 0x67}, {0x8F, 0xBC, 0xF4}}, {{0xFA, 0x68}, {0x8F, 0xB0, 0xA9}},
	{{0xFA, 0x69}, {0x8F, 0xB0, 0xC8}}, {{0xFA, 0x6A}, {0x8F, 0xF4, 0xAF}},
	{{0xFA, 0x6B}, {0x8F, 0xB0, 0xD2}}, {{0xFA, 0x6C}, {0x8F, 0xB0, 0xD4}},
	{{0xFA, 0x6D}, {0x8F, 0xB0, 0xE3}}, {{0xFA, 0x6E}, {0x8F, 0xB0, 0xEE}},
	{{0xFA, 0x6F}, {0x8F, 0xB1, 0xA7}}, {{0xFA, 0x70}, {0x8F, 0xB1, 0xA3}},
	{{0xFA, 0x71}, {0x8F, 0xB1, 0xAC}}, {{0xFA, 0x72}, {0x8F, 0xB1, 0xA9}},
	{{0xFA, 0x73}, {0x8F, 0xB1, 0xBE}}, {{0xFA, 0x74}, {0x8F, 0xB1, 0xDF}},
	{{0xFA, 0x75}, {0x8F, 0xB1, 0xD8}}, {{0xFA, 0x76}, {0x8F, 0xB1, 0xC8}},
	{{0xFA, 0x77}, {0x8F, 0xB1, 0xD7}}, {{0xFA, 0x78}, {0x8F, 0xB1, 0xE3}},
	{{0xFA, 0x79}, {0x8F, 0xB1, 0xF4}}, {{0xFA, 0x7A}, {0x8F, 0xB1, 0xE1}},
	{{0xFA, 0x7B}, {0x8F, 0xB2, 0xA3}}, {{0xFA, 0x7C}, {0x8F, 0xF4, 0xB0}},
	{{0xFA, 0x7D}, {0x8F, 0xB2, 0xBB}}, {{0xFA, 0x7E}, {0x8F, 0xB2, 0xE6}},
	{{0xFA, 0x80}, {0x8F, 0xB2, 0xED}}, {{0xFA, 0x81}, {0x8F, 0xB2, 0xF5}},
	{{0xFA, 0x82}, {0x8F, 0xB2, 0xFC}}, {{0xFA, 0x83}, {0x8F, 0xF4, 0xB1}},
	{{0xFA, 0x84}, {0x8F, 0xB3, 0xB5}}, {{0xFA, 0x85}, {0x8F, 0xB3, 0xD8}},
	{{0xFA, 0x86}, {0x8F, 0xB3, 0xDB}}, {{0xFA, 0x87}, {0x8F, 0xB3, 0xE5}},
	{{0xFA, 0x88}, {0x8F, 0xB3, 0xEE}}, {{0xFA, 0x89}, {0x8F, 0xB3, 0xFB}},
	{{0xFA, 0x8A}, {0x8F, 0xF4, 0xB2}}, {{0xFA, 0x8B}, {0x8F, 0xF4, 0xB3}},
	{{0xFA, 0x8C}, {0x8F, 0xB4, 0xC0}}, {{0xFA, 0x8D}, {0x8F, 0xB4, 0xC7}},
	{{0xFA, 0x8E}, {0x8F, 0xB4, 0xD0}}, {{0xFA, 0x8F}, {0x8F, 0xB4, 0xDE}},
	{{0xFA, 0x90}, {0x8F, 0xF4, 0xB4}}, {{0xFA, 0x91}, {0x8F, 0xB5, 0xAA}},
	{{0xFA, 0x92}, {0x8F, 0xF4, 0xB5}}, {{0xFA, 0x93}, {0x8F, 0xB5, 0xAF}},
	{{0xFA, 0x94}, {0x8F, 0xB5, 0xC4}}, {{0xFA, 0x95}, {0x8F, 0xB5, 0xE8}},
	{{0xFA, 0x96}, {0x8F, 0xF4, 0xB6}}, {{0xFA, 0x97}, {0x8F, 0xB7, 0xC2}},
	{{0xFA, 0x98}, {0x8F, 0xB7, 0xE4}}, {{0xFA, 0x99}, {0x8F, 0xB7, 0xE8}},
	{{0xFA, 0x9A}, {0x8F, 0xB7, 0xE7}}, {{0xFA, 0x9B}, {0x8F, 0xF4, 0xB7}},
	{{0xFA, 0x9C}, {0x8F, 0xF4, 0xB8}}, {{0xFA, 0x9D}, {0x8F, 0xF4, 0xB9}},
	{{0xFA, 0x9E}, {0x8F, 0xB8, 0xCE}}, {{0xFA, 0x9F}, {0x8F, 0xB8, 0xE1}},
	{{0xFA, 0xA0}, {0x8F, 0xB8, 0xF5}}, {{0xFA, 0xA1}, {0x8F, 0xB8, 0xF7}},
	{{0xFA, 0xA2}, {0x8F, 0xB8, 0xF8}}, {{0xFA, 0xA3}, {0x8F, 0xB8, 0xFC}},
	{{0xFA, 0xA4}, {0x8F, 0xB9, 0xAF}}, {{0xFA, 0xA5}, {0x8F, 0xB9, 0xB7}},
	{{0xFA, 0xA6}, {0x8F, 0xBA, 0xBE}}, {{0xFA, 0xA7}, {0x8F, 0xBA, 0xDB}},
	{{0xFA, 0xA8}, {0x8F, 0xCD, 0xAA}}, {{0xFA, 0xA9}, {0x8F, 0xBA, 0xE1}},
	{{0xFA, 0xAA}, {0x8F, 0xF4, 0xBA}}, {{0xFA, 0xAB}, {0x8F, 0xBA, 0xEB}},
	{{0xFA, 0xAC}, {0x8F, 0xBB, 0xB3}}, {{0xFA, 0xAD}, {0x8F, 0xBB, 0xB8}},
	{{0xFA, 0xAE}, {0x8F, 0xF4, 0xBB}}, {{0xFA, 0xAF}, {0x8F, 0xBB, 0xCA}},
	{{0xFA, 0xB0}, {0x8F, 0xF4, 0xBC}}, {{0xFA, 0xB1}, {0x8F, 0xF4, 0xBD}},
	{{0xFA, 0xB2}, {0x8F, 0xBB, 0xD0}}, {{0xFA, 0xB3}, {0x8F, 0xBB, 0xDE}},
	{{0xFA, 0xB4}, {0x8F, 0xBB, 0xF4}}, {{0xFA, 0xB5}, {0x8F, 0xBB, 0xF5}},
	{{0xFA, 0xB6}, {0x8F, 0xBB, 0xF9}}, {{0xFA, 0xB7}, {0x8F, 0xBC, 0xE4}},
	{{0xFA, 0xB8}, {0x8F, 0xBC, 0xED}}, {{0xFA, 0xB9}, {0x8F, 0xBC, 0xFE}},
	{{0xFA, 0xBA}, {0x8F, 0xF4, 0xBE}}, {{0xFA, 0xBB}, {0x8F, 0xBD, 0xC2}},
	{{0xFA, 0xBC}, {0x8F, 0xBD, 0xE7}}, {{0xFA, 0xBD}, {0x8F, 0xF4, 0xBF}},
	{{0xFA, 0xBE}, {0x8F, 0xBD, 0xF0}}, {{0xFA, 0xBF}, {0x8F, 0xBE, 0xB0}},
	{{0xFA, 0xC0}, {0x8F, 0xBE, 0xAC}}, {{0xFA, 0xC1}, {0x8F, 0xF4, 0xC0}},
	{{0xFA, 0xC2}, {0x8F, 0xBE, 0xB3}}, {{0xFA, 0xC3}, {0x8F, 0xBE, 0xBD}},
	{{0xFA, 0xC4}, {0x8F, 0xBE, 0xCD}}, {{0xFA, 0xC5}, {0x8F, 0xBE, 0xC9}},
	{{0xFA, 0xC6}, {0x8F, 0xBE, 0xE4}}, {{0xFA, 0xC7}, {0x8F, 0xBF, 0xA8}},
	{{0xFA, 0xC8}, {0x8F, 0xBF, 0xC9}}, {{0xFA, 0xC9}, {0x8F, 0xC0, 0xC4}},
	{{0xFA, 0xCA}, {0x8F, 0xC0, 0xE4}}, {{0xFA, 0xCB}, {0x8F, 0xC0, 0xF4}},
	{{0xFA, 0xCC}, {0x8F, 0xC1, 0xA6}}, {{0xFA, 0xCD}, {0x8F, 0xF4, 0xC1}},
	{{0xFA, 0xCE}, {0x8F, 0xC1, 0xF5}}, {{0xFA, 0xCF}, {0x8F, 0xC1, 0xFC}},
	{{0xFA, 0xD0}, {0x8F, 0xF4, 0xC2}}, {{0xFA, 0xD1}, {0x8F, 0xC1, 0xF8}},
	{{0xFA, 0xD2}, {0x8F, 0xC2, 0xAB}}, {{0xFA, 0xD3}, {0x8F, 0xC2, 0xA1}},
	{{0xFA, 0xD4}, {0x8F, 0xC2, 0xA5}}, {{0xFA, 0xD5}, {0x8F, 0xF4, 0xC3}},
	{{0xFA, 0xD6}, {0x8F, 0xC2, 0xB8}}, {{0xFA, 0xD7}, {0x8F, 0xC2, 0xBA}},
	{{0xFA, 0xD8}, {0x8F, 0xF4, 0xC4}}, {{0xFA, 0xD9}, {0x8F, 0xC2, 0xC4}},
	{{0xFA, 0xDA}, {0x8F, 0xC2, 0xD2}}, {{0xFA, 0xDB}, {0x8F, 0xC2, 0xD7}},
	{{0xFA, 0xDC}, {0x8F, 0xC2, 0xDB}}, {{0xFA, 0xDD}, {0x8F, 0xC2, 0xDE}},
	{{0xFA, 0xDE}, {0x8F, 0xC2, 0xED}}, {{0xFA, 0xDF}, {0x8F, 0xC2, 0xF0}},
	{{0xFA, 0xE0}, {0x8F, 0xF4, 0xC5}}, {{0xFA, 0xE1}, {0x8F, 0xC3, 0xA1}},
	{{0xFA, 0xE2}, {0x8F, 0xC3, 0xB5}}, {{0xFA, 0xE3}, {0x8F, 0xC3, 0xC9}},
	{{0xFA, 0xE4}, {0x8F, 0xC3, 0xB9}}, {{0xFA, 0xE5}, {0x8F, 0xF4, 0xC6}},
	{{0xFA, 0xE6}, {0x8F, 0xC3, 0xD8}}, {{0xFA, 0xE7}, {0x8F, 0xC3, 0xFE}},
	{{0xFA, 0xE8}, {0x8F, 0xF4, 0xC7}}, {{0xFA, 0xE9}, {0x8F, 0xC4, 0xCC}},
	{{0xFA, 0xEA}, {0x8F, 0xF4, 0xC8}}, {{0xFA, 0xEB}, {0x8F, 0xC4, 0xD9}},
	{{0xFA, 0xEC}, {0x8F, 0xC4, 0xEA}}, {{0xFA, 0xED}, {0x8F, 0xC4, 0xFD}},
	{{0xFA, 0xEE}, {0x8F, 0xF4, 0xC9}}, {{0xFA, 0xEF}, {0x8F, 0xC5, 0xA7}},
	{{0xFA, 0xF0}, {0x8F, 0xC5, 0xB5}}, {{0xFA, 0xF1}, {0x8F, 0xC5, 0xB6}},
	{{0xFA, 0xF2}, {0x8F, 0xF4, 0xCA}}, {{0xFA, 0xF3}, {0x8F, 0xC5, 0xD5}},
	{{0xFA, 0xF4}, {0x8F, 0xC6, 0xB8}}, {{0xFA, 0xF5}, {0x8F, 0xC6, 0xD7}},
	{{0xFA, 0xF6}, {0x8F, 0xC6, 0xE0}}, {{0xFA, 0xF7}, {0x8F, 0xC6, 0xEA}},
	{{0xFA, 0xF8}, {0x8F, 0xC6, 0xE3}}, {{0xFA, 0xF9}, {0x8F, 0xC7, 0xA1}},
	{{0xFA, 0xFA}, {0x8F, 0xC7, 0xAB}}, {{0xFA, 0xFB}, {0x8F, 0xC7, 0xC7}},
	{{0xFA, 0xFC}, {0x8F, 0xC7, 0xC3}}, {{0xFB, 0x40}, {0x8F, 0xC7, 0xCB}},
	{{0xFB, 0x41}, {0x8F, 0xC7, 0xCF}}, {{0xFB, 0x42}, {0x8F, 0xC7, 0xD9}},
	{{0xFB, 0x43}, {0x8F, 0xF4, 0xCB}}, {{0xFB, 0x44}, {0x8F, 0xF4, 0xCC}},
	{{0xFB, 0x45}, {0x8F, 0xC7, 0xE6}}, {{0xFB, 0x46}, {0x8F, 0xC7, 0xEE}},
	{{0xFB, 0x47}, {0x8F, 0xC7, 0xFC}}, {{0xFB, 0x48}, {0x8F, 0xC7, 0xEB}},
	{{0xFB, 0x49}, {0x8F, 0xC7, 0xF0}}, {{0xFB, 0x4A}, {0x8F, 0xC8, 0xB1}},
	{{0xFB, 0x4B}, {0x8F, 0xC8, 0xE5}}, {{0xFB, 0x4C}, {0x8F, 0xC8, 0xF8}},
	{{0xFB, 0x4D}, {0x8F, 0xC9, 0xA6}}, {{0xFB, 0x4E}, {0x8F, 0xC9, 0xAB}},
	{{0xFB, 0x4F}, {0x8F, 0xC9, 0xAD}}, {{0xFB, 0x50}, {0x8F, 0xF4, 0xCD}},
	{{0xFB, 0x51}, {0x8F, 0xC9, 0xCA}}, {{0xFB, 0x52}, {0x8F, 0xC9, 0xD3}},
	{{0xFB, 0x53}, {0x8F, 0xC9, 0xE9}}, {{0xFB, 0x54}, {0x8F, 0xC9, 0xE3}},
	{{0xFB, 0x55}, {0x8F, 0xC9, 0xFC}}, {{0xFB, 0x56}, {0x8F, 0xC9, 0xF4}},
	{{0xFB, 0x57}, {0x8F, 0xC9, 0xF5}}, {{0xFB, 0x58}, {0x8F, 0xF4, 0xCE}},
	{{0xFB, 0x59}, {0x8F, 0xCA, 0xB3}}, {{0xFB, 0x5A}, {0x8F, 0xCA, 0xBD}},
	{{0xFB, 0x5B}, {0x8F, 0xCA, 0xEF}}, {{0xFB, 0x5C}, {0x8F, 0xCA, 0xF1}},
	{{0xFB, 0x5D}, {0x8F, 0xCB, 0xAE}}, {{0xFB, 0x5E}, {0x8F, 0xF4, 0xCF}},
	{{0xFB, 0x5F}, {0x8F, 0xCB, 0xCA}}, {{0xFB, 0x60}, {0x8F, 0xCB, 0xE6}},
	{{0xFB, 0x61}, {0x8F, 0xCB, 0xEA}}, {{0xFB, 0x62}, {0x8F, 0xCB, 0xF0}},
	{{0xFB, 0x63}, {0x8F, 0xCB, 0xF4}}, {{0xFB, 0x64}, {0x8F, 0xCB, 0xEE}},
	{{0xFB, 0x65}, {0x8F, 0xCC, 0xA5}}, {{0xFB, 0x66}, {0x8F, 0xCB, 0xF9}},
	{{0xFB, 0x67}, {0x8F, 0xCC, 0xAB}}, {{0xFB, 0x68}, {0x8F, 0xCC, 0xAE}},
	{{0xFB, 0x69}, {0x8F, 0xCC, 0xAD}}, {{0xFB, 0x6A}, {0x8F, 0xCC, 0xB2}},
	{{0xFB, 0x6B}, {0x8F, 0xCC, 0xC2}}, {{0xFB, 0x6C}, {0x8F, 0xCC, 0xD0}},
	{{0xFB, 0x6D}, {0x8F, 0xCC, 0xD9}}, {{0xFB, 0x6E}, {0x8F, 0xF4, 0xD0}},
	{{0xFB, 0x6F}, {0x8F, 0xCD, 0xBB}}, {{0xFB, 0x70}, {0x8F, 0xF4, 0xD1}},
	{{0xFB, 0x71}, {0x8F, 0xCE, 0xBB}}, {{0xFB, 0x72}, {0x8F, 0xF4, 0xD2}},
	{{0xFB, 0x73}, {0x8F, 0xCE, 0xBA}}, {{0xFB, 0x74}, {0x8F, 0xCE, 0xC3}},
	{{0xFB, 0x75}, {0x8F, 0xF4, 0xD3}}, {{0xFB, 0x76}, {0x8F, 0xCE, 0xF2}},
	{{0xFB, 0x77}, {0x8F, 0xB3, 0xDD}}, {{0xFB, 0x78}, {0x8F, 0xCF, 0xD5}},
	{{0xFB, 0x79}, {0x8F, 0xCF, 0xE2}}, {{0xFB, 0x7A}, {0x8F, 0xCF, 0xE9}},
	{{0xFB, 0x7B}, {0x8F, 0xCF, 0xED}}, {{0xFB, 0x7C}, {0x8F, 0xF4, 0xD4}},
	{{0xFB, 0x7D}, {0x8F, 0xF4, 0xD5}}, {{0xFB, 0x7E}, {0x8F, 0xF4, 0xD6}},
	{{0xFB, 0x80}, {0x8F, 0xF4, 0xD7}}, {{0xFB, 0x81}, {0x8F, 0xD0, 0xE5}},
	{{0xFB, 0x82}, {0x8F, 0xF4, 0xD8}}, {{0xFB, 0x83}, {0x8F, 0xD0, 0xE9}},
	{{0xFB, 0x84}, {0x8F, 0xD1, 0xE8}}, {{0xFB, 0x85}, {0x8F, 0xF4, 0xD9}},
	{{0xFB, 0x86}, {0x8F, 0xF4, 0xDA}}, {{0xFB, 0x87}, {0x8F, 0xD1, 0xEC}},
	{{0xFB, 0x88}, {0x8F, 0xD2, 0xBB}}, {{0xFB, 0x89}, {0x8F, 0xF4, 0xDB}},
	{{0xFB, 0x8A}, {0x8F, 0xD3, 0xE1}}, {{0xFB, 0x8B}, {0x8F, 0xD3, 0xE8}},
	{{0xFB, 0x8C}, {0x8F, 0xD4, 0xA7}}, {{0xFB, 0x8D}, {0x8F, 0xF4, 0xDC}},
	{{0xFB, 0x8E}, {0x8F, 0xF4, 0xDD}}, {{0xFB, 0x8F}, {0x8F, 0xD4, 0xD4}},
	{{0xFB, 0x90}, {0x8F, 0xD4, 0xF2}}, {{0xFB, 0x91}, {0x8F, 0xD5, 0xAE}},
	{{0xFB, 0x92}, {0x8F, 0xF4, 0xDE}}, {{0xFB, 0x93}, {0x8F, 0xD7, 0xDE}},
	{{0xFB, 0x94}, {0x8F, 0xF4, 0xDF}}, {{0xFB, 0x95}, {0x8F, 0xD8, 0xA2}},
	{{0xFB, 0x96}, {0x8F, 0xD8, 0xB7}}, {{0xFB, 0x97}, {0x8F, 0xD8, 0xC1}},
	{{0xFB, 0x98}, {0x8F, 0xD8, 0xD1}}, {{0xFB, 0x99}, {0x8F, 0xD8, 0xF4}},
	{{0xFB, 0x9A}, {0x8F, 0xD9, 0xC6}}, {{0xFB, 0x9B}, {0x8F, 0xD9, 0xC8}},
	{{0xFB, 0x9C}, {0x8F, 0xD9, 0xD1}}, {{0xFB, 0x9D}, {0x8F, 0xF4, 0xE0}},
	{{0xFB, 0x9E}, {0x8F, 0xF4, 0xE1}}, {{0xFB, 0x9F}, {0x8F, 0xF4, 0xE2}},
	{{0xFB, 0xA0}, {0x8F, 0xF4, 0xE3}}, {{0xFB, 0xA1}, {0x8F, 0xF4, 0xE4}},
	{{0xFB, 0xA2}, {0x8F, 0xDC, 0xD3}}, {{0xFB, 0xA3}, {0x8F, 0xDD, 0xC8}},
	{{0xFB, 0xA4}, {0x8F, 0xDD, 0xD4}}, {{0xFB, 0xA5}, {0x8F, 0xDD, 0xEA}},
	{{0xFB, 0xA6}, {0x8F, 0xDD, 0xFA}}, {{0xFB, 0xA7}, {0x8F, 0xDE, 0xA4}},
	{{0xFB, 0xA8}, {0x8F, 0xDE, 0xB0}}, {{0xFB, 0xA9}, {0x8F, 0xF4, 0xE5}},
	{{0xFB, 0xAA}, {0x8F, 0xDE, 0xB5}}, {{0xFB, 0xAB}, {0x8F, 0xDE, 0xCB}},
	{{0xFB, 0xAC}, {0x8F, 0xF4, 0xE6}}, {{0xFB, 0xAD}, {0x8F, 0xDF, 0xB9}},
	{{0xFB, 0xAE}, {0x8F, 0xF4, 0xE7}}, {{0xFB, 0xAF}, {0x8F, 0xDF, 0xC3}},
	{{0xFB, 0xB0}, {0x8F, 0xF4, 0xE8}}, {{0xFB, 0xB1}, {0x8F, 0xF4, 0xE9}},
	{{0xFB, 0xB2}, {0x8F, 0xE0, 0xD9}}, {{0xFB, 0xB3}, {0x8F, 0xF4, 0xEA}},
	{{0xFB, 0xB4}, {0x8F, 0xF4, 0xEB}}, {{0xFB, 0xB5}, {0x8F, 0xE1, 0xE2}},
	{{0xFB, 0xB6}, {0x8F, 0xF4, 0xEC}}, {{0xFB, 0xB7}, {0x8F, 0xF4, 0xED}},
	{{0xFB, 0xB8}, {0x8F, 0xF4, 0xEE}}, {{0xFB, 0xB9}, {0x8F, 0xE2, 0xC7}},
	{{0xFB, 0xBA}, {0x8F, 0xE3, 0xA8}}, {{0xFB, 0xBB}, {0x8F, 0xE3, 0xA6}},
	{{0xFB, 0xBC}, {0x8F, 0xE3, 0xA9}}, {{0xFB, 0xBD}, {0x8F, 0xE3, 0xAF}},
	{{0xFB, 0xBE}, {0x8F, 0xE3, 0xB0}}, {{0xFB, 0xBF}, {0x8F, 0xE3, 0xAA}},
	{{0xFB, 0xC0}, {0x8F, 0xE3, 0xAB}}, {{0xFB, 0xC1}, {0x8F, 0xE3, 0xBC}},
	{{0xFB, 0xC2}, {0x8F, 0xE3, 0xC1}}, {{0xFB, 0xC3}, {0x8F, 0xE3, 0xBF}},
	{{0xFB, 0xC4}, {0x8F, 0xE3, 0xD5}}, {{0xFB, 0xC5}, {0x8F, 0xE3, 0xD8}},
	{{0xFB, 0xC6}, {0x8F, 0xE3, 0xD6}}, {{0xFB, 0xC7}, {0x8F, 0xE3, 0xDF}},
	{{0xFB, 0xC8}, {0x8F, 0xE3, 0xE3}}, {{0xFB, 0xC9}, {0x8F, 0xE3, 0xE1}},
	{{0xFB, 0xCA}, {0x8F, 0xE3, 0xD4}}, {{0xFB, 0xCB}, {0x8F, 0xE3, 0xE9}},
	{{0xFB, 0xCC}, {0x8F, 0xE4, 0xA6}}, {{0xFB, 0xCD}, {0x8F, 0xE3, 0xF1}},
	{{0xFB, 0xCE}, {0x8F, 0xE3, 0xF2}}, {{0xFB, 0xCF}, {0x8F, 0xE4, 0xCB}},
	{{0xFB, 0xD0}, {0x8F, 0xE4, 0xC1}}, {{0xFB, 0xD1}, {0x8F, 0xE4, 0xC3}},
	{{0xFB, 0xD2}, {0x8F, 0xE4, 0xBE}}, {{0xFB, 0xD3}, {0x8F, 0xF4, 0xEF}},
	{{0xFB, 0xD4}, {0x8F, 0xE4, 0xC0}}, {{0xFB, 0xD5}, {0x8F, 0xE4, 0xC7}},
	{{0xFB, 0xD6}, {0x8F, 0xE4, 0xBF}}, {{0xFB, 0xD7}, {0x8F, 0xE4, 0xE0}},
	{{0xFB, 0xD8}, {0x8F, 0xE4, 0xDE}}, {{0xFB, 0xD9}, {0x8F, 0xE4, 0xD1}},
	{{0xFB, 0xDA}, {0x8F, 0xF4, 0xF0}}, {{0xFB, 0xDB}, {0x8F, 0xE4, 0xDC}},
	{{0xFB, 0xDC}, {0x8F, 0xE4, 0xD2}}, {{0xFB, 0xDD}, {0x8F, 0xE4, 0xDB}},
	{{0xFB, 0xDE}, {0x8F, 0xE4, 0xD4}}, {{0xFB, 0xDF}, {0x8F, 0xE4, 0xFA}},
	{{0xFB, 0xE0}, {0x8F, 0xE4, 0xEF}}, {{0xFB, 0xE1}, {0x8F, 0xE5, 0xB3}},
	{{0xFB, 0xE2}, {0x8F, 0xE5, 0xBF}}, {{0xFB, 0xE3}, {0x8F, 0xE5, 0xC9}},
	{{0xFB, 0xE4}, {0x8F, 0xE5, 0xD0}}, {{0xFB, 0xE5}, {0x8F, 0xE5, 0xE2}},
	{{0xFB, 0xE6}, {0x8F, 0xE5, 0xEA}}, {{0xFB, 0xE7}, {0x8F, 0xE5, 0xEB}},
	{{0xFB, 0xE8}, {0x8F, 0xF4, 0xF1}}, {{0xFB, 0xE9}, {0x8F, 0xF4, 0xF2}},
	{{0xFB, 0xEA}, {0x8F, 0xF4, 0xF3}}, {{0xFB, 0xEB}, {0x8F, 0xE6, 0xE8}},
	{{0xFB, 0xEC}, {0x8F, 0xE6, 0xEF}}, {{0xFB, 0xED}, {0x8F, 0xE7, 0xAC}},
	{{0xFB, 0xEE}, {0x8F, 0xF4, 0xF4}}, {{0xFB, 0xEF}, {0x8F, 0xE7, 0xAE}},
	{{0xFB, 0xF0}, {0x8F, 0xF4, 0xF5}}, {{0xFB, 0xF1}, {0x8F, 0xE7, 0xB1}},
	{{0xFB, 0xF2}, {0x8F, 0xF4, 0xF6}}, {{0xFB, 0xF3}, {0x8F, 0xE7, 0xB2}},
	{{0xFB, 0xF4}, {0x8F, 0xE8, 0xB1}}, {{0xFB, 0xF5}, {0x8F, 0xE8, 0xB6}},
	{{0xFB, 0xF6}, {0x8F, 0xF4, 0xF7}}, {{0xFB, 0xF7}, {0x8F, 0xF4, 0xF8}},
	{{0xFB, 0xF8}, {0x8F, 0xE8, 0xDD}}, {{0xFB, 0xF9}, {0x8F, 0xF4, 0xF9}},
	{{0xFB, 0xFA}, {0x8F, 0xF4, 0xFA}}, {{0xFB, 0xFB}, {0x8F, 0xE9, 0xD1}},
	{{0xFB, 0xFC}, {0x8F, 0xF4, 0xFB}}, {{0xFC, 0x40}, {0x8F, 0xE9, 0xED}},
	{{0xFC, 0x41}, {0x8F, 0xEA, 0xCD}}, {{0xFC, 0x42}, {0x8F, 0xF4, 0xFC}},
	{{0xFC, 0x43}, {0x8F, 0xEA, 0xDB}}, {{0xFC, 0x44}, {0x8F, 0xEA, 0xE6}},
	{{0xFC, 0x45}, {0x8F, 0xEA, 0xEA}}, {{0xFC, 0x46}, {0x8F, 0xEB, 0xA5}},
	{{0xFC, 0x47}, {0x8F, 0xEB, 0xFB}}, {{0xFC, 0x48}, {0x8F, 0xEB, 0xFA}},
	{{0xFC, 0x49}, {0x8F, 0xF4, 0xFD}}, {{0xFC, 0x4A}, {0x8F, 0xEC, 0xD6}},
	{{0xFC, 0x4B}, {0x8F, 0xF4, 0xFE}}, {{0x00, 0x00}, {0x00, 0x00, 0x00}},
};

/*
 * Byte classification of the internal representation, which is UTF-8.
 *
 * These keep the names and the meaning the Shift-JIS version had, so the call
 * sites all over the editor still read correctly:
 *
 *   ISkanji(c)  the byte starts a multi-byte character
 *   ISkana(c)   the byte is a one-byte kana - impossible in UTF-8, see below
 *   ISdisp(c)   the byte is part of a multi-byte character (lead or trailing)
 *
 * What did change is that a multi-byte character is no longer always two
 * bytes. Use utf_len()/utf_lenat() for the length and utf_width() for the
 * number of columns; never assume 2.
 */
	int
ISkanji(int code)
{
	if (code >= 0x100)
		return 0;
	return UTF8_ISLEAD(code) ? 1 : 0;
}

/*
 * Halfwidth kana used to be a single byte in Shift-JIS (0xa1-0xdf). In UTF-8
 * they are ordinary multi-byte characters (U+FF61..U+FF9F), so no byte is a
 * one-byte kana any more and this is always false. Callers that really want to
 * know whether a character is kana use utf_iskana().
 */
	int
ISkana(int code)
{
	return 0;
}

	int
ISdisp(int code)
{
	if (code >= 0x100)
		return 0;
	return (UTF8_ISLEAD(code) || UTF8_ISTAIL(code)) ? 1 : 0;
}

/* input pos : 1..strlen
   return  0 : single byte character
	   1 : first byte of a multi-byte character
	   2 : trailing byte of a multi-byte character
*/
	int
ISkanjiPosition(char_u *ptr, int pos)
{
	char_u	*p;
	char_u	*head;

	if (ptr == NULL || pos <= 0)
		return 0;
	/* pos is 1 based, and must not run past the end of the string. */
	for (p = ptr; pos > 1; pos--, p++)
		if (*p == NUL)
			return 0;
	if (*p == NUL)
		return 0;
	if (UTF8_ISLEAD(*p))
		return 1;
	if (!UTF8_ISTAIL(*p))
		return 0;
	/*
	 * A trailing byte only counts as one if it really belongs to a character
	 * that starts earlier; a stray 0x80 byte is a single byte of its own.
	 */
	head = utf_head(ptr, p);
	if (head == p || !UTF8_ISLEAD(*head))
		return 0;
	if ((int)(p - head) >= utf_len(*head))
		return 0;
	return 2;
}

	int
ISkanjiPointer(char_u *ptr, char_u *p)
{
	return(ISkanjiPosition(ptr, p - ptr + 1));
}

	int
ISkanjiCol(linenr_t lnum, colnr_t col)
{
	return(ISkanjiPosition(ml_get_buf(curbuf, lnum, FALSE), col + 1));
}


/*
 * Bytes in the character at 'col' of line 'lnum'; the counterpart of
 * ISkanjiCol() for stepping over a character.
 */
	int
kanjilenCol(linenr_t lnum, colnr_t col)
{
	return utf_lenat(ml_get_buf(curbuf, lnum, FALSE), (int)col);
}

/*
 * Snap the cursor onto the start of the character it is in. Nothing moves when
 * it is already on a boundary.
 */
	void
kanji_align(void)
{
	curwin->w_cursor.col = (colnr_t)utf_headoff(
								ml_get(curwin->w_cursor.lnum),
								(int)curwin->w_cursor.col);
}

/*
 * Move the cursor forward to the next character boundary, if it is not on one
 * already. The counterpart of kanji_align().
 */
	void
kanji_align_next(void)
{
	char_u	*base = ml_get(curwin->w_cursor.lnum);
	int		 h = utf_headoff(base, (int)curwin->w_cursor.col);

	if (h < (int)curwin->w_cursor.col)
		curwin->w_cursor.col = (colnr_t)(h + utf_lenat(base, h));
}

/*
 * Byte index of the last byte of the character that covers 'col'. Used where an
 * inclusive end of range has to cover the whole character.
 */
	int
kanji_endcol(linenr_t lnum, colnr_t col)
{
	char_u	*base = ml_get_buf(curbuf, lnum, FALSE);
	int		 h = utf_headoff(base, (int)col);

	return h + utf_lenat(base, h) - 1;
}

/*
 * Grow 'len' bytes starting at 'startcol' of 'base' so that the range ends on a
 * character boundary. Returns the adjusted length.
 */
	int
kanji_fixlen(char_u *base, int startcol, int len)
{
	int		endcol = startcol + len;		/* one past the last byte */
	int		h = utf_headoff(base, endcol);

	if (h < endcol)							/* ends inside a character */
		len += utf_lenat(base, h) - (endcol - h);
	return len;
}

	int
ISkanjiCur(void)
{
	return(ISkanjiPosition(ml_get_buf(curbuf, curwin->w_cursor.lnum, FALSE),
						curwin->w_cursor.col + 1));
}

	int
ISkanjiFpos(FPOS *po)
{
	return(ISkanjiPosition(ml_get_buf(curbuf, po->lnum, FALSE), po->col + 1));
}

	int
vcol2col(WIN *wp, linenr_t lnum, colnr_t maxcol, int *wantcol, int num, int colum)
{
	char_u	*	line;
	char_u	*	ptr;
	char_u		c;
	colnr_t		vcol;

	ptr = line = ml_get_buf(wp->w_buffer, lnum, FALSE);

	vcol = 0;
	if (num)
		vcol = 8;
	while ((c = *ptr) != NUL)
	{
		if (ISkanji(c))
		{
			int		clen = utf_lenat(ptr, 0);
			int		cwid = utf_width(ptr);

			/* A double width character does not straddle the right edge: a
			 * filler column is inserted before it. */
			if (cwid == 2 && colum && ((int)(vcol % colum) == (colum - 1)))
			{
				vcol++;
				if (vcol > maxcol)
				{
					ptr = utf_prev(line, ptr);
					break;
				}
			}
			vcol += cwid;
			if (vcol > maxcol)
				break;
			ptr += clen;
			continue;
		}
		if (colum && ptr[1] != NUL && c < ' ' && c != TAB
									&& ((int)(vcol % colum) == (colum - 1)))
		{
			vcol += chartabsize(ptr, vcol);
			ptr ++;
			continue;
		}
		vcol += chartabsize(ptr, vcol);
		if (vcol > maxcol)
			break;
		ptr ++;
	}
	if (wantcol != NULL)
	{
		if (c == NUL)
			*wantcol = ptr - line;
		else if (ISkanji(c))
			*wantcol = ptr - line + utf_lenat(ptr, 0) - 1;
		else
			*wantcol = ptr - line;
	}
	return(ptr - line);
}

/*
 *	Japanese Character;
 */
	int_u
sjistojis(char_u high, char_u low)
{
	if (IS_X0212(high))
	{
		int			i;
		char_u		ss3;
		int_u		euc;

		for (i = 0; altconv[i].sjis[0]; i++)
		{
			if (altconv[i].sjis[0] == high && altconv[i].sjis[1] == low)
			{
				high = altconv[i].alt[0];
				low  = altconv[i].alt[1];
				break;
			}
		}
		if (altconv[i].sjis[0] == 0x00)
		{
			if ((euc = sjistoeuc3(high, low, &ss3)) != 0 && ss3 == 0x00)
				return(euc & 0x7f7f);
			else
				return(0x2020);
		}
	}
	if (high <= 0x9f)
		high -= 0x71;
	else
		high -= 0xb1;
	high = high * 2 + 1;
	if (low > 0x7f)
		low--;
	if (low >= 0x9e)
	{
		low -= 0x7d;
		high++;
	}
	else
	{
		low -= 0x1f;
	}
	return(((int_u)high << 8) | (low & 0xff));
}

	static int_u
sjistoeuc(char_u high, char_u low, char_u *ss3)
{
	*ss3 = 0x00;
	if (IS_X0212(high))
		return(sjistoeuc3(high, low, ss3));
	return(sjistojis(high, low) | 0x8080);
}

	static int_u
sjistoeuc3(char_u high, char_u low, char_u *ss3)
{
	int		i;

	for (i = 0; ss3conv[i].sjis[0]; i++)
	{
		if (ss3conv[i].sjis[0] == high && ss3conv[i].sjis[1] == low)
		{
			*ss3 = ss3conv[i].ss3[0];
			high = ss3conv[i].ss3[1];
			low  = ss3conv[i].ss3[2];
			return(((int_u)high << 8) | (low & 0xff));
		}
	}

/* CAUTION! JISX0213 feature is still EXPERIMENTAL. use TOG standard. */
#ifdef	JISX0213
	if (0x40 <= low && low != 0x7f && low <= 0xfc)
	{
		int_u	gr;

		high = (high - 0xe0) * 2 + 1;
		if (low >= 0x7f)
			low--;
		if (low >= 0x9e)
		{
			high++;
			low -= 0x7d;
		}
		else
			low -= 0x1f;

		if (high == 0x22)
			high += 0x06;
		else if (0x26 <= high && high <= 0x29)
			high += 0x06;
		else if (0x2a <= high)
			high += 0x44;

		gr = (int_u)high << 8 | low;
		gr |= 0x8080;

		*ss3 = JP_EUC_G3;
		return(gr);
	}
#endif

	if (high <= 0xf4)
	{
		*ss3 = 0x00;
		high -= 0x05;
		return(sjistojis(high, low) | 0x8080);
	}
	else
	{
		*ss3 = JP_EUC_G3;
		high -= 0x0a;
	}
	return(sjistojis(high, low) | 0x8080);
}

	int_u
jistosjis(char_u high, char_u low)
{
	if (high & 1)
		low += 0x1f;
	else
		low += 0x7d;
	if (low >= 0x7f)
		low++;
	high = ((high - 0x21) >> 1) + 0x81;
	if (high > 0x9f)
		high += 0x40;
	return(((int_u)high << 8) | (low & 0xff));
}

	static int_u
euctosjis(char_u high, char_u low)
{
	return(jistosjis((char_u)(high & 0x7f), (char_u)(low & 0x7f)));
}

	static int_u
euctosjis3(char_u high, char_u low)
{
	int			i;
	int_u		sjis;

	for (i = 0; ss3conv[i].sjis[0]; i++)
	{
		if (ss3conv[i].ss3[0] == JP_EUC_G3
				&& ss3conv[i].ss3[1] == high && ss3conv[i].ss3[2] == low)
		{
			high = ss3conv[i].sjis[0];
			low  = ss3conv[i].sjis[1];
			return(((int_u)high << 8) | (low & 0xff));
		}
	}

#ifdef	JISX0213
	while (0x21 <= (high & 0x7f) && (high & 0x7f) <= 0x7e
							&& 0x21 <= (low & 0x7f) && (low & 0x7f) <= 0x7e)
	{
		high &= 0x7f;
		low  &= 0x7f;

		if (high == 0x21 || 0x23 <= high && high <= 0x25)
			;
		else if (high == 0x28)
			high -= 0x06;
		else if (0x2c <= high && high <= 0x2f)
			high -= 0x06;
		else if (0x6e <= high && high <= 0x7e)
			high -= 0x44;
		else
			break;

		if (high & 0x01)
			low += 0x7e;
		else
		{
			low += 0x1f;
			if (low > 0x7e)
				low++;
		}
		high = high / 2 + 0xe0;
		sjis = (int_u)high << 8 | low;
		return(sjis);
	}
#endif

	sjis = jistosjis((char_u)(high & 0x7f), (char_u)(low & 0x7f));
	if (0xeb40 <= sjis && sjis <= 0xeffc)
		sjis += 0x0a00;
	return(sjis);
}

/*
 * return kanji shift-in string
 */
	static char_u *
kanjiin(int code)
{
	switch(code) {
	case JP_JIS:		return "\033$B";
	default:			return "";
	}
}

/*
 * return kanji shift-out string
 */
	static char_u *
asciiin(int code)
{
	switch(code) {
	case JP_JIS:		return "\033(B";
	default:			return "";
	}
}

/*
 * return kana shift-in string
 */
	static char_u *
kanain(int code)
{
	switch(code) {
	case JP_JIS:		return "\033(I";
	default:			return "";
	}
}

	static char_u *
JPdisp(int *now, int mode, int code)
{
	static	char_u	buffer[32];
	char_u			*p;

	buffer[0] = NUL;
	switch (*now) {
	case JP_ASCII:
		switch (mode) {
		case JP_KANJI:
			p = kanjiin(code);
			if (p)
				STRCAT(buffer, p);
			break;
		case JP_KANA:
			p = kanain(code);
			if (p)
				STRCAT(buffer, p);
			break;
		}
		break;
	case JP_KANJI:
		switch (mode) {
		case JP_ASCII:
			p = asciiin(code);
			if (p)
				STRCAT(buffer, p);
			break;
		case JP_KANA:
			p = kanain(code);
			if (p)
				STRCAT(buffer, p);
			break;
		}
		break;
	case JP_KANA:
		switch (mode) {
		case JP_ASCII:
			p = asciiin(code);
			if (p)
				STRCAT(buffer, p);
			break;
		case JP_KANJI:
			p = kanjiin(code);
			if (p)
				STRCAT(buffer, p);
			break;
		}
		break;
	}
	*now = mode;
	return(buffer);
}

/*
 * convert SJIS letter into suitable letter.
 */
	void
kanjito(char_u *k1, char_u *k2, int code)
{
	int_u		kanji;
	char_u		ss3;

	switch(code) {
	case JP_JIS:
		kanji = sjistojis(*k1, *k2);
		*k1 = (kanji & 0xff00) >> 8;
		*k2 = kanji & 0xff;
		break;
	case JP_EUC:
		kanji = sjistoeuc(*k1, *k2, &ss3);
		*k1 = (kanji & 0xff00) >> 8;
		*k2 = kanji & 0xff;
	    break;
#ifdef UCODE
	case JP_UTF8:
	case JP_WIDE:
		multi2wide(k1, k2, 2, TRUE);
		break;
#endif
	default:
		break;
	}
}

	void
kanato(char_u *k1, char_u *k2, int code)
{
	switch(code) {
	case JP_JIS:
		*k1 &= 0x7f;
		*k2 = NUL;
		break;
	case JP_EUC:
		*k2 = *k1;
		*k1 = JP_EUC_G2;
	    break;
#ifdef UCODE
	case JP_UTF8:
	case JP_WIDE:
		multi2wide(k1, k2, 1, TRUE);
		break;
#endif
	default:
		*k2 = NUL;
		break;
	}
}

/*
 *	Japanese Character class;
 *					Make sure this routine is consistent with search.c:cls().
 *
 * 	for Japanese
 *		3 - alphabet, digits
 *		4 - japanese hiragana
 *		5 - japanese katakana
 *		6 - symbols
 *		7 - other multi-char letter
 */

/*
 * Character class of a code point, used to decide where a word ends.
 */
	int
jpclscp(int cp)
{
	char_u	sjis[2];

	if (cp >= 0x3041 && cp <= 0x309f)
		return JPC_HIRA;
	if ((cp >= 0x30a0 && cp <= 0x30ff) || (cp >= 0x31f0 && cp <= 0x31ff))
		return JPC_KATA;
	if (cp >= 0xff61 && cp <= 0xff9f)
		return JPC_KANA;					/* halfwidth katakana */
	if ((cp >= 0xff10 && cp <= 0xff19)
			|| (cp >= 0xff21 && cp <= 0xff3a)
			|| (cp >= 0xff41 && cp <= 0xff5a))
		return JPC_ALNUM;					/* fullwidth alphanumerics */
	if ((cp >= 0x3400 && cp <= 0x4dbf)
			|| (cp >= 0x4e00 && cp <= 0x9fff)
			|| (cp >= 0xf900 && cp <= 0xfaff)
			|| (cp >= 0x20000 && cp <= 0x3ffff))
		return JPC_KANJI;
	/*
	 * Letters of other scripts are word characters, like ASCII letters: a word
	 * of Hangul or Cyrillic should be one "w" motion, not one per character.
	 */
	if ((cp >= 0x00c0 && cp <= 0x024f)			/* latin extended */
			|| (cp >= 0x0370 && cp <= 0x052f)	/* greek, cyrillic */
			|| (cp >= 0x0590 && cp <= 0x08ff)	/* hebrew, arabic */
			|| (cp >= 0x0e00 && cp <= 0x0e7f)	/* thai */
			|| (cp >= 0x1100 && cp <= 0x11ff)	/* hangul jamo */
			|| (cp >= 0xac00 && cp <= 0xd7a3))	/* hangul syllables */
		return 1;
	/*
	 * Anything else that has a Shift-JIS form keeps the class the JIS table
	 * gives it, so Japanese punctuation behaves as it always did.
	 */
	if (cp2sjis(cp, sjis) == 2)
	{
		int		ret = sjistojis(sjis[0], sjis[1]);
		int		c = ((int_u)ret & 0xff00) >> 8;
		int		k = (int_u)ret & 0xff;

		ret = jptab[c & 0x7f].cls1;
		if (ret == JPC_KIGOU)
			ret = jptab[k & 0x7f].cls2;
		if (ret == JPC_KIGOU2)
			ret = JPC_KIGOU;
		return ret;
	}
	return JPC_KIGOU;						/* some other wide symbol */
}

/*
 * Character class of the character at 'ptr'. 0 for white space, 1 for a word
 * character, -1 for anything else, or one of the JPC_ classes.
 */
	int
jpcls(char_u *ptr)
{
	int		cp;

	if (ptr == NULL)
		return 0;
	if (*ptr == ' ' || *ptr == '\t' || *ptr == NUL)
		return 0;
	if (*ptr < 0x80)
		return isidchar(*ptr) ? 1 : -1;
	cp = utf_decode(ptr, NULL);
	if (cp == UTF8_ERROR)
		return -1;
	return jpclscp(cp);
}

/*
 *	isjppunc(c, k) returns whether a kanji character ck necessary KINSOKU
 *	processing or not.
 */
	int
isjppunc(char_u *ptr, int type)
{
	char_u	sjis[2];
	int		cp;
	int		jis;
	char_u	c, k;

	cp = utf_decode(ptr, NULL);
	if (cp == UTF8_ERROR || cp2sjis(cp, sjis) != 2)
		return FALSE;					/* no line breaking rule for it */
	jis = sjistojis(sjis[0], sjis[1]);
	c = ((int_u)jis & 0xff00) >> 8;
	k =  (int_u)jis & 0xff;
	switch(jptab[c & 0x7f].cls1)
	{
	case JPC_KIGOU:
		return type ? jptab[k & 0x7f].punccsym : jptab[k & 0x7f].puncosym;
	case JPC_HIRA:
	case JPC_KATA:
		return type ? jptab[k & 0x7f].puncckana : FALSE;
	default:
		return FALSE;
	}
}

/*
 *	isaspunc(c, type) returns whether an ascii character ck necessary KINSOKU
 *	processing or not.
 */
	int
isaspunc(char_u c, int type)
{
	return(type ? jptab[c & 0x7f].punccasc: jptab[c & 0x7f].puncoasc);
}

/*
 *	isjsend(*cp) returns whether a JIS character *cp separates
 *	sentences or not.
 */
	int
isjsend(char_u *cp)
{
	int	kanji	= sjistojis(cp[0], cp[1]);
	int	k1		= (kanji & 0xff00) >> 8;
	int	k2		= kanji & 0xff;
	return k1 == '!' && jptab[k2 & 0x7f].stcend;
}

/*
 *	jptocase(&c, &k, tocase)
 *		modify c & k to case tocase
 *			tocase == UPPER : to upper
 *			tocase == LOWER : to lower
 *			tocase == others : swap case
 */
	void
jptocase(char_u *cp, char_u *kp, int tocase)
{
	char_u		k;
	int_u		kanji = sjistojis(*cp, *kp);

	*cp	= (kanji & 0xff00) >> 8;
	*kp = kanji & 0xff;
	k = *kp & 0x7f;
	switch(jptab[*cp & 0x7f].cls1) {
	case JPC_ALNUM:
		if (tocase != LOWER && islower(k))
			*kp = TO_UPPER(k);
		if (tocase != UPPER && isupper(k))
			*kp = TO_LOWER(k);
		break;
	case JPC_KIGOU:
		if (  (tocase != LOWER && jptab[k & 0x7f].scase == JLOS)
		   || (tocase != UPPER && jptab[k & 0x7f].scase == JUPS))
			*kp = jptab[k & 0x7f].swap;
		break;
	case JPC_KATA:
		if (tocase != -1)
			*cp = JP1_HIRA;
		break;
	case JPC_HIRA:
		if (tocase != 1)
			*cp = JP1_KATA;
		break;
	default:
		break;
	}
	kanji = jistosjis(*cp, *kp);
	*cp	= (kanji & 0xff00) >> 8;
	*kp = kanji & 0xff;
}

/*
 *	isspace(c, k) returns whether a kanji character ck space
 */
	int
isjpspace(char_u *ptr)
{
	int		cp;

	if (ptr == NULL || *ptr < 0x80)
		return FALSE;
	cp = utf_decode(ptr, NULL);
	return (cp == 0x3000					/* ideographic space */
			|| cp == 0x00a0					/* no-break space */
			|| (cp >= 0x2000 && cp <= 0x200a)
			|| cp == 0x202f || cp == 0x205f
			|| cp == 0x3164) ? TRUE : FALSE;
}

/*
 *
 *
 */
#ifdef UCODE
	static int
judge_sjis_euc(char_u *ptr)
{
	if (((0xa1 <= ptr[0] && ptr[0] <= 0xfe)
				&& (0xa1 <= ptr[1] && ptr[1] <= 0xfe))
			|| (ptr[0] == 0x8e && (0xa1 <= ptr[1] && ptr[1] <= 0xdf))
			|| (ptr[0] == 0x8f && (0xa1 <= ptr[1] && ptr[1] <= 0xfe)))
		return(TRUE);/* EUC */
	else if ((((0x81 <= ptr[0] && ptr[0] <= 0x9f)
					|| (0xe0 <= ptr[0] && ptr[0] <= 0xef))
				&& ((0x40 <= ptr[1] && ptr[1] <= 0x7e)
					|| (0x80 <= ptr[1] && ptr[1] <= 0xfc)))
			|| (0xa1 <= ptr[0] && ptr[0] <= 0xdf))
		return(TRUE);/* SJIS */
	return(FALSE);
}

	static int
judge_ucs(char_u *ptr)
{
	short_u	ucs;
	char_u	dst[2];

	if (ptr[0] < 0xe0)
		ucs = ((ptr[0] & 0x1f) << 6) | (ptr[1] & 0x3f);
	else
		ucs = ((ptr[0] & 0x0f) << 12) | ((ptr[1] & 0x3f) << 6) | (ptr[2] & 0x3f);
	dst[1] = ucs & 255;
	dst[0] = ucs >> 8;
	if (wide2multi(dst, 2, TRUE, FALSE) == 1 && dst[0] == '?')
		return(FALSE);
	return(TRUE);
}
#endif

/*
 *
 *
 */
	int
judge_jcode(char_u *origcode, int *ubig, char_u *ptr, long size)
{
	char	code;
	int		i;
	int		bfr  = FALSE;	/* Kana Moji */
	int		bfk  = 0;		/* EUC Kana */
	int		sjis = 0;
	int		euc  = 0;
#ifdef UCODE
	int		utf8 = 0;
	int		bfu  = 0;
	int		utf8ok = TRUE;	/* everything so far parses as UTF-8 */
	int		utf8seq = 0;	/* multi-byte sequences seen */
#endif

	code = '\0';
#ifdef UCODE
	if ((ptr[0] == 0xff && ptr[1] == 0xfe)
							|| (ptr[0] == 0xfe && ptr[1] == 0xff))
	{
		if (ptr[0] == 0xfe && ptr[1] == 0xff)
			*ubig = TRUE;
		else
			*ubig = FALSE;
		code = 'U';		/* UNICODE */
		goto breakBreak;
	}
	if (ptr[0] == 0xef && ptr[1] == 0xbb && ptr[2] == 0xbf)
	{
		code = 'T';		/* MS UTF8 */
		goto breakBreak;
	}
	if (*origcode == 'U')
		*origcode = 'S';

	/* valid UTF-8 or not */
	i = 0;
	while (i < size)
	{
		if (ptr[i] < 0x80)
			i++;
		else if (ptr[i] < 0xc0)
		{
			/* malformed */
			utf8 = 0;
			utf8ok = FALSE;
			break;
		}
		else if (ptr[i] < 0xe0)
		{
			if (size - i > 1)
			{
				if (ptr[i + 1] >= 0x80 && ptr[i + 1] < 0xc0)
				{
					if (judge_sjis_euc(&ptr[i]))
						;
					else if (judge_ucs(&ptr[i]))
						utf8++;
				}
				else
				{
					/* malformed */
					utf8 = 0;
					utf8ok = FALSE;
					break;
				}
			}
			utf8seq++;
			i += 2;
		}
		else if (ptr[i] < 0xf0)
		{
			if (size - i > 2)
			{
				if (ptr[i + 1] >= 0x80 && ptr[i + 1] < 0xc0
						&& ptr[i + 2] >= 0x80 && ptr[i + 2] < 0xc0)
				{
					if (judge_sjis_euc(&ptr[i]))
						;
					else if (judge_ucs(&ptr[i]))
						utf8++;
				}
				else
				{
					/* malformed */
					utf8 = 0;
					utf8ok = FALSE;
					break;
				}
			}
			utf8seq++;
			i += 3;
		}
		else if (ptr[i] < 0xf5)
		{
			/*
			 * Four byte sequence, i.e. a character outside the BMP. Neither
			 * Shift-JIS nor EUC ever produces this pattern, so it is strong
			 * evidence; it used to be skipped without counting, which made a
			 * single emoji tip the whole file over to Shift-JIS.
			 */
			if (size - i > 3)
			{
				if (!(ptr[i + 1] >= 0x80 && ptr[i + 1] < 0xc0
						&& ptr[i + 2] >= 0x80 && ptr[i + 2] < 0xc0
						&& ptr[i + 3] >= 0x80 && ptr[i + 3] < 0xc0))
				{
					/* malformed */
					utf8 = 0;
					utf8ok = FALSE;
					break;
				}
				utf8 += 2;
			}
			utf8seq++;
			i += 4;
		}
		else if (ptr[i] < 0xf8)
		{
			/* 0xf5..0xf7: not valid UTF-8 */
			utf8 = 0;
			utf8ok = FALSE;
			break;
		}
		else if (ptr[i] < 0xfc)
		{
			/* valid but not supported */
			if (size - i > 4)
			{
				if (!(ptr[i + 1] >= 0x80 && ptr[i + 1] < 0xc0
						&& ptr[i + 2] >= 0x80 && ptr[i + 2] < 0xc0
						&& ptr[i + 3] >= 0x80 && ptr[i + 3] < 0xc0
						&& ptr[i + 4] >= 0x80 && ptr[i + 4] < 0xc0))
				{
					/* malformed */
					utf8 = 0;
					utf8ok = FALSE;
					break;
				}
			}
			i += 5;
		}
		else if (ptr[i] < 0xfe)
		{
			/* valid but not supported */
			if (size - i > 5)
			{
				if (!(ptr[i + 1] >= 0x80 && ptr[i + 1] < 0xc0
						&& ptr[i + 2] >= 0x80 && ptr[i + 2] < 0xc0
						&& ptr[i + 3] >= 0x80 && ptr[i + 3] < 0xc0
						&& ptr[i + 4] >= 0x80 && ptr[i + 4] < 0xc0
						&& ptr[i + 5] >= 0x80 && ptr[i + 5] < 0xc0))
				{
					/* malformed */
					utf8 = 0;
					utf8ok = FALSE;
					break;
				}
			}
			i += 6;
		}
		else
		{
			/* malformed */
			utf8 = 0;
			utf8ok = FALSE;
			break;
		}
	}
	/*
	 * If the whole buffer parses as UTF-8 and holds at least one multi-byte
	 * character, it is UTF-8. Shift-JIS and EUC never pass this test on real
	 * text: their trailing bytes fall outside the UTF-8 continuation range
	 * (0x80-0xbf), and a one byte halfwidth kana is not a valid lead byte
	 * either. Without this, Japanese UTF-8 was regularly taken for Shift-JIS,
	 * because most of its two byte prefixes also look like valid Shift-JIS.
	 */
	if (utf8ok && utf8seq > 0)
	{
		code = 'T';
		goto breakBreak;
	}
	if (utf8 > 1)
	{
		code = 'T';
		goto breakBreak;
	}
#endif /* UCODE */

	i = 0;
	while (i < size)
	{
		if (ptr[i] == '\033' && (size - i >= 3))
		{
			if ((ptr[i+1] == '$' && ptr[i+2] == 'B')
						 || (ptr[i+1] == '(' && ptr[i+2] == 'B'))
			{
				code = 'J';
#ifdef USE_OPT
				if (!(p_opt & OPT_NO_JIS))
					i += 3;
				else
#endif
				goto breakBreak;
			}
			else if ((ptr[i+1] == '$' && ptr[i+2] == '@')
						 || (ptr[i+1] == '(' && ptr[i+2] == 'J'))
			{
				code = 'J';
#ifdef USE_OPT
				if (!(p_opt & OPT_NO_JIS))
					i += 3;
				else
#endif
				goto breakBreak;
			}
			else if (ptr[i+1] == '(' && ptr[i+2] == 'I')
			{
				code = 'J';
				i += 3;
			}
			else if (ptr[i+1] == ')' && ptr[i+2] == 'I')
			{
				code = 'J';
				i += 3;
			}
			else
				i++;
			bfr = FALSE;
			bfk = 0;
		}
		else
		{
			if (ptr[i] < 0x20)
			{
#ifndef UCODE
				if (bfr == TRUE)
				{
					code = 'S';
					goto breakBreak;
				}
#endif
				bfr = FALSE;
				bfk = 0;
				/* ?? check kudokuten ?? && ?? hiragana ?? */
				if ((i >= 2) && (ptr[i-2] == 0x81)
						&& (0x41 <= ptr[i-1] && ptr[i-1] <= 0x49))
				{
					code = 'S';
					sjis += 100;	/* kudokuten */
				}
				else if ((i >= 2) && (ptr[i-2] == 0xa1)
						&& (0xa2 <= ptr[i-1] && ptr[i-1] <= 0xaa))
				{
					code = 'E';
					euc  += 100;	/* kudokuten */
				}
				else if ((i >= 2) && (ptr[i-2] == 0x82) && (0xa0 <= ptr[i-1]))
					sjis += 40;		/* hiragana */
				else if ((i >= 2) && (ptr[i-2] == 0xa4) && (0xa0 <= ptr[i-1]))
					euc  += 40;		/* hiragana */
#ifdef UCODE
				/*
				 * These look back two or three bytes, so there have to be
				 * that many: a file that starts with a newline used to read
				 * before the start of the buffer here.
				 */
				else if ((i >= 2) && (0xa1 <= ptr[i-2] && ptr[i-2] <= 0xfe)
								&& (0xa1 <= ptr[i-1] && ptr[i-1] <= 0xfe))
					;	/* EUC */
				else if ((i >= 2) && 0x8e == ptr[i-2]
								&& (0xa1 <= ptr[i-1] && ptr[i-1] <= 0xdf))
					;	/* EUC */
				else if ((i >= 2) && ((0x81 <= ptr[i-2] && ptr[i-2] <= 0x9f)
								|| (0xe0 <= ptr[i-2] && ptr[i-2] <= 0xef))
						&& ((0x40 <= ptr[i-1] && ptr[i-1] <= 0x7e)
								|| (0x80 <= ptr[i-1] && ptr[i-1] <= 0xfc)))
					;	/* SJIS */
				else if ((i >= 3) && (ptr[i-3] & 0xf0) == 0xe0
											&& (ptr[i-2] & 0xc0) == 0x80
											&& (ptr[i-1] & 0xc0) == 0x80)
				{
					code = 'T';
					utf8  += 30;
				}
				else if ((i >= 2) && (ptr[i-2] & 0xe0) == 0xc0
											&& (ptr[i-1] & 0xc0) == 0x80)
				{
					code = 'T';
					utf8  += 10;
				}
#endif
			}
			else
			{
				/* ?? check hiragana or katana ?? */
				if ((size - i > 1) && (ptr[i] == 0x82) && (0xa0 <= ptr[i+1]))
					sjis++;	/* hiragana */
				else if ((size - i > 1) && (ptr[i] == 0x83)
						&& (0x40 <= ptr[i+1] && ptr[i+1] <= 0x9f))
					sjis++;	/* katakana */
				else if ((size - i > 1) && (ptr[i] == 0xa4) && (0xa0 <= ptr[i+1]))
					euc++;	/* hiragana */
				else if ((size - i > 1) && (ptr[i] == 0xa5) && (0xa0 <= ptr[i+1]))
					euc++;	/* katakana */
#ifdef UCODE
				if (bfu)
					bfu--;
				else
#endif
				if (bfr == TRUE)
				{
					if ((i >= 1) && (0x40 <= ptr[i] && ptr[i] <= 0xa0) && sjis_islead(ptr[i-1]))
					{
						code = 'S';
						goto breakBreak;
					}
					else if ((i >= 1) && (0x81 <= ptr[i-1] && ptr[i-1] <= 0x9f) && ((0x40 <= ptr[i] && ptr[i] < 0x7e) || (0x7e < ptr[i] && ptr[i] <= 0xfc)))
					{
						code = 'S';
						goto breakBreak;
					}
					else if ((i >= 1) && (0xfd <= ptr[i] && ptr[i] <= 0xfe) && (0xa1 <= ptr[i-1] && ptr[i-1] <= 0xfe))
					{
						code = 'E';
						goto breakBreak;
					}
					else if ((i >= 1) && (0xfd <= ptr[i-1] && ptr[i-1] <= 0xfe) && (0xa1 <= ptr[i] && ptr[i] <= 0xfe))
					{
						code = 'E';
						goto breakBreak;
					}
					else if ((i >= 1) && (ptr[i] < 0xa0 || 0xdf < ptr[i]) && (0x8e == ptr[i-1]))
					{
						code = 'S';
						goto breakBreak;
					}
					else if (ptr[i] <= 0x7f)
					{
						code = 'S';
						goto breakBreak;
					}
					else
					{
						if (0xa1 <= ptr[i] && ptr[i] <= 0xa6)
							euc++;	/* sjis hankaku kana kigo */
						else if (0xa1 <= ptr[i] && ptr[i] <= 0xdf)
							;	/* sjis hankaku kana */
						else if (0xa1 <= ptr[i] && ptr[i] <= 0xfe)
							euc++;
						else if (0x8e == ptr[i])
							euc++;
						else if (0x20 <= ptr[i] && ptr[i] <= 0x7f)
							sjis++;
						bfr = FALSE;
						bfk = 0;
					}
				}
#ifdef UCODE
				else if ((size - i > 3) && (ptr[i] & 0xf0) == 0xe0
											&& (ptr[i+1] & 0xc0) == 0x80
											&& (ptr[i+2] & 0xc0) == 0x80
										&& ((ptr[i+3] & 0x80) == 0x00
											|| (ptr[i+3] & 0xf0) == 0xe0
											|| (ptr[i+3] & 0xe0) == 0xc0)
						&& !((0xa1 <= ptr[i] && ptr[i] <= 0xfe)
								&& (0xa1 <= ptr[i+1] && ptr[i+1] <= 0xfe)
								&& (0xa1 <= ptr[i+2] && ptr[i+2] <= 0xfe)))
				{
					utf8++;
					bfu = 2;
					bfk = 0;
				}
				else if ((size - i > 2) && (ptr[i] & 0xe0) == 0xc0
											&& (ptr[i+1] & 0xc0) == 0x80
										&& ((ptr[i+2] & 0x80) == 0x00
											|| (ptr[i+2] & 0xf0) == 0xe0
											|| (ptr[i+2] & 0xe0) == 0xc0)
						&& !((0xa1 <= ptr[i] && ptr[i] <= 0xfe)
								&& (0xa1 <= ptr[i+1] && ptr[i+1] <= 0xfe)))
				{
					utf8++;
					bfu = 1;
					bfk = 0;
				}
#endif
				else if (0x8e == ptr[i])
				{
					if (size - i <= 1)
						;
					else if (0xa1 <= ptr[i+1] && ptr[i+1] <= 0xdf)
					{
						/* EUC KANA or SJIS KANJI */
						if (bfk == 1)
							euc += 100;
						bfk++;
						i++;
					}
					else
					{
						/* SJIS only */
#ifdef UCODE
						sjis += 100;
#else
						code = 'S';
						goto breakBreak;
#endif
					}
				}
				else if (0x8f == ptr[i])
				{
					if (size - i <= 2)
						;
					else if ((0xa1 <= ptr[i+1] && ptr[i+1] <= 0xfe)
							&& (0xa1 <= ptr[i+2] && ptr[i+2] <= 0xfe))
					{
						euc += 10;
						i += 2;
					}
					else
					{
						/* SJIS only */
#ifdef UCODE
						sjis += 10;
#else
						code = 'S';
						goto breakBreak;
#endif
					}
				}
				else if (0x81 <= ptr[i] && ptr[i] <= 0x9f)
				{
					/* SJIS only */
#ifdef UCODE
					sjis += 1;
#else
					code = 'S';
					if ((size - i >= 1)
							&& ((0x40 <= ptr[i+1] && ptr[i+1] <= 0x7e)
								|| (0x80 <= ptr[i+1] && ptr[i+1] <= 0xfc)))
						goto breakBreak;
#endif
				}
				else if (0xfd <= ptr[i] && ptr[i] <= 0xfe)
				{
					/* EUC only */
					code = 'E';
					if ((size - i >= 1)
								&& (0xa1 <= ptr[i+1] && ptr[i+1] <= 0xfe))
						goto breakBreak;
				}
				else if (ptr[i] <= 0x7f)
					;
				else
				{
					bfr = TRUE;
					bfk = 0;
				}
			}
			i++;
		}
	}
#ifdef UCODE
	if (code == '\0' || (utf8 + sjis + euc) > size)
#else
	if (code == '\0')
#endif
	{
		code = *origcode;
#ifdef UCODE
		if (utf8 > sjis && utf8 > euc)
			code = 'T';
		else
#endif
		if (sjis > euc)
			code = 'S';
		else if (sjis < euc)
			code = 'E';
	}
breakBreak:
	return(code);
}

/*
 * Shift-JIS byte tests, for the pivot only. The editor itself uses ISkanji(),
 * which classifies UTF-8.
 */
	static int
sjis_islead(int c)
{
	return (kanji_map_sjis[c & 0xff] & 1) != 0;
}

	static int
sjis_iskana(int c)
{
	return (kanji_map_sjis[c & 0xff] & 2) != 0;	/* one byte halfwidth kana */
}

	static int
sjis_isdisp(int c)
{
	return kanji_map_sjis[c & 0xff] != 0;
}

/*
 * One or two Shift-JIS bytes at src -> code point, or UTF8_ERROR.
 */
/*
 * Is this a well formed Shift-JIS sequence? The conversion tables index on the
 * two bytes without checking them, so anything else has to be rejected here.
 */
	static int
sjis_valid(int c1, int c2, int len)
{
	if (c1 < 0x80)
		return TRUE;
	if (c1 >= 0xa1 && c1 <= 0xdf)
		return len >= 1;					/* one byte halfwidth kana */
	if (!((c1 >= 0x81 && c1 <= 0x9f) || (c1 >= 0xe0 && c1 <= 0xfc)))
		return FALSE;
	if (len < 2)
		return FALSE;
	return ((c2 >= 0x40 && c2 <= 0x7e) || (c2 >= 0x80 && c2 <= 0xfc));
}

	static int
sjis2cp(char_u *src, int len)
{
	char_u	buf[2];
	int		cp;

	if (src[0] < 0x80)
		return src[0];
	if (!sjis_valid(src[0], (len > 1) ? src[1] : 0, len))
		return UTF8_ERROR;
	buf[0] = src[0];
	buf[1] = (len > 1) ? src[1] : NUL;
	multi2wide(&buf[0], &buf[1], len, TRUE);		/* TRUE: big endian out */
	cp = (buf[0] << 8) | buf[1];
	return cp == 0 ? UTF8_ERROR : cp;
}

/*
 * Code point -> Shift-JIS bytes in buf (needs 2). Returns the length, or 0 when
 * the character has no Shift-JIS form.
 */
	static int
cp2sjis(int cp, char_u *buf)
{
	char_u	w[2];
	int		len;

	if (cp < 0)
		return 0;
	if (cp < 0x80)
	{
		buf[0] = cp;
		return 1;
	}
	if (cp > 0xffff)			/* outside the BMP: nothing to map onto */
		return 0;
	w[0] = (cp >> 8) & 0xff;	/* big endian, wide2multi() swaps it back */
	w[1] = cp & 0xff;
	len = wide2multi(w, 2, TRUE, FALSE);
	if (len <= 0)
		return 0;
	if (len == 1 && w[0] == '?' && cp != '?')
		return 0;				/* the code page has no such character */
	memcpy((char *)buf, (char *)w, (size_t)len);
	return len;
}

/*
 * Shift-JIS -> UTF-8. Returns the length written, or -1 when dst is too small.
 */
	static int
sjis2utf8_n(char_u *src, int srclen, char_u *dst, int dstlen)
{
	char_u	*d = dst;
	int		i = 0;

	while (i < srclen)
	{
		char_u	buf[UTF8_MAXLEN];
		int		n, cp, len;

		if (src[i] < 0x80)
		{
			if (d - dst >= dstlen)
				return -1;
			*d++ = src[i++];
			continue;
		}
		if (iskeycode(&src[i], srclen - i, &n))
		{
			if ((int)(d - dst) + n > dstlen)
				return -1;
			memmove(d, &src[i], (size_t)n);
			d += n;
			i += n;
			continue;
		}
		n = (sjis_islead(src[i]) && i + 1 < srclen) ? 2 : 1;
		cp = sjis2cp(&src[i], n);
		if (cp == UTF8_ERROR)
		{
			cp = '?';
			n = 1;					/* not a real character: skip one byte */
		}
		len = utf_encode(cp, buf);
		if ((int)(d - dst) + len > dstlen)
			return -1;
		memcpy((char *)d, (char *)buf, (size_t)len);
		d += len;
		i += n;
	}
	return (int)(d - dst);
}

/*
 * UTF-8 -> Shift-JIS, into a fresh buffer. Characters with no Shift-JIS form
 * become '?'; that loss is inherent in writing a legacy encoding.
 */
	static char_u *
utf82sjis(char_u *src)
{
	char_u	*top;
	char_u	*d;

	if (src == NULL)
		return NULL;
	/* Shift-JIS is never longer than the UTF-8 it came from. */
	if ((top = alloc((unsigned)(STRLEN(src) + 1))) == NULL)
		return NULL;
	d = top;
	while (*src != NUL)
	{
		int		len;
		int		cp;
		int		n;

		if (*src < 0x80)
		{
			*d++ = *src++;
			continue;
		}
		cp = utf_decode(src, &len);
		src += len;
		if (cp == UTF8_ERROR)
		{
			*d++ = '?';
			continue;
		}
		n = cp2sjis(cp, d);
		if (n == 0)
			*d++ = '?';
		else
			d += n;
	}
	*d = NUL;
	return top;
}

/*
 * UTF-8 -> UCS-2, in a fresh buffer terminated by a 16 bit NUL. Characters
 * outside the BMP become a surrogate pair, so nothing is lost.
 */
	static char_u *
utf82ucs2(char_u *src, int ubig)
{
	char_u	*top;
	char_u	*d;

	if (src == NULL)
		return NULL;
	/* At worst every input byte turns into one 16 bit unit. */
	if ((top = alloc((unsigned)(STRLEN(src) * 2 + 4))) == NULL)
		return NULL;
	d = top;
	for (;;)
	{
		int		cp;
		int		len;
		int		units[2];
		int		n = 1;
		int		i;

		if (*src == NUL)
		{
			cp = 0;
			len = 0;
		}
		else
		{
			cp = utf_decode(src, &len);
			if (cp == UTF8_ERROR)
				cp = '?';
		}
		if (cp > 0xffff)
		{
			cp -= 0x10000;
			units[0] = 0xd800 + (cp >> 10);
			units[1] = 0xdc00 + (cp & 0x3ff);
			n = 2;
		}
		else
			units[0] = cp;
		for (i = 0; i < n; i++)
		{
			if (ubig)
			{
				*d++ = (units[i] >> 8) & 0xff;
				*d++ = units[i] & 0xff;
			}
			else
			{
				*d++ = units[i] & 0xff;
				*d++ = (units[i] >> 8) & 0xff;
			}
		}
		if (len == 0)
			break;					/* the terminating NUL is written */
		src += len;
	}
	return top;
}

/*
 * UCS-2 -> UTF-8. 'srclen' is in bytes. Returns the length written to dst, or
 * -1 when it does not fit. Surrogate pairs are joined back together.
 */
	int
ucs22utf8_n(char_u *src, int srclen, char_u *dst, int dstlen, int ubig)
{
	char_u	*d = dst;
	int		i = 0;

	while (i + 1 < srclen)
	{
		char_u	buf[UTF8_MAXLEN];
		int		unit;
		int		cp;
		int		len;

		unit = ubig ? ((src[i] << 8) | src[i + 1])
					: ((src[i + 1] << 8) | src[i]);
		i += 2;
		if (unit >= 0xd800 && unit <= 0xdbff && i + 1 < srclen)
		{							/* high surrogate: take the low one too */
			int		low = ubig ? ((src[i] << 8) | src[i + 1])
							   : ((src[i + 1] << 8) | src[i]);

			if (low >= 0xdc00 && low <= 0xdfff)
			{
				cp = 0x10000 + ((unit - 0xd800) << 10) + (low - 0xdc00);
				i += 2;
			}
			else
				cp = '?';			/* unpaired */
		}
		else if (unit >= 0xd800 && unit <= 0xdfff)
			cp = '?';				/* unpaired surrogate */
		else
			cp = unit;
		len = utf_encode(cp, buf);
		if ((int)(d - dst) + len > dstlen)
			return -1;
		memcpy((char *)d, (char *)buf, (size_t)len);
		d += len;
	}
	return (int)(d - dst);
}

/*
 * Set while converting what was typed rather than text, by keyconvsfrom().
 *
 * A special key does not arrive as a character: the machine-dependent input
 * code reports it as K_NUL followed by a key code byte, and CTRL-@ as K_ZERO,
 * for check_termcode() to match. Those bytes are not characters in any code
 * 'jmask' can name, so conversion turned each of them into '?'.
 *
 * They cannot be picked out by scanning the bytes: K_NUL (0xfd) and K_ZERO
 * (0xa0) are both perfectly good second bytes of a Shift-JIS character -- 0x82
 * 0xa0 is HIRAGANA A -- and 0xa0 is a UTF-8 continuation byte. So the tests sit
 * where the converters already are at the start of a character, and only there.
 */
static int		keycodes = FALSE;

/*
 * kanjiconvsfrom() for a chunk of typed bytes: convert the characters in it,
 * and let the key codes through untouched.
 */
	int				/* return the length of dst */
keyconvsfrom(char_u *ptr, int ptrlen, char_u *dst, int dstlen, char *tail, char code, int *charsetp)
{
	int		n;

	keycodes = TRUE;
	n = kanjiconvsfrom(ptr, ptrlen, dst, dstlen, tail, code, charsetp);
	keycodes = FALSE;
	return n;
}

/*
 * True for a byte that is a key code rather than the start of a character.
 * 'n' is set to how many bytes of it there are.
 */
	static int
iskeycode(char_u *ptr, int ptrlen, int *n)
{
	if (!keycodes || ptrlen <= 0)
		return FALSE;
	if (ptr[0] == K_NUL)
	{
			/* the key code byte after K_NUL is not a character either */
		*n = (ptrlen > 1) ? 2 : 1;
		return TRUE;
	}
	if (ptr[0] == K_ZERO)
	{
		*n = 1;
		return TRUE;
	}
	return FALSE;
}

/*
 * Convert 'ptrlen' bytes at 'ptr' from encoding 'code' into the internal UTF-8,
 * writing at most 'dstlen' bytes to 'dst'. Returns the length written, or -1
 * when it does not fit.
 */
	int				/* return the length of dst */
kanjiconvsfrom(char_u *ptr, int ptrlen, char_u *dst, int dstlen, char *tail, char code, int *charsetp)
{
	char_u	*tmp;
	int		n;

	if (tail)
		tail[0] = NUL;
#ifdef UCODE
	if (code == JP_UTF8)
	{
		/*
		 * Already the internal encoding. Copy it through, replacing anything
		 * malformed so the buffer only ever holds valid UTF-8.
		 */
		char_u	*d = dst;
		int		i = 0;

		while (i < ptrlen)
		{
			int		cp, len, need;
			char_u	buf[UTF8_MAXLEN];

			if (ptr[i] < 0x80)
			{
				if (d - dst >= dstlen)
					return -1;
				*d++ = ptr[i++];
				continue;
			}
			if (iskeycode(&ptr[i], ptrlen - i, &len))
			{
				if ((int)(d - dst) + len > dstlen)
					return -1;
				memmove(d, &ptr[i], (size_t)len);
				d += len;
				i += len;
				continue;
			}
			/*
			 * Check for a character split across the end of this chunk BEFORE
			 * decoding it: the missing bytes would make utf_decode() call it
			 * malformed and every byte would turn into '?'. That is what
			 * mangled pasted text, which arrives in chunks.
			 */
			need = utf_len(ptr[i]);
			if (need > 1 && i + need > ptrlen && tail != NULL)
			{
				int		k;

				for (k = 0; i + k < ptrlen && k < UTF8_MAXLEN; k++)
					tail[k] = ptr[i + k];
				tail[k] = NUL;
				break;
			}
			cp = utf_decode(&ptr[i], &len);
			if (cp == UTF8_ERROR)
			{
				if (d - dst >= dstlen)
					return -1;
				*d++ = '?';
				i += len;
				continue;
			}
			len = utf_encode(cp, buf);
			if ((int)(d - dst) + len > dstlen)
				return -1;
			memcpy((char *)d, (char *)buf, (size_t)len);
			d += len;
			i += len;
		}
		return (int)(d - dst);
	}
#endif
	/* Legacy encodings pivot through Shift-JIS. */
	if ((tmp = lalloc((long_u)(ptrlen * 2 + 8), TRUE)) == NULL)
		return -1;
	n = sjis_convsfrom(ptr, ptrlen, tmp, ptrlen * 2 + 8, tail, code, charsetp);
	if (n < 0)
	{
		free(tmp);
		return -1;
	}
	n = sjis2utf8_n(tmp, n, dst, dstlen);
	free(tmp);
	return n;
}

/*
 * Convert the internal UTF-8 string 'ptr' to encoding 'code', into a fresh
 * buffer that the caller frees.
 */
	char_u *
kanjiconvsto(char_u *ptr, int code, int ubig)
{
	char_u	*sjis;
	char_u	*res;

	if (ptr == NULL)
		return ptr;
#ifdef UCODE
	if (code == JP_UTF8)
	{								/* already the internal encoding */
		int		len = STRLEN(ptr) + 1;

		if ((res = alloc((unsigned)len)) == NULL)
			return NULL;
		memcpy((char *)res, (char *)ptr, (size_t)len);
		return res;
	}
	if (code == JP_WIDE)
		return utf82ucs2(ptr, ubig);
#endif
	if ((sjis = utf82sjis(ptr)) == NULL)
		return NULL;
	res = sjis_convsto(sjis, code, ubig);
	free(sjis);
	return res;
}

	static int		/* return the length of dst */
sjis_convsfrom(char_u *ptr, int ptrlen, char_u *dst, int dstlen, char *tail, char code, int *charsetp)
{
	char_u	*dtop;
	int		c;
	int		charset;

	dtop = dst;
	if (tail)
		tail[0] = NUL;

	switch (code) {
	case JP_EUC:
	case JP_SJIS:
	case JP_JIS:
#ifdef UCODE
	case JP_UTF8:
#endif
		if (charsetp)
			charset = *charsetp;
		else
			charset = JP_ASCII;
		while (ptrlen) {
			int		keylen;

			if (dst - dtop >= dstlen)
				return -1;
			if (iskeycode(ptr, ptrlen, &keylen))
			{
				if ((dst - dtop) + keylen > dstlen)
					return -1;
				memmove(dst, ptr, (size_t)keylen);
				dst += keylen;
				ptr += keylen;
				ptrlen -= keylen;
				continue;
			}
			ptrlen --;

			if (*ptr & 0x80)
			{
				charset = JP_ASCII;
				switch (code) {
				case JP_EUC:
					switch (kanji_map_euc[*ptr]) {
					case 1:		/* kanji */
						if (ptrlen >= 1 && ptr[1] != NUL)
						{					/* JIS0208 char. */
							*dst++ = (euctosjis(ptr[0], ptr[1]) & 0xff00) >> 8;
							*dst++ = euctosjis(ptr[0], ptr[1]) & 0xff;
							if (dst[-1] == '\0')
								dst[-2] = dst[-1] = ' ';
							ptr += 2;
							ptrlen --;
						}
						else if (tail && ptrlen == 0)
						{					/* not completed  */
							tail[0] = *ptr;
							tail[1] = NUL;
						}
						else
						{					/* Illegal char  */
							*(dst - 1) &= 0x7f;
						}
						continue;
					case 3:		/* kanji */
						if (ptrlen >= 2 && ptr[2] != NUL)
						{					/* JIS0208 char. */
							*dst++ = (euctosjis3(ptr[1], ptr[2]) & 0xff00) >> 8;
							*dst++ = euctosjis3(ptr[1], ptr[2]) & 0xff;
							if (dst[-1] == '\0')
								dst[-2] = dst[-1] = ' ';
							ptr += 3;
							ptrlen -= 2;
						}
						else if (tail && ptrlen < 1)
						{					/* not completed  */
							tail[0] = ptr[0];
							tail[1] = (ptrlen == 0 ? NUL : ptr[1]);
							tail[2] = NUL;
						}
						else
						{					/* Illegal char  */
							*(dst - 1) &= 0x7f;
						}
						continue;
					case 2:
						if (ptr[1] != NUL)
						{
							if (p_jkc && sjis_iskana(ptr[1]))
							{
								char_u c1 = NUL;
								char_u c2 = NUL;

								if ((ptrlen >= 3) && (ptr[2] == JP_EUC_G2))
								{
									if (jisx0201rto0208(ptr[1], ptr[3], &c1, &c2))
									{
										ptr += 4;
										*dst++ = c1;
										*dst++ = c2;
										ptrlen -= 3;
									}
									else
									{
										ptr += 2;
										*dst++ = c1;
										*dst++ = c2;
										ptrlen --;
									}
								}
								else
								{
									(void)jisx0201rto0208(ptr[1], NUL, &c1, &c2);
									ptr += 2;
									*dst++ = c1;
									*dst++ = c2;
									ptrlen --;
								}
							}
							else
							{
								*dst++  = * ++ptr;
								ptr++;
								ptrlen --;
							}
							continue;
						}
						if (tail && ptrlen == 0)
						{					/* not completed  */
							tail[0] = *ptr;
							tail[1] = NUL;
							continue;
						}
						break;
					default:
						if (!sjis_isdisp(*ptr))
							*dst ++ = *ptr ++;
						else
						{
							*dst = *ptr & 0x7f;
							ptr++;
							dst++;
						}
						break;
					}
					break;
				case JP_SJIS:
					c = *dst ++ = *ptr ++;
					if (sjis_islead(c))
					{
						if (ptrlen >= 1 && *ptr != NUL)
						{
							* dst++ = * ptr++;
							if (dst[-1] == '\0')
								dst[-2] = dst[-1] = ' ';
							ptrlen --;
						}
						else if (tail && ptrlen == 0)
						{
							tail[0] = c;
							tail[1] = NUL;
							dst--;
						}
						else
						{
							*(dst - 1) &= 0x7f;
						}
						continue;
					}
					else if (p_jkc && sjis_iskana(c))
					{					/* JIS X 0201R 8bit encoding */
						char_u c1 = NUL;

						if (jisx0201rto0208((char_u)c, (char_u)(ptrlen ? *ptr : NUL), &dst[-1], &c1))
						{	/* 2 characters -> double byte character. */
							* dst++ = c1;
							ptr ++;
							ptrlen --;
						}
						else if (c1)
							* dst++ = c1;
						continue;
					}
					break;
#ifdef UCODE
				case JP_UTF8:
					c = *ptr++;
					if (c < 0xc0)
					{ /* malformed */
						*dst++ = '?';
					}
					else if (c < 0xe0 && ptrlen >= 1)
					{
						short_u ucs = ((c & 0x1f) << 6) | (ptr[0] & 0x3f);
						int len;
						dst[1] = ucs & 255;
						dst[0] = ucs >> 8;
						len = wide2multi(dst, 2, TRUE, FALSE);
						dst += len;
						ptr += 1;
						ptrlen--;
					}
					else if (c < 0xf0 && ptrlen >= 2)
					{
						short_u ucs = ((c & 0x0f) << 12) |
							((ptr[0] & 0x3f) << 6) | (ptr[1] & 0x3f);
						int len;
						dst[1] = ucs & 255;
						dst[0] = ucs >> 8;
						len = wide2multi(dst, 2, TRUE, FALSE);
						dst += len;
						ptr += 2;
						ptrlen -= 2;
					}
					else if (c < 0xf8 && ptrlen >= 3)
					{
						ptr += 3;
						ptrlen -= 3;
						*dst++ = '?';
					}
					else if (c < 0xfc  && ptrlen >= 4)
					{
						ptr += 4;
						ptrlen -= 4;
						*dst++ = '?';
					}
					else if (c < 0xfe  && ptrlen >= 5)
					{
						ptr += 5;
						ptrlen -= 5;
						*dst++ = '?';
					}
					else
					{ /* malformed */
						*dst++ = '?';
					}
					if (tail && ptrlen == 0)
					{					/* not completed  */
						tail[0] = *ptr;
						tail[1] = NUL;
						continue;
					}
					break;
#endif /* UCODE */
				default:
					if (!sjis_isdisp(*ptr))
						*dst ++ = *ptr ++;
					else
					{
						*dst = *ptr & 0x7f;
						dst++;
						ptr++;
					}
					break;
				}
			}
			else
			{
				c = *dst ++ = *ptr ++;
#ifdef USE_OPT
				if (c == ESC && (code == JP_JIS || !(p_opt & OPT_NO_JIS)))
#else
				if (c == ESC)
#endif
				{
					dst --;

					if (ptrlen == 0)
					{
						if (tail && code == JP_JIS)
						{
							tail[0] = c;
							tail[1] = NUL;
						}
						else
							dst++;
						continue;
					}

					if (*ptr == '(')
					{
						ptrlen --;
						ptr ++;

						if (ptrlen == 0)
						{
							if (tail && code == JP_JIS)
							{
								tail[0] = c;
								tail[1] = '(';
								tail[2] = NUL;
							}
							else
							{
								dst ++;
								* dst++ = '(';
							}
							continue;
						}

						if (*ptr == 'I')
						{						/* JIS X 0201R ISO2022 encoding */
							charset = JP_KANA;
							ptr ++;
							ptrlen --;
							continue;
						}
						else if (*ptr == 'J' || *ptr == 'H' || *ptr == 'B')
						{						/* ASCII/JIS In */
							charset = JP_ASCII;
							ptr ++;
							ptrlen --;
							continue;
						}
						else
						{
							dst ++;
							* dst++ = '(';
							continue;
						}
					}

					if (*ptr == '$')
					{
						ptrlen --;
						ptr ++;

						if (ptrlen == 0)
						{
							if (tail && code == JP_JIS)
							{
								tail[0] = c;
								tail[1] = '$';
								tail[2] = NUL;
							}
							else
							{
								dst ++;
								* dst++ = '$';
							}
							continue;
						}

						if (*ptr == '@' || *ptr == 'B')		/* Kanji In */
						{
							charset = JP_KANJI;
							ptrlen --;
							ptr ++;
							continue;
						}
						else
						{
							dst ++;
							* dst ++ = '$';
							continue;
						}
					}
					else
						dst++;
				}
				switch (charset)
				{
					case JP_ASCII:
						break;

					case JP_KANA:
						if (p_jkc)
						{					/* JIS X 0201R 8bit encoding */
							char_u c1 = NUL;

							if (jisx0201rto0208((char_u)c, (char_u)(ptrlen ? *ptr : NUL), &dst[-1], &c1))
							{	/* 2 characters -> double byte character. */
								* dst++ = c1;
								ptr ++;
								ptrlen --;
							}
							else if (c1)
								* dst++ = c1;
							continue;
						}
						else
						{
							dst[-1] |= 0x80;
						}
						break;

					default: /* JP_KANJI */
						if (ptrlen == 0)
						{
							if (tail && code == JP_JIS)
							{
								tail[0] = c;
								tail[1] = NUL;
								dst --;
							}
						}
						else if (c > ' ' && *ptr > ' ')
						{
							ptrlen --;
							dst[-1] = (jistosjis(ptr[-1], ptr[0]) & 0xff00) >> 8;
							dst[0]  = jistosjis(ptr[-1], ptr[0]) & 0xff;
							if (dst[0] == '\0')
								dst[-1] = dst[0] = ' ';
							dst++;
							ptr++;
						}
						else if (c == NUL || c == '\n' || c == '\r')
							charset = JP_ASCII;
				}
			}
		}
		if (charsetp)
			*charsetp = charset;
		break; /* return dst - dtop; */
#ifdef UCODE
	case JP_WIDE:		/* UNICODE */
#endif
	default:
		while (ptrlen)
		{
			*dst ++ = *ptr ++;
			ptrlen --;
			if (dst - dtop > dstlen)
				return -1;
		}
		break; /* return dst - dtop; */
	}
	return dst - dtop;
}

#ifdef UCODE
# define TO_UTF8(ucs, ptr) \
				if (ucs < 0x80) { *ptr ++ = ucs; } \
				else if (ucs < 0x800) { \
					*ptr ++ = (ucs >> 6) | 0xc0; \
					*ptr ++ = (ucs & 0x3f) | 0x80; \
				} else { \
					*ptr ++ = (ucs >> 12) | 0xe0; \
					*ptr ++ = ((ucs >> 6) & 0x3f) | 0x80; \
					*ptr ++ = (ucs & 0x3f) | 0x80; }
#endif

	static char_u *
sjis_convsto(char_u *ptr, int code, int ubig)
{
	char_u	*top, *ptr2;
	char_u	*cp;
	char_u	ss3;
	int_u	kanji;
	int		nshift;
	int		mode;

	if (ptr == NULL)
		return ptr;

	top = ptr;

	switch (code) {
	case JP_SJIS:
		nshift = STRLEN(ptr) + 1;
		top = alloc(nshift);
		memcpy((char *)top, (char *)ptr, nshift);
		top[nshift - 1] = '\0';
		return top;
#ifdef UCODE
	case JP_UTF8:
		for(nshift = 0; *ptr; ptr++)
		{
			if (sjis_islead(*ptr))
			{
				nshift += 2;
				ptr++;
			}
			else if (sjis_iskana(*ptr))
				nshift += 2;
		}
		ptr = top;
		top = ptr2 = alloc(STRLEN(top) + nshift + 1);
		while (*ptr)
		{
			char_u buf[2];
			if (sjis_islead(*ptr))
			{
				buf[0] = ptr[0];
				buf[1] = ptr[1];
				multi2wide(buf, buf + 1, 2, TRUE);
				kanji = buf[1] | (buf[0] << 8);
				TO_UTF8(kanji, ptr2);
				ptr += 2;
			}
			else if (sjis_iskana(*ptr))
			{
				buf[0] = ptr[0];
				buf[1] = '\0';
				multi2wide(buf, buf + 1, 1, TRUE);
				kanji = buf[1] | (buf[0] << 8);
				TO_UTF8(kanji, ptr2);
				ptr += 1;
			}
			else
				*ptr2 ++ = *ptr ++;
		}
		*ptr2 = NUL;
		return top;
	case JP_WIDE:		/* UNICODE */
		for (nshift = 0; *ptr; ptr++)
		{
			if (sjis_islead(*ptr))
				ptr++;
			else
				nshift++;
		}
		ptr = top;
		top = ptr2 = alloc(STRLEN(top) + nshift + 2);
		while (*ptr)
		{
			if (sjis_islead(*ptr))
			{
				ptr2[0] = ptr[0];
				ptr2[1] = ptr[1];
				multi2wide(&ptr2[0], &ptr2[1], 2, ubig);
				ptr  += 2;
				ptr2 += 2;
			}
			else
			{
				ptr2[0] = ptr[0];
				ptr2[1] = 0;
				multi2wide(&ptr2[0], &ptr2[1], 1, ubig);
				ptr  += 1;
				ptr2 += 2;
			}
		}
		ptr2[0] = ptr[0];
		ptr2[1] = 0;
		multi2wide(&ptr2[0], &ptr2[1], 1, ubig);
		return top;
#endif /* UCODE */
	case JP_EUC:
		for(nshift = 0; *ptr; ptr++)
		{
			if (sjis_islead(*ptr))
			{
				if (IS_X0212(*ptr))
					nshift++;
				ptr++;
			}
			else if (sjis_iskana(*ptr))
				nshift++;
		}
		ptr = top;
		top = ptr2 = alloc(STRLEN(top) + nshift + 1);
		while (*ptr)
		{
			if (sjis_islead(*ptr))
			{
				kanji = sjistoeuc(ptr[0], ptr[1], &ss3);
				if (ss3)
					*ptr2 ++ = ss3;
				*ptr2 ++ = (kanji & 0xff00) >> 8;
				*ptr2 ++ = kanji & 0xff;
				ptr += 2;
			}
			else if (sjis_iskana(*ptr))
			{
				*ptr2 ++ = JP_EUC_G2;
				*ptr2 ++ = *ptr ++;
			}
			else
				*ptr2 ++ = *ptr ++;
		}
		*ptr2 = NUL;
		return top;
	case JP_JIS:
		kanji = 0;
		for(nshift = 0; *ptr; ptr++)
		{
			if (sjis_islead(*ptr))
			{
				ptr++;
				if (kanji != 1)
					nshift++;
				kanji = 1;
			}
			else if (sjis_iskana(*ptr))
			{
				if (kanji != 2)
					nshift++;
				kanji = 2;
			}
			else
			{
				if (kanji != 0)
					nshift++;
				kanji = 0;
			}
		}
		if (nshift)
			nshift++;		/* eol code */

		ptr = top;
		top = ptr2 = alloc(STRLEN(top) + (nshift * 3 * 2/*safe*/) + 1);
		mode = JP_ASCII;
		while (*ptr)
		{
			if (sjis_islead(*ptr))
			{
				cp = JPdisp(&mode, JP_KANJI, code);
				for(; *cp;)
					*ptr2++ = *cp++;
				kanji = sjistojis(ptr[0], ptr[1]);
				*ptr2 ++ = (kanji & 0xff00) >> 8;
				*ptr2 ++ = kanji & 0xff;
				ptr += 2;
			}
			else if (sjis_iskana(*ptr))
			{
				cp = JPdisp(&mode, JP_KANA, code);
				for(; *cp;)
					*ptr2++ = *cp++;
				*ptr2 ++ = *ptr & 0x7f;
				ptr ++;
			}
			else
			{
				cp = JPdisp(&mode, JP_ASCII, code);
				for(; *cp;)
					*ptr2++ = *cp++;
				*ptr2 ++ = *ptr ++;
			}
		}
		cp = JPdisp(&mode, JP_ASCII, code);
		for(; *cp;)
			*ptr2++ = *cp++;
		*ptr2 = NUL;
		return top;
	}
	return top;
}

	char *
fileconvsfrom(char_u *org)
{
	static char_u	fnamebuf[2][MAXPATHL];
	static int		cnt = 0;
	char_u		*	fname;
	int				jkc = p_jkc;
#ifdef MSDOS
	char_u		*	p;
	char_u		*	t;
	int				cygnus = FALSE;
#endif

	if (org == NULL)
		return(NULL);
	p_jkc = FALSE;
	fname = &fnamebuf[++cnt & 1][0];
	{
		/*
		 * kanjiconvsfrom() returns -1 when the name does not fit, and using
		 * that as an index wrote a NUL in front of the buffer and handed back
		 * whatever the last call had left in it. Give the name back unconverted
		 * instead: it is too long for anything downstream either way, and a
		 * caller that cannot open it says so.
		 */
		int		len = kanjiconvsfrom(org, strlen(org),
								fname, MAXPATHL - 1, NULL, FILECODE, NULL);

		if (len < 0)
		{
			STRNCPY(fname, org, (size_t)(MAXPATHL - 1));
			len = STRLEN(org) < (MAXPATHL - 1) ? (int)STRLEN(org) : MAXPATHL - 1;
		}
		fname[len] = NUL;
	}
#ifdef MSDOS
	t = p = (char_u *)fname;
	if (p[0] == '/' && p[1] == '/' && isalpha(p[2]) && p[3] == '/')
	{
		cygnus = TRUE;
		t[0] = p[2];
		t[1] = ':';
		t[2] = '/';
		memmove(&t[3], &p[4], strlen(p) - 3);
	}
	else if (strnicmp("/cygdrive/", p, 10) == 0 && isalpha(p[10]) && p[11] == '/')
	{
		cygnus = TRUE;
		t[0] = p[10];
		t[1] = ':';
		t[2] = '/';
		memmove(&t[3], &p[12], strlen(p) - 11);
	}
	while (*p)
	{
		if (ISkanji(*p))
		{
			int		n = utf_lenat(p, 0);

			while (n-- > 0)
				*t++ = *p++;
		}
		else if (cygnus && p[0] == '\\')
			p++;
		else if (p != (char_u *)fname && p[0] == '\\' && p[1] == '\\')
			p++;
		else
			*t++ = *p++;
	}
	*t = '\0';
#endif
	p_jkc = jkc;
	return(fname);
}

	char *
fileconvsto(char_u *org)
{
	static char		fnamebuf[2][MAXPATHL];
	static int		cnt = 0;
	char		*	fname;
	char_u		*	p;
#ifdef MSDOS
	char_u		*	t;
	int				cygnus = FALSE;
#endif

	if (org == NULL)
		return(NULL);
	fname = &fnamebuf[++cnt & 1][0];
#ifdef MSDOS
	/* a name longer than the buffer is not a name any of this can use, but it
	 * must not be written past the end of it either */
	STRNCPY(fname, org, (size_t)(MAXPATHL - 1));
	fname[MAXPATHL - 1] = NUL;
	t = p = (char_u *)fname;
	if (p[0] == '/' && p[1] == '/' && isalpha(p[2]) && p[3] == '/')
	{
		cygnus = TRUE;
		t[0] = p[2];
		t[1] = ':';
		t[2] = '/';
		memmove(&t[3], &p[4], strlen(p) - 3);
	}
	while (*p)
	{
		if (ISkanji(*p))
		{
			int		n = utf_lenat(p, 0);

			while (n-- > 0)
				*t++ = *p++;
		}
		else if (cygnus && p[0] == '\\')
			p++;
		else if (p != (char_u *)fname && p[0] == '\\' && p[1] == '\\')
			p++;
		else
			*t++ = *p++;
	}
	*t = '\0';
	org = fname;
#endif
#ifdef UCODE
	if (FILECODE == JP_UTF8)
	{
		/*
		 * Already the internal code, so there is nothing to convert:
		 * kanjiconvsto() would allocate a copy, copy into it, and then be copied
		 * out of and freed again for nothing. Every file name in the Windows
		 * build comes through here.
		 */
		if (org != (char_u *)fname)		/* the MSDOS block above copies it */
		{
			STRNCPY(fname, org, (size_t)(MAXPATHL - 1));
			fname[MAXPATHL - 1] = NUL;
		}
		return(fname);
	}
#endif
	p = kanjiconvsto(org, FILECODE, TRUE);
	if (p == NULL)
		return(NULL);
	STRNCPY(fname, p, (size_t)(MAXPATHL - 1));
	fname[MAXPATHL - 1] = NUL;
	free(p);
	return(fname);
}

	void
binaryconvsfrom(linenr_t lnum, char code, int *tail, char_u *ptr, int len, char_u *dst)
{
	char_u	*wk = ptr;
	int		i;

	memset(dst, '\0', 81);
	sprintf(dst, "%08lx: ", lnum * 16);
	dst += 10;

	for (i = 0; i < 16; ++i)
	{
		if (len <= i)
		{
			*dst++ = ' ';
			*dst++ = ' ';
		}
		else
		{
			sprintf(dst, "%02x", *wk);
			dst += 2;
			++wk;
		}
		if (i & 1)
			*dst++ = ' ';
	}
	*dst++ = ';';
	for (i = 0; i < len;)
	{
		if (*tail)
		{
			*tail = FALSE;
			goto normal;
		}
		switch (code) {
		case JP_EUC:
			if ((kanji_map_euc[*ptr] & 1) && (0xa1 <= ptr[1] && ptr[1] <= 0xfe))
			{
				*dst++ = (euctosjis(ptr[0], ptr[1]) & 0xff00) >> 8;
				*dst++ = euctosjis(ptr[0], ptr[1]) & 0xff;
				ptr += 2;
				i += 2;
				if (i > len)
					*tail = TRUE;
			}
			else if ((kanji_map_euc[*ptr] & 2) && (0xa1 <= ptr[1] && ptr[1] <= 0xdf))
			{
				*dst++ = '.';
				*dst++ = ptr[1];
				ptr += 2;
				i += 2;
				if (i > len)
					*tail = TRUE;
			}
			else
				goto normal;
			break;
		case JP_SJIS:
			if (sjis_islead(*ptr)
					&& ((0x40 <= ptr[1] && ptr[1] <= 0x7e) || (0x80 <= ptr[1] && ptr[1] <= 0xfc)))
			{
				*dst++ = *ptr++;
				*dst++ = *ptr++;
				i += 2;
				if (i > len)
					*tail = TRUE;
			}
			else if (sjis_iskana(*ptr))
			{
				*dst++ = *ptr++;
				i += 1;
			}
			else
				goto normal;
			break;
#ifdef NT
		case JP_WIDE:		/* UNICODE */
			{
				int			wlen;
				char_u		buf[2];
				int			nochar;

				if (ptr[0] == 0xfe && ptr[1] == 0xff)
				{
					*tail = TRUE;
					goto normal;
				}
				wlen = WideCharToMultiByte(p_cpage, WC_COMPOSITECHECK, (LPCWSTR)ptr, 1, (LPSTR)buf, 2, NULL, &nochar);
				if (nochar)
					goto normal;
				if (wlen == 2)
				{
					*dst++ = buf[0];
					*dst++ = buf[1];
				}
				else if (buf[0] < ' ')
				{
					*dst++ = '.';
					*dst++ = '.';
				}
				else
				{
					*dst++ = buf[0];
					*dst++ = '.';
				}
				ptr += 2;
				i += 2;
				if (i > len)
					*tail = TRUE;
			}
			break;
#endif
		default:
normal:
			if (*ptr < ' ' || 0x7f <= *ptr)
				*dst++ = '.';
			else
				*dst++ = *ptr;
			ptr++;
			i++;
			break;
		}
	}
}

	char_u *
binaryconvsto(char code, char_u *ptr, int *len, int ubig)
{
	char_u				value = 0;
	int					cnt = 0;
	char_u				*top;
	int					quote = FALSE;

	*len = 0;
	if ((strchr(ptr, ':') > strchr(ptr, '"')) && strchr(ptr, '"') != NULL)
		;
	else if ((top = strchr(ptr, ':')) != NULL)
		ptr = top + 1;
	top = alloc(STRLEN(ptr) + 1);

	while (*ptr)
	{
		if (quote)
		{
			if (*ptr == '"')
				quote = FALSE;
			else if (ptr[0] == '\\' && ptr[1] == '"')
			{
				top[(*len)++] = '"';
				ptr++;
			}
			else if (code == JP_JIS)
				top[(*len)++] = *ptr;
			else
			{
				char_u		buf[UTF8_MAXLEN + 1];
				char_u		*tmpptr;
				char_u		*p;

				if (ISkanji(*ptr))
				{
					int		n = utf_lenat(ptr, 0);
					int		k;

					for (k = 0; k < n; k++)
						buf[k] = ptr[k];
					buf[n] = NUL;
					p = tmpptr = kanjiconvsto(buf, code, ubig);
					while (*p)
						top[(*len)++] = *p++;
					free(tmpptr);
					ptr += n - 1;
				}
				else
					top[(*len)++] = *ptr;
			}
		}
		else
		{
			switch (*ptr) {
			case '0': case '1': case '2': case '3': case '4':
			case '5': case '6': case '7': case '8': case '9':
				value = (value << 4) + (*ptr - '0');
				cnt++;
				break;
			case 'a': case 'A':
				value = (value << 4) + 10
								+ (*ptr == 'a' ? *ptr - 'a' : *ptr - 'A');
				cnt++;
				break;
			case 'b': case 'B':
				value = (value << 4) + 10
								+ (*ptr == 'b' ? *ptr - 'a' : *ptr - 'A');
				cnt++;
				break;
			case 'c': case 'C':
				value = (value << 4) + 10
								+ (*ptr == 'c' ? *ptr - 'a' : *ptr - 'A');
				cnt++;
				break;
			case 'd': case 'D':
				value = (value << 4) + 10
								+ (*ptr == 'd' ? *ptr - 'a' : *ptr - 'A');
				cnt++;
				break;
			case 'e': case 'E':
				value = (value << 4) + 10
								+ (*ptr == 'e' ? *ptr - 'a' : *ptr - 'A');
				cnt++;
				break;
			case 'f': case 'F':
				value = (value << 4) + 10
								+ (*ptr == 'f' ? *ptr - 'a' : *ptr - 'A');
				cnt++;
				break;
			case ';':
				if (cnt == 1)
				{
					top[(*len)++] = value;
					cnt = 0;
					value = 0;
				}
				return(top);
			case '"':
				if (cnt == 1)
				{
					top[(*len)++] = value;
					cnt = 0;
					value = 0;
				}
				quote = TRUE;
				break;
			case ' ':
			case '\t':
			case '\n':
			case '\r':
			default:
				if (cnt == 1)
				{
					top[(*len)++] = value;
					cnt = 0;
					value = 0;
				}
				break;
			}
			if (cnt == 2)
			{
				top[(*len)++] = value;
				cnt = 0;
				value = 0;
			}
		}
		ptr++;
	}
	return(top);
}

/*
 * JIS X 0201 Right hand set(hankaku kana) <-> JIS X 0208(zenkaku)
 *
 */
	static int
jisx0201rto0208(char_u src0, char_u src1, char_u *dst0, char_u *dst1)
{
	char_u	c, y;
	char_u	*x0201p, z;
	int		conv;

	src0 |= 0x80;
	src1 |= 0x80;
	c = (char_u)src0;
	x0201p = (char_u *)jisx0201r + 2 * (c - 0xa0);

	if (! sjis_islead(y = *x0201p))
	{
		*dst0 = y;
		*dst1 = NUL;
		return FALSE;
	}

	z = *(x0201p + 1);
	conv = FALSE;
	if		((char_u)src1 == 0xdf &&		/* maru */
				(c >= 0xca && c <= 0xce))	/* ha - ho */
	{
		z += 2;
		conv = TRUE;
	}
	else if ((char_u)src1 == 0xde)			/* dakuten */
	{
		conv = TRUE;
		if (   (c >= 0xb6 && c <= 0xc4)		/* ka - to */
			|| (c >= 0xca && c <= 0xce) )	/* ha - ho */
			z ++;
		else if (c == 0xb3)					/* u -> vu*/
			z = 0x94;
		else
			conv = FALSE;
	}

	*dst0 = y;
	*dst1 = z;

	return conv ? TRUE : FALSE;
}

/*
 * compare two strings, ignoring case
 * return 0 for match, 1 for difference
 */
/*
 * Halfwidth forms U+FF61..U+FF9F folded to their fullwidth equivalent
 * (the NFKC mapping). Used by the Japanese insensitive compare below.
 */
static short	halfwidth_fold[] = {
	0x3002, 0x300c, 0x300d, 0x3001, 0x30fb, 0x30f2, 0x30a1, 0x30a3,
	0x30a5, 0x30a7, 0x30a9, 0x30e3, 0x30e5, 0x30e7, 0x30c3, 0x30fc,
	0x30a2, 0x30a4, 0x30a6, 0x30a8, 0x30aa, 0x30ab, 0x30ad, 0x30af,
	0x30b1, 0x30b3, 0x30b5, 0x30b7, 0x30b9, 0x30bb, 0x30bd, 0x30bf,
	0x30c1, 0x30c4, 0x30c6, 0x30c8, 0x30ca, 0x30cb, 0x30cc, 0x30cd,
	0x30ce, 0x30cf, 0x30d2, 0x30d5, 0x30d8, 0x30db, 0x30de, 0x30df,
	0x30e0, 0x30e1, 0x30e2, 0x30e4, 0x30e6, 0x30e8, 0x30e9, 0x30ea,
	0x30eb, 0x30ec, 0x30ed, 0x30ef, 0x30f3, 0x3099, 0x309a,
};

/*
 * Katakana that take a voiced or semi voiced mark. The voiced form is the next
 * code point, the semi voiced one the one after that.
 */
	static int
jp_voiced(int cp, int semi)
{
	if (semi)
	{
		if (cp >= 0x30cf && cp <= 0x30db && ((cp - 0x30cf) % 3) == 0)
			return cp + 2;					/* ha..ho */
		return 0;
	}
	if (cp == 0x30a6)						/* u -> vu */
		return 0x30f4;
	if (cp == 0x30ef)						/* wa -> va */
		return 0x30f7;
	if ((cp >= 0x30ab && cp <= 0x30c8 && ((cp - 0x30ab) % 2) == 0)
			|| (cp >= 0x30cf && cp <= 0x30db && ((cp - 0x30cf) % 3) == 0))
		return cp + 1;						/* ka..to, ha..ho */
	return 0;
}

/*
 * Fold the character at *pp for the Japanese case and width insensitive
 * compare, advancing *pp past everything it consumed:
 *
 *	ASCII lower case	-> upper case
 *	fullwidth ASCII		-> plain ASCII, then upper case
 *	hiragana			-> katakana
 *	halfwidth katakana	-> fullwidth, combining a following sound mark
 */
	static int
jp_foldstep(char_u **pp)
{
	char_u	*p = *pp;
	int		cp;
	int		len;

	cp = utf_decode(p, &len);
	*pp = p + (len < 1 ? 1 : len);
	if (cp == UTF8_ERROR)
		return *p;
	if (cp >= 'a' && cp <= 'z')
		return cp - 'a' + 'A';
	if (cp >= 0xff01 && cp <= 0xff5e)		/* fullwidth ASCII */
	{
		cp = cp - 0xff01 + '!';
		return (cp >= 'a' && cp <= 'z') ? cp - 'a' + 'A' : cp;
	}
	if (cp >= 0x3041 && cp <= 0x3096)		/* hiragana -> katakana */
		return cp + 0x60;
	if (cp >= 0xff61 && cp <= 0xff9f)
	{
		int		full = halfwidth_fold[cp - 0xff61];
		int		next;
		int		nlen;
		int		mark;

		next = utf_decode(*pp, &nlen);
		if (next == 0xff9e || next == 0xff9f)
		{
			mark = jp_voiced(full, next == 0xff9f);
			if (mark != 0)
			{
				*pp += nlen;
				return mark;
			}
		}
		return full;
	}
	return cp;
}

/*
 * Fold a single code point the same way jp_foldstep() folds a character: case,
 * kana width, hiragana to katakana. Used by the regexp engine, which compares
 * code points rather than byte sequences.
 */
	int
jp_foldcp(int cp)
{
	char_u	buf[UTF8_MAXLEN + 1];
	char_u	*p = buf;
	int		len;

	len = utf_encode(cp, buf);
	buf[len] = NUL;
	return jp_foldstep(&p);
}

/*
 * How many bytes at the start of 's1' match the first 'len' bytes of 's2',
 * ignoring case, kana width and hiragana/katakana? 0 when they do not match.
 * Tab and space are considered equal, as they were before.
 */
	int
jp_strnicmp(char_u *s1, char_u *s2, size_t len)
{
	char_u	*p1 = s1;
	char_u	*p2 = s2;
	long	rest = (long)len;
	int		tlen = 0;

	while (rest > 0)
	{
		char_u	*q1 = p1;
		char_u	*q2 = p2;
		int		c1, c2;

		if (*p2 == NUL)
			return tlen;
		if (*p1 == NUL)
			return 0;
		c1 = jp_foldstep(&p1);
		c2 = jp_foldstep(&p2);
		if (c1 != c2 && !((*q1 == '\t' && *q2 == ' ')
						|| (*q1 == ' ' && *q2 == '\t')))
			return 0;
		tlen += (int)(p1 - q1);
		rest -= (long)(p2 - q2);
	}
	return tlen;
}

#ifdef UCODE
int ucs2sjis __ARGS((unsigned short ucs, unsigned char* sjis));
void sjis2ucs __ARGS((unsigned char* sjis, int len, unsigned char* ucs));

	int
wide2multi(char_u *ptr, int size, int ubig, int first)
{
	char_u	*	p = ptr;
	char_u		buf[2];
	int			len;
	int			total = 0;
	int			i;

	if (first)
	{
		p += 2;
		size -= 2;
	}
	for (i = 0; i < size; i += 2)
	{
		if (ubig)
		{
			buf[0]	= p[0];
			p[0]	= p[1];
			p[1]	= buf[0];
		}
#ifdef NT
		len = WideCharToMultiByte(p_cpage, WC_COMPOSITECHECK, (LPCWSTR)p, 1, (LPSTR)buf, 2, NULL, NULL);
#else
		len = ucs2sjis((*(p+1) << 8) | *p, buf);
#endif
		memmove((char *)&ptr[total], buf, len);
		p += 2;
		total += len;
	}
	return(total);
}

	void
multi2wide(char_u *k1, char_u *k2, int len, int ubig)
{
	char_u	buf[2];
	char_u	wbuf[2/*sizeof(WCHAR)*/];

	buf[0] = *k1;
	buf[1] = *k2;
#ifdef NT
	MultiByteToWideChar(p_cpage, MB_PRECOMPOSED, (LPSTR)buf, len, (LPWSTR)wbuf, 1);
#else
	sjis2ucs(buf, len, wbuf);
#endif
	if (ubig)
	{
		*k1 = wbuf[1];
		*k2 = wbuf[0];
	}
	else
	{
		*k1 = wbuf[0];
		*k2 = wbuf[1];
	}
}
#endif /* UCODE */
#endif	/* KANJI */
