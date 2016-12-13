/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * (C) Copyright 2016 linuxdaemon / linuxdemon1 <walter@walterbarnes.net>
 *     - Fixed pointer dereference issues in the score calculations
 *     - Rewrote the consonant/vowel switches to use strchr()
 *     - Changed the calculation function to use std::string instead of a c-ctring
 *     - Migrated to using a std::map rather than blindly iterating over a whole array
 * (C) Copyright 2013 SimosNap IRC Network <admin@simosnap.org>
 *                    lnx85 <lnx85@lnxlabs.it>
 *     - Added exempt support (nick, ident, host and fullname based)
 * (C) Copyright 2011 Syloq <syloq@nightstar.net>
 *     - Renamed failedconnects to showfailedconnects
 *     - Added example config and notes
 *     - Added defaults to all parameters
 * (C) Copyright 2008 Robin Burchell <w00t@inspircd.org>
 * (C) Copyright 2004-2005, Bram Matthys (Syzop) <syzop@vulnscan.org>
 *     - Contains ideas from Keith Dunnett <keith@dunnett.org>
 *     - Most of the detection mechanisms come from SpamAssassin FVGT_Tripwire.
 * ---------------------------------------------------
 * Example config:
   <antirandom
        showfailedconnects="1" (Defaults to 1 )
        debugmode="0" (Defaults to 0)
        threshold="10" (Defaults to 10 Valid values between 1 and 100)
        banaction="ZLINE" (Defaults to "" Valid values: GLINE,ZLINE,KILL,"")
        banduration="86400" (Defaults to 86400 (1 day). Time in seconds)
        banreason="You look like a bot. Change your nick/ident/gecos and try reconnecting.">
   <antirandomexempt type="host" pattern="*.tld">
   <antirandomexempt type="ident" pattern="*lightirc">
   <antirandomexempt type="fullname" pattern="Mibbit">

   Notes:
        showfailedconnects - Show failed connection
        This module uses a scoring system that will react based
         on the threshold set above. If the threshold is set too low many
         clients will be tagged a bot.
 */

#include "inspircd.h"
#include "xline.h"

/* $ModDesc: A module to prevent against bots using random patterns. */
/* $ModAuthor: lnx85 */
/* $ModAuthorMail: lnx85@lnxlabs.it */
/* $ModDepends: core 2.0 */
/* $CompileFlags: -std=c++11 */

#define ANTIRANDOM_ACT_KILL     0
#define ANTIRANDOM_ACT_ZLINE    1
#define ANTIRANDOM_ACT_GLINE    2
#define ANTIRANDOM_ACT_NONE     3

enum AntirandomExemptType { NICK, IDENT, HOST, FULLNAME };

class AntirandomExempt
{
public:
	AntirandomExemptType type;
	std::string pattern;

	AntirandomExempt(AntirandomExemptType t, const std::string &p)
	: type(t), pattern(p)
	{
	}
};
typedef std::vector<AntirandomExempt> AntirandomExemptList;

// TODO Rewrite this to use a std::map rather than an array
// Would remove the need to iterate over the full thing
static const char *triples_txt[] = {
	"aj", "fqtvxz",
	"aq", "deghjkmnprtxyz",
	"av", "bfhjqwxz",
	"az", "jwx",
	"bd", "bghjkmpqvxz",
	"bf", "bcfgjknpqvwxyz",
	"bg", "bdfghjkmnqstvxz",
	"bh", "bfhjkmnqvwxz",
	"bj", "bcdfghjklmpqtvwxyz",
	"bk", "dfjkmqrvwxyz",
	"bl", "bgpqwxz",
	"bm", "bcdflmnqz",
	"bn", "bghjlmnpqtvwx",
	"bp", "bfgjknqvxz",
	"bq", "bcdefghijklmnopqrstvwxyz",
	"bt", "dgjkpqtxz",
	"bv", "bfghjklnpqsuvwxz",
	"bw", "bdfjknpqsuwxyz",
	"bx", "abcdfghijklmnopqtuvwxyz",
	"bz", "bcdfgjklmnpqrstvwxz",
	"cb", "bfghjkpqyz",
	"cc", "gjqxz",
	"cd", "hjkqvwxz",
	"cf", "gjknqvwyz",
	"cg", "bdfgjkpqvz",
	"cl", "fghjmpqxz",
	"cm", "bjkqv",
	"cn", "bghjkpqwxz",
	"cp", "gjkvxyz",
	"cq", "abcdefghijklmnopqsvwxyz",
	"cr", "gjqx",
	"cs", "gjxz",
	"cv", "bdfghjklmnquvwxyz",
	"cx", "abdefghjklmnpqrstuvwxyz",
	"cy", "jqy",
	"cz", "bcdfghjlpqrtvwxz",
	"db", "bdgjnpqtxz",
	"dc", "gjqxz",
	"dd", "gqz",
	"df", "bghjknpqvxyz",
	"dg", "bfgjqvxz",
	"dh", "bfkmnqwxz",
	"dj", "bdfghjklnpqrwxz",
	"dk", "cdhjkpqrtuvwxz",
	"dl", "bfhjknqwxz",
	"dm", "bfjnqw",
	"dn", "fgjkmnpqvwz",
	"dp", "bgjkqvxz",
	"dq", "abcefghijkmnopqtvwxyz",
	"dr", "bfkqtvx",
	"dt", "qtxz",
	"dv", "bfghjknqruvwyz",
	"dw", "cdfjkmnpqsvwxz",
	"dx", "abcdeghjklmnopqrsuvwxyz",
	"dy", "jyz",
	"dz", "bcdfgjlnpqrstvxz",
	"eb", "jqx",
	"eg", "cjvxz",
	"eh", "hxz",
	"ej", "fghjpqtwxyz",
	"ek", "jqxz",
	"ep", "jvx",
	"eq", "bcghijkmotvxyz",
	"ev", "bfpq",
	"fc", "bdjkmnqvxz",
	"fd", "bgjklqsvyz",
	"fg", "fgjkmpqtvwxyz",
	"fh", "bcfghjkpqvwxz",
	"fj", "bcdfghijklmnpqrstvwxyz",
	"fk", "bcdfghjkmpqrstvwxz",
	"fl", "fjkpqxz",
	"fm", "dfhjlmvwxyz",
	"fn", "bdfghjklnqrstvwxz",
	"fp", "bfjknqtvwxz",
	"fq", "abcefghijklmnopqrstvwxyz",
	"fr", "nqxz",
	"fs", "gjxz",
	"ft", "jqx",
	"fv", "bcdfhjklmnpqtuvwxyz",
	"fw", "bcfgjklmpqstuvwxyz",
	"fx", "bcdfghjklmnpqrstvwxyz",
	"fy", "ghjpquvxy",
	"fz", "abcdfghjklmnpqrtuvwxyz",
	"gb", "bcdknpqvwx",
	"gc", "gjknpqwxz",
	"gd", "cdfghjklmqtvwxz",
	"gf", "bfghjkmnpqsvwxyz",
	"gg", "jkqvxz",
	"gj", "bcdfghjklmnpqrstvwxyz",
	"gk", "bcdfgjkmpqtvwxyz",
	"gl", "fgjklnpqwxz",
	"gm", "dfjkmnqvxz",
	"gn", "jkqvxz",
	"gp", "bjknpqtwxyz",
	"gq", "abcdefghjklmnopqrsvwxyz",
	"gr", "jkqt",
	"gt", "fjknqvx",
	"gu", "qwx",
	"gv", "bcdfghjklmpqstvwxyz",
	"gw", "bcdfgjknpqtvwxz",
	"gx", "abcdefghjklmnopqrstvwxyz",
	"gy", "jkqxy",
	"gz", "bcdfgjklmnopqrstvxyz",
	"hb", "bcdfghjkqstvwxz",
	"hc", "cjknqvwxz",
	"hd", "fgjnpvz",
	"hf", "bfghjkmnpqtvwxyz",
	"hg", "bcdfgjknpqsxyz",
	"hh", "bcgklmpqrtvwxz",
	"hj", "bcdfgjkmpqtvwxyz",
	"hk", "bcdgkmpqrstvwxz",
	"hl", "jxz",
	"hm", "dhjqrvwxz",
	"hn", "jrxz",
	"hp", "bjkmqvwxyz",
	"hq", "abcdefghijklmnopqrstvwyz",
	"hr", "cjqx",
	"hs", "jqxz",
	"hv", "bcdfgjklmnpqstuvwxz",
	"hw", "bcfgjklnpqsvwxz",
	"hx", "abcdefghijklmnopqrstuvwxyz",
	"hz", "bcdfghjklmnpqrstuvwxz",
	"ib", "jqx",
	"if", "jqvwz",
	"ih", "bgjqx",
	"ii", "bjqxy",
	"ij", "cfgqxy",
	"ik", "bcfqx",
	"iq", "cdefgjkmnopqtvxyz",
	"iu", "hiwxy",
	"iv", "cfgmqx",
	"iw", "dgjkmnpqtvxz",
	"ix", "jkqrxz",
	"iy", "bcdfghjklpqtvwx",
	"jb", "bcdghjklmnopqrtuvwxyz",
	"jc", "cfgjkmnopqvwxy",
	"jd", "cdfghjlmnpqrtvwx",
	"jf", "abcdfghjlnopqrtuvwxyz",
	"jg", "bcdfghijklmnopqstuvwxyz",
	"jh", "bcdfghjklmnpqrxyz",
	"jj", "bcdfghjklmnopqrstuvwxyz",
	"jk", "bcdfghjknqrtwxyz",
	"jl", "bcfghjmnpqrstuvwxyz",
	"jm", "bcdfghiklmnqrtuvwyz",
	"jn", "bcfjlmnpqsuvwxz",
	"jp", "bcdfhijkmpqstvwxyz",
	"jq", "abcdefghijklmnopqrstuvwxyz",
	"jr", "bdfhjklpqrstuvwxyz",
	"js", "bfgjmoqvxyz",
	"jt", "bcdfghjlnpqrtvwxz",
	"jv", "abcdfghijklpqrstvwxyz",
	"jw", "bcdefghijklmpqrstuwxyz",
	"jx", "abcdefghijklmnopqrstuvwxyz",
	"jy", "bcdefghjkpqtuvwxyz",
	"jz", "bcdfghijklmnopqrstuvwxyz",
	"kb", "bcdfghjkmqvwxz",
	"kc", "cdfgjknpqtwxz",
	"kd", "bfghjklmnpqsvwxyz",
	"kf", "bdfghjkmnpqsvwxyz",
	"kg", "cghjkmnqtvwxyz",
	"kh", "cfghjkqx",
	"kj", "bcdfghjkmnpqrstwxyz",
	"kk", "bcdfgjmpqswxz",
	"kl", "cfghlmqstwxz",
	"km", "bdfghjknqrstwxyz",
	"kn", "bcdfhjklmnqsvwxz",
	"kp", "bdfgjkmpqvxyz",
	"kq", "abdefghijklmnopqrstvwxyz",
	"kr", "bcdfghjmqrvwx",
	"ks", "jqx",
	"kt", "cdfjklqvx",
	"ku", "qux",
	"kv", "bcfghjklnpqrstvxyz",
	"kw", "bcdfgjklmnpqsvwxz",
	"kx", "abcdefghjklmnopqrstuvwxyz",
	"ky", "vxy",
	"kz", "bcdefghjklmnpqrstuvwxyz",
	"lb", "cdgkqtvxz",
	"lc", "bqx",
	"lg", "cdfgpqvxz",
	"lh", "cfghkmnpqrtvx",
	"lk", "qxz",
	"ln", "cfjqxz",
	"lp", "jkqxz",
	"lq", "bcdefhijklmopqrstvwxyz",
	"lr", "dfgjklmpqrtvwx",
	"lv", "bcfhjklmpwxz",
	"lw", "bcdfgjknqxz",
	"lx", "bcdfghjklmnpqrtuwz",
	"lz", "cdjptvxz",
	"mb", "qxz",
	"md", "hjkpvz",
	"mf", "fkpqvwxz",
	"mg", "cfgjnpqsvwxz",
	"mh", "bchjkmnqvx",
	"mj", "bcdfghjknpqrstvwxyz",
	"mk", "bcfgklmnpqrvwxz",
	"ml", "jkqz",
	"mm", "qvz",
	"mn", "fhjkqxz",
	"mq", "bdefhjklmnopqtwxyz",
	"mr", "jklqvwz",
	"mt", "jkq",
	"mv", "bcfghjklmnqtvwxz",
	"mw", "bcdfgjklnpqsuvwxyz",
	"mx", "abcefghijklmnopqrstvwxyz",
	"mz", "bcdfghjkmnpqrstvwxz",
	"nb", "hkmnqxz",
	"nf", "bghqvxz",
	"nh", "fhjkmqtvxz",
	"nk", "qxz",
	"nl", "bghjknqvwxz",
	"nm", "dfghjkqtvwxz",
	"np", "bdjmqwxz",
	"nq", "abcdfghjklmnopqrtvwxyz",
	"nr", "bfjkqstvx",
	"nv", "bcdfgjkmnqswxz",
	"nw", "dgjpqvxz",
	"nx", "abfghjknopuyz",
	"nz", "cfqrxz",
	"oc", "fjvw",
	"og", "qxz",
	"oh", "fqxz",
	"oj", "bfhjmqrswxyz",
	"ok", "qxz",
	"oq", "bcdefghijklmnopqrstvwxyz",
	"ov", "bfhjqwx",
	"oy", "qxy",
	"oz", "fjpqtvx",
	"pb", "fghjknpqvwz",
	"pc", "gjq",
	"pd", "bgjkvwxz",
	"pf", "hjkmqtvwyz",
	"pg", "bdfghjkmqsvwxyz",
	"ph", "kqvx",
	"pk", "bcdfhjklmpqrvx",
	"pl", "ghkqvwx",
	"pm", "bfhjlmnqvwyz",
	"pn", "fjklmnqrtvwz",
	"pp", "gqwxz",
	"pq", "abcdefghijklmnopqstvwxyz",
	"pr", "hjkqrwx",
	"pt", "jqxz",
	"pv", "bdfghjklquvwxyz",
	"pw", "fjkmnpqsuvwxz",
	"px", "abcdefghijklmnopqrstuvwxyz",
	"pz", "bdefghjklmnpqrstuvwxyz",
	"qa", "ceghkopqxy",
	"qb", "bcdfghjklmnqrstuvwxyz",
	"qc", "abcdfghijklmnopqrstuvwxyz",
	"qd", "defghijklmpqrstuvwxyz",
	"qe", "abceghjkmopquwxyz",
	"qf", "abdfghijklmnopqrstuvwxyz",
	"qg", "abcdefghijklmnopqrtuvwxz",
	"qh", "abcdefghijklmnopqrstuvwxyz",
	"qi", "efgijkmpwx",
	"qj", "abcdefghijklmnopqrstuvwxyz",
	"qk", "abcdfghijklmnopqrsuvwxyz",
	"ql", "abcefghjklmnopqrtuvwxyz",
	"qm", "bdehijklmnoqrtuvxyz",
	"qn", "bcdefghijklmnoqrtuvwxyz",
	"qo", "abcdefgijkloqstuvwxyz",
	"qp", "abcdefghijkmnopqrsuvwxyz",
	"qq", "bcdefghijklmnopstwxyz",
	"qr", "bdefghijklmnoqruvwxyz",
	"qs", "bcdefgijknqruvwxz",
	"qt", "befghjklmnpqtuvwxz",
	"qu", "cfgjkpwz",
	"qv", "abdefghjklmnopqrtuvwxyz",
	"qw", "bcdfghijkmnopqrstuvwxyz",
	"qx", "abcdefghijklmnopqrstuvwxyz",
	"qy", "abcdefghjklmnopqrstuvwxyz",
	"qz", "abcdefghijklmnopqrstuvwxyz",
	"rb", "fxz",
	"rg", "jvxz",
	"rh", "hjkqrxz",
	"rj", "bdfghjklmpqrstvwxz",
	"rk", "qxz",
	"rl", "jnq",
	"rp", "jxz",
	"rq", "bcdefghijklmnopqrtvwxy",
	"rr", "jpqxz",
	"rv", "bcdfghjmpqrvwxz",
	"rw", "bfgjklqsvxz",
	"rx", "bcdfgjkmnopqrtuvwxz",
	"rz", "djpqvxz",
	"sb", "kpqtvxz",
	"sd", "jqxz",
	"sf", "bghjkpqw",
	"sg", "cgjkqvwxz",
	"sj", "bfghjkmnpqrstvwxz",
	"sk", "qxz",
	"sl", "gjkqwxz",
	"sm", "fkqwxz",
	"sn", "dhjknqvwxz",
	"sq", "bfghjkmopstvwxz",
	"sr", "jklqrwxz",
	"sv", "bfhjklmnqtwxyz",
	"sw", "jkpqvwxz",
	"sx", "bcdefghjklmnopqrtuvwxyz",
	"sy", "qxy",
	"sz", "bdfgjpqsvxz",
	"tb", "cghjkmnpqtvwx",
	"tc", "jnqvx",
	"td", "bfgjkpqtvxz",
	"tf", "ghjkqvwyz",
	"tg", "bdfghjkmpqsx",
	"tj", "bdfhjklmnpqstvwxyz",
	"tk", "bcdfghjklmpqvwxz",
	"tl", "jkqwxz",
	"tm", "bknqtwxz",
	"tn", "fhjkmqvwxz",
	"tp", "bjpqvwxz",
	"tq", "abdefhijklmnopqrstvwxyz",
	"tr", "gjqvx",
	"tv", "bcfghjknpquvwxz",
	"tw", "bcdfjknqvz",
	"tx", "bcdefghjklmnopqrsuvwxz",
	"tz", "jqxz",
	"uc", "fjmvx",
	"uf", "jpqvx",
	"ug", "qvx",
	"uh", "bcgjkpvxz",
	"uj", "wbfghklmqvwx",
	"uk", "fgqxz",
	"uq", "bcdfghijklmnopqrtwxyz",
	"uu", "fijkqvwyz",
	"uv", "bcdfghjkmpqtwxz",
	"uw", "dgjnquvxyz",
	"ux", "jqxz",
	"uy", "jqxyz",
	"uz", "fgkpqrx",
	"vb", "bcdfhijklmpqrtuvxyz",
	"vc", "bgjklnpqtvwxyz",
	"vd", "bdghjklnqvwxyz",
	"vf", "bfghijklmnpqtuvxz",
	"vg", "bcdgjkmnpqtuvwxyz",
	"vh", "bcghijklmnpqrtuvwxyz",
	"vj", "abcdfghijklmnpqrstuvwxyz",
	"vk", "bcdefgjklmnpqruvwxyz",
	"vl", "hjkmpqrvwxz",
	"vm", "bfghjknpquvxyz",
	"vn", "bdhjkmnpqrtuvwxz",
	"vp", "bcdeghjkmopqtuvwyz",
	"vq", "abcdefghijklmnopqrstvwxyz",
	"vr", "fghjknqrtvwxz",
	"vs", "dfgjmqz",
	"vt", "bdfgjklmnqtx",
	"vu", "afhjquwxy",
	"vv", "cdfghjkmnpqrtuwxz",
	"vw", "abcdefghijklmnopqrtuvwxyz",
	"vx", "abcefghjklmnopqrstuvxyz",
	"vy", "oqx",
	"vz", "abcdefgjklmpqrstvwxyz",
	"wb", "bdfghjpqtvxz",
	"wc", "bdfgjkmnqvwx",
	"wd", "dfjpqvxz",
	"wf", "cdghjkmqvwxyz",
	"wg", "bcdfgjknpqtvwxyz",
	"wh", "cdghjklpqvwxz",
	"wj", "bfghijklmnpqrstvwxyz",
	"wk", "cdfgjkpqtuvxz",
	"wl", "jqvxz",
	"wm", "dghjlnqtvwxz",
	"wp", "dfgjkpqtvwxz",
	"wq", "abcdefghijklmnopqrstvwxyz",
	"wr", "cfghjlmpqwx",
	"wt", "bdgjlmnpqtvx",
	"wu", "aikoquvwy",
	"wv", "bcdfghjklmnpqrtuvwxyz",
	"ww", "bcdgkpqstuvxyz",
	"wx", "abcdefghijklmnopqrstuvwxz",
	"wy", "jquwxy",
	"wz", "bcdfghjkmnopqrstuvwxz",
	"xa", "ajoqy",
	"xb", "bcdfghjkmnpqsvwxz",
	"xc", "bcdgjkmnqsvwxz",
	"xd", "bcdfghjklnpqstuvwxyz",
	"xf", "bcdfghjkmnpqtvwxyz",
	"xg", "bcdfghjkmnpqstvwxyz",
	"xh", "cdfghjkmnpqrstvwxz",
	"xi", "jkqy",
	"xj", "abcdefghijklmnopqrstvwxyz",
	"xk", "abcdfghjkmnopqrstuvwxyz",
	"xl", "bcdfghjklmnpqrvwxz",
	"xm", "bcdfghjknpqvwxz",
	"xn", "bcdfghjklmnpqrvwxyz",
	"xp", "bcfjknpqvxz",
	"xq", "abcdefghijklmnopqrstvwxyz",
	"xr", "bcdfghjklnpqrsvwyz",
	"xs", "bdfgjmnqrsvxz",
	"xt", "jkpqvwxz",
	"xu", "fhjkquwx",
	"xv", "bcdefghjklmnpqrsuvwxyz",
	"xw", "bcdfghjklmnpqrtuvwxyz",
	"xx", "bcdefghjkmnpqrstuwyz",
	"xy", "jxy",
	"xz", "abcdefghjklmnpqrstuvwxyz",
	"yb", "cfghjmpqtvwxz",
	"yc", "bdfgjmpqsvwx",
	"yd", "chjkpqvwx",
	"yf", "bcdghjmnpqsvwx",
	"yg", "cfjkpqtxz",
	"yh", "bcdfghjkpqx",
	"yi", "hjqwxy",
	"yj", "bcdfghjklmnpqrstvwxyz",
	"yk", "bcdfgpqvwxz",
	"ym", "dfgjqvxz",
	"yp", "bcdfgjkmqxz",
	"yq", "abcdefghijklmnopqrstvwxyz",
	"yr", "jqx",
	"yt", "bcfgjnpqx",
	"yv", "bcdfghjlmnpqstvwxz",
	"yw", "bfgjklmnpqstuvwxz",
	"yx", "bcdfghjknpqrstuvwxz",
	"yy", "bcdfghjklpqrstvwxz",
	"yz", "bcdfjklmnpqtvwx",
	"zb", "dfgjklmnpqstvwxz",
	"zc", "bcdfgjmnpqstvwxy",
	"zd", "bcdfghjklmnpqstvwxy",
	"zf", "bcdfghijkmnopqrstvwxyz",
	"zg", "bcdfgjkmnpqtvwxyz",
	"zh", "bcfghjlpqstvwxz",
	"zj", "abcdfghjklmnpqrstuvwxyz",
	"zk", "bcdfghjklmpqstvwxz",
	"zl", "bcdfghjlnpqrstvwxz",
	"zm", "bdfghjklmpqstvwxyz",
	"zn", "bcdfghjlmnpqrstuvwxz",
	"zp", "bcdfhjklmnpqstvwxz",
	"zq", "abcdefghijklmnopqrstvwxyz",
	"zr", "bcfghjklmnpqrstvwxyz",
	"zs", "bdfgjmnqrsuwxyz",
	"zt", "bcdfgjkmnpqtuvwxz",
	"zu", "ajqx",
	"zv", "bcdfghjklmnpqrstuvwxyz",
	"zw", "bcdfghjklmnpqrstuvwxyz",
	"zx", "abcdefghijklmnopqrstuvwxyz",
	"zy", "fxy",
	"zz", "cdfhjnpqrvx",
	NULL, NULL
};

static std::map<std::string, std::string> init_map(const char **a)
{
    std::map<std::string, std::string> m;
    const char **ci;
    for (ci = a; *ci; ci += 2) {
        m.insert(std::pair<std::string, std::string>(ci[0], ci[1]));
    }
    return m;
}

const static std::map<std::string, std::string> triples_map = init_map(triples_txt);

class ModuleAntiRandom : public Module
{
 private:
	bool ShowFailedConnects;
	bool DebugMode;
	unsigned int Threshold;
	unsigned int BanAction;
	unsigned int BanDuration;
	std::string BanReason;
	AntirandomExemptList Exempts;
 public:
	ModuleAntiRandom()
	{
		Implementation eventlist[] = { I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	virtual ~ModuleAntiRandom() { }

	virtual Version GetVersion()
	{
	    return Version("A module to prevent against bots using random patterns",VF_NONE);
    }

	unsigned int GetStringScore(const std::string &original_str)
	{
		unsigned int score = 0;

		unsigned int highest_vowels = 0;
		unsigned int highest_consonants = 0;
		unsigned int highest_digits = 0;

		unsigned int vowels = 0;
		unsigned int consonants = 0;
		unsigned int digits = 0;

		/* Fast digit/consonant/vowel checks... */
		for (const char &c : original_str)
		{
			if ((c >= '0') && (c <= '9'))
			{
				digits++;
			}
			else
			{
				if (digits > highest_digits)
					highest_digits = digits;
				digits = 0;
			}

			/* Check consonants */
            if (strchr("bcdfghjklmnpqrstvwxz", c))
            {
                consonants++;
            }
            else
            {
                if (consonants > highest_consonants)
                    highest_consonants = consonants;
                consonants = 0;
            }

			/* Check vowels */
            if (strchr("aeiou", c))
            {
                vowels++;
            }
            else
            {
                if (vowels > highest_vowels)
                    highest_vowels = vowels;
                vowels = 0;
            }
		}

		/* Now set up for our checks. */
		if (highest_digits > digits)
			digits = highest_digits;
		if (highest_consonants > consonants)
			consonants = highest_consonants;
		if (highest_vowels > vowels)
			vowels = highest_vowels;

		if (digits >= 5)
		{
			score += digits;
			if (this->DebugMode)
				ServerInstance->SNO->WriteGlobalSno('a', "m_antirandom: %s:MATCH digits", original_str.c_str());
		}
		if (vowels >= 4)
		{
			score += vowels;
			if (this->DebugMode)
				ServerInstance->SNO->WriteGlobalSno('a', "m_antirandom: %s:MATCH vowels", original_str.c_str());
		}
		if (consonants >= 4)
		{
			score += consonants;
			if (this->DebugMode)
				ServerInstance->SNO->WriteGlobalSno('a',  "m_antirandom: %s:MATCH consonants", original_str.c_str());
		}


		/*
		 * Now, do the triples checks. For each char in the string we're checking ...
		 */
        for (size_t i = 0; i < (original_str.length() - 2); i++)
        {
            std::map<std::string, std::string>::const_iterator trip;
            // Check whether the current and next characters are the first half of a triple, if so, check for the 3rd character in the second half
            if ((trip = triples_map.find(original_str.substr(i, 2))) != triples_map.end() &&
                    trip->second.find_first_of(original_str[i + 2]) != std::string::npos)
            {
                score++;
                if (this->DebugMode)
                    ServerInstance->SNO->WriteGlobalSno('a', "m_antirandom: %s:MATCH triple (%s:%c/%c/%c)",
                            original_str.c_str(), trip->second.c_str(), original_str[i], original_str[i + 1], original_str[i + 2]);
            }
        }
		return score;
	}

	unsigned int GetUserScore(User *user)
	{
		int nscore, uscore, gscore, score;
		struct timeval tv_alpha, tv_beta;

		gettimeofday(&tv_alpha, NULL);

		nscore = GetStringScore(user->nick);
		uscore = GetStringScore(user->ident);
		gscore = GetStringScore(user->fullname);
		score = nscore + uscore + gscore;

		gettimeofday(&tv_beta, NULL);
		if (this->DebugMode)
			ServerInstance->SNO->WriteGlobalSno('a', "m_antirandom Timing: %ld microseconds",
				((tv_beta.tv_sec - tv_alpha.tv_sec) * 1000000) + (tv_beta.tv_usec - tv_alpha.tv_usec));

		if (this->DebugMode)
			ServerInstance->SNO->WriteGlobalSno('a', "m_antirandom Got score: %d/%d/%d = %d", nscore, uscore, gscore, score);
		return score;
	}

	bool IsAntirandomExempt(User *user)
	{
		for (AntirandomExemptList::iterator iter = Exempts.begin(); iter != Exempts.end(); iter++)
		{
			switch (iter->type)
			{
				case NICK:
				{
					if (InspIRCd::Match(user->nick, iter->pattern, ascii_case_insensitive_map))
					{
						if (this->DebugMode)
							ServerInstance->SNO->WriteGlobalSno('a', "m_antirandom exempt: NICK (%s)", iter->pattern.c_str());
						return true;
					}
					break;
				}
				case IDENT:
				{
					if (InspIRCd::Match(user->ident, iter->pattern, ascii_case_insensitive_map))
					{
						if (this->DebugMode)
							ServerInstance->SNO->WriteGlobalSno('a', "m_antirandom exempt: IDENT (%s)", iter->pattern.c_str());
						return true;
					}
					break;
				}
				case HOST:
				{
					if (InspIRCd::Match(user->host, iter->pattern, ascii_case_insensitive_map) || InspIRCd::MatchCIDR(user->GetIPString(), iter->pattern, ascii_case_insensitive_map))
					{
						if (this->DebugMode)
							ServerInstance->SNO->WriteGlobalSno('a', "m_antirandom exempt: HOST (%s)", iter->pattern.c_str());
						return true;
					}
					break;
				}
				case FULLNAME:
				{
					if (InspIRCd::Match(user->fullname, iter->pattern, ascii_case_insensitive_map))
					{
						if (this->DebugMode)
							ServerInstance->SNO->WriteGlobalSno('a', "m_antirandom exempt: FULLNAME (%s)", iter->pattern.c_str());
						return true;
					}
					break;
				}
			}
		}
		return false;
	}

	ModResult OnUserRegister(LocalUser* user)
	{
		if (IsAntirandomExempt(user))
		{
			return MOD_RES_PASSTHRU;
		}

		unsigned int score = GetUserScore(user);

		if (score > this->Threshold)
		{
			std::string method = "allowed because no action was set";

			switch (this->BanAction)
			{
				case ANTIRANDOM_ACT_KILL:
				{
					ServerInstance->Users->QuitUser(user, this->BanReason);
					method="Killed";
					break;
				}
				case ANTIRANDOM_ACT_ZLINE:
				{
					ZLine* zl = new ZLine(ServerInstance->Time(), this->BanDuration, ServerInstance->Config->ServerName, this->BanReason.c_str(), user->GetIPString());
                	if (ServerInstance->XLines->AddLine(zl,user))
                		ServerInstance->XLines->ApplyLines();
                	else
                		delete zl;
                	method="Z-Lined";
					break;
				}
				case ANTIRANDOM_ACT_GLINE:
				{
					GLine* gl = new GLine(ServerInstance->Time(), this->BanDuration, ServerInstance->Config->ServerName, this->BanReason.c_str(), "*", user->GetIPString());
                	if (ServerInstance->XLines->AddLine(gl,user))
                		ServerInstance->XLines->ApplyLines();
                	else
                		delete gl;
                	method="G-Lined";
					break;
				}
			}
			if (this->ShowFailedConnects)
			{
			    std::string realhost = user->GetFullRealHost();
				ServerInstance->SNO->WriteGlobalSno('a', "Connection from %s (%s) was %s by m_antirandom with a score of %d - which exceeds threshold of %d", realhost.c_str(), user->GetIPString(), method.c_str(), score, this->Threshold);

				ServerInstance->Logs->Log("CONFIG",DEFAULT, "Connection from %s (%s) was %s by m_antirandom with a score of %d - which exceeds threshold of %d", realhost.c_str(), user->GetIPString(), method.c_str(), score, this->Threshold);
			}
			return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	virtual void OnRehash(User* user)
	{
		ConfigReader myConf;
		std::string tmp;

		this->ShowFailedConnects = myConf.ReadFlag("antirandom", "showfailedconnects", "1", 0);
		this->DebugMode = myConf.ReadFlag("antirandom", "debugmode", "0", 0);

		tmp = myConf.ReadValue("antirandom", "threshold", 0);
		if (!tmp.empty())
			this->Threshold = atoi(tmp.c_str());
		else
			this->Threshold = 10; // fairly safe

		// Sanity checks
		if (this->Threshold < 1)
		    this->Threshold = 1;

		if (this->Threshold >= 100)
		    this->Threshold = 100;

		this->BanAction = ANTIRANDOM_ACT_NONE;
		tmp = myConf.ReadValue("antirandom", "banaction", 0);

		if (tmp == "GLINE")
		{
			this->BanAction = ANTIRANDOM_ACT_GLINE;
		}
		if (tmp == "ZLINE")
		{
			this->BanAction = ANTIRANDOM_ACT_ZLINE;
		}
		else if (tmp == "KILL")
		{
			this->BanAction = ANTIRANDOM_ACT_KILL;
		}

		tmp = myConf.ReadValue("antirandom", "banduration", 0);
		if (!tmp.empty())
			this->BanDuration = ServerInstance->Duration(tmp.c_str());
		    // Sanity check
		    if ((int)this->BanDuration <= 0)
		        this->BanDuration = 1;
		else
			this->BanDuration = 86400; // One day.

		tmp = myConf.ReadValue("antirandom", "banreason", 0);
		if (!tmp.empty())
			this->BanReason = tmp;
		else
			this->BanReason = "You look like a bot. Change your nick/ident/gecos and try reconnecting.";

		Exempts.clear();

		ConfigTagList exempts_list = ServerInstance->Config->ConfTags("antirandomexempt");
		for (ConfigIter i = exempts_list.first; i != exempts_list.second; ++i)
		{
			ConfigTag* tag = i->second;

			std::string type = tag->getString("type");
			std::string pattern = tag->getString("pattern");

			if(pattern.length() && type.length())
			{
				AntirandomExemptType exemptType;
				if (type == "nick")
					exemptType = NICK;
				else if (type == "ident")
					exemptType = IDENT;
				else if (type == "host")
					exemptType = HOST;
				else if (type == "fullname")
					exemptType = FULLNAME;
				else
				{
					ServerInstance->SNO->WriteGlobalSno('a', "m_antirandom: Invalid <antirandomexempt:type> value in config: %s", type.c_str());
					ServerInstance->Logs->Log("CONFIG",DEFAULT, "m_antirandom: Invalid <antirandomexempt:type> value in config: %s", type.c_str());
					continue;
				}

				Exempts.push_back(AntirandomExempt(exemptType, pattern));
				if (this->DebugMode)
					ServerInstance->SNO->WriteGlobalSno('a', "m_antirandom: Added exempt: %s (%s)", type.c_str(), pattern.c_str());
			}
			else
			{
				ServerInstance->Logs->Log("CONFIG",DEFAULT, "m_antirandom: Invalid block <antirandomexempt type=\"%s\" pattern=\"%s\">", type.c_str(), pattern.c_str());
				ServerInstance->SNO->WriteGlobalSno('a', "m_antirandom: Invalid block <antirandomexempt type=\"%s\" pattern=\"%s\">", type.c_str(), pattern.c_str());
				continue;
			}
		}
	}

};


MODULE_INIT(ModuleAntiRandom)

