/*
 * Synergy -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2002 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "XDGKeyUtil.h"

#include "KeyTypes.h"

#include <xkbcommon/xkbcommon-keysyms.h>

// clang-format off

/*
 * This table maps keysym values into the corresponding ISO 10646
 * (UCS, Unicode) values.
 *
 * The array keysymtab[] contains pairs of XKB values for graphical
 * characters and the corresponding Unicode value.
 *
 * Author: Markus G. Kuhn <http://www.cl.cam.ac.uk/~mgk25/>,
 *         University of Cambridge, April 2001
 *
 * Special thanks to Richard Verhoeven <https://github.com/Tupig/Tupig_synergy/issues> for preparing
 * an initial draft of the mapping table.
 *
 * This software is in the public domain. Share and enjoy!
 */
const KeySymMap XDGKeyUtil::s_keySymToUCS4 = {
#include "XDGKeyUnicodeLegacy.inc"
#include "XDGKeyUnicodeScripts.inc"
    {XKB_KEY_OE, 0x0152},                         /* LATIN CAPITAL LIGATURE OE */
    {XKB_KEY_oe, 0x0153},                         /* LATIN SMALL LIGATURE OE */
    {XKB_KEY_Ydiaeresis, 0x0178},                 /* LATIN CAPITAL LETTER Y WITH DIAERESIS */
    {XKB_KEY_EuroSign, 0x20ac},                   /* EURO SIGN */

    /* combining dead keys */
    {XKB_KEY_dead_abovedot, 0x0307},    /* COMBINING DOT ABOVE */
    {XKB_KEY_dead_abovering, 0x030a},   /* COMBINING RING ABOVE */
    {XKB_KEY_dead_acute, 0x0301},       /* COMBINING ACUTE ACCENT */
    {XKB_KEY_dead_breve, 0x0306},       /* COMBINING BREVE */
    {XKB_KEY_dead_caron, 0x030c},       /* COMBINING CARON */
    {XKB_KEY_dead_cedilla, 0x0327},     /* COMBINING CEDILLA */
    {XKB_KEY_dead_circumflex, 0x0302},  /* COMBINING CIRCUMFLEX ACCENT */
    {XKB_KEY_dead_diaeresis, 0x0308},   /* COMBINING DIAERESIS */
    {XKB_KEY_dead_doubleacute, 0x030b}, /* COMBINING DOUBLE ACUTE ACCENT */
    {XKB_KEY_dead_grave, 0x0300},       /* COMBINING GRAVE ACCENT */
    {XKB_KEY_dead_macron, 0x0304},      /* COMBINING MACRON */
    {XKB_KEY_dead_ogonek, 0x0328},      /* COMBINING OGONEK */
    {XKB_KEY_dead_tilde, 0x0303}        /* COMBINING TILDE */
};
/* XXX -- map these too
XKB_KEY_Cyrillic_GHE_bar
XKB_KEY_Cyrillic_ZHE_descender
XKB_KEY_Cyrillic_KA_descender
XKB_KEY_Cyrillic_KA_vertstroke
XKB_KEY_Cyrillic_EN_descender
XKB_KEY_Cyrillic_U_straight
XKB_KEY_Cyrillic_U_straight_bar
XKB_KEY_Cyrillic_HA_descender
XKB_KEY_Cyrillic_CHE_descender
XKB_KEY_Cyrillic_CHE_vertstroke
XKB_KEY_Cyrillic_SHHA
XKB_KEY_Cyrillic_SCHWA
XKB_KEY_Cyrillic_I_macron
XKB_KEY_Cyrillic_O_bar
XKB_KEY_Cyrillic_U_macron
XKB_KEY_Cyrillic_ghe_bar
XKB_KEY_Cyrillic_zhe_descender
XKB_KEY_Cyrillic_ka_descender
XKB_KEY_Cyrillic_ka_vertstroke
XKB_KEY_Cyrillic_en_descender
XKB_KEY_Cyrillic_u_straight
XKB_KEY_Cyrillic_u_straight_bar
XKB_KEY_Cyrillic_ha_descender
XKB_KEY_Cyrillic_che_descender
XKB_KEY_Cyrillic_che_vertstroke
XKB_KEY_Cyrillic_shha
XKB_KEY_Cyrillic_schwa
XKB_KEY_Cyrillic_i_macron
XKB_KEY_Cyrillic_o_bar
XKB_KEY_Cyrillic_u_macron

XKB_KEY_Armenian_eternity
XKB_KEY_Armenian_ligature_ew
XKB_KEY_Armenian_full_stop
XKB_KEY_Armenian_verjaket
XKB_KEY_Armenian_parenright
XKB_KEY_Armenian_parenleft
XKB_KEY_Armenian_guillemotright
XKB_KEY_Armenian_guillemotleft
XKB_KEY_Armenian_em_dash
XKB_KEY_Armenian_dot
XKB_KEY_Armenian_mijaket
XKB_KEY_Armenian_but
XKB_KEY_Armenian_separation_mark
XKB_KEY_Armenian_comma
XKB_KEY_Armenian_en_dash
XKB_KEY_Armenian_hyphen
XKB_KEY_Armenian_yentamna
XKB_KEY_Armenian_ellipsis
XKB_KEY_Armenian_amanak
XKB_KEY_Armenian_exclam
XKB_KEY_Armenian_accent
XKB_KEY_Armenian_shesht
XKB_KEY_Armenian_paruyk
XKB_KEY_Armenian_question
XKB_KEY_Armenian_AYB
XKB_KEY_Armenian_ayb
XKB_KEY_Armenian_BEN
XKB_KEY_Armenian_ben
XKB_KEY_Armenian_GIM
XKB_KEY_Armenian_gim
XKB_KEY_Armenian_DA
XKB_KEY_Armenian_da
XKB_KEY_Armenian_YECH
XKB_KEY_Armenian_yech
XKB_KEY_Armenian_ZA
XKB_KEY_Armenian_za
XKB_KEY_Armenian_E
XKB_KEY_Armenian_e
XKB_KEY_Armenian_AT
XKB_KEY_Armenian_at
XKB_KEY_Armenian_TO
XKB_KEY_Armenian_to
XKB_KEY_Armenian_ZHE
XKB_KEY_Armenian_zhe
XKB_KEY_Armenian_INI
XKB_KEY_Armenian_ini
XKB_KEY_Armenian_LYUN
XKB_KEY_Armenian_lyun
XKB_KEY_Armenian_KHE
XKB_KEY_Armenian_khe
XKB_KEY_Armenian_TSA
XKB_KEY_Armenian_tsa
XKB_KEY_Armenian_KEN
XKB_KEY_Armenian_ken
XKB_KEY_Armenian_HO
XKB_KEY_Armenian_ho
XKB_KEY_Armenian_DZA
XKB_KEY_Armenian_dza
XKB_KEY_Armenian_GHAT
XKB_KEY_Armenian_ghat
XKB_KEY_Armenian_TCHE
XKB_KEY_Armenian_tche
XKB_KEY_Armenian_MEN
XKB_KEY_Armenian_men
XKB_KEY_Armenian_HI
XKB_KEY_Armenian_hi
XKB_KEY_Armenian_NU
XKB_KEY_Armenian_nu
XKB_KEY_Armenian_SHA
XKB_KEY_Armenian_sha
XKB_KEY_Armenian_VO
XKB_KEY_Armenian_vo
XKB_KEY_Armenian_CHA
XKB_KEY_Armenian_cha
XKB_KEY_Armenian_PE
XKB_KEY_Armenian_pe
XKB_KEY_Armenian_JE
XKB_KEY_Armenian_je
XKB_KEY_Armenian_RA
XKB_KEY_Armenian_ra
XKB_KEY_Armenian_SE
XKB_KEY_Armenian_se
XKB_KEY_Armenian_VEV
XKB_KEY_Armenian_vev
XKB_KEY_Armenian_TYUN
XKB_KEY_Armenian_tyun
XKB_KEY_Armenian_RE
XKB_KEY_Armenian_re
XKB_KEY_Armenian_TSO
XKB_KEY_Armenian_tso
XKB_KEY_Armenian_VYUN
XKB_KEY_Armenian_vyun
XKB_KEY_Armenian_PYUR
XKB_KEY_Armenian_pyur
XKB_KEY_Armenian_KE
XKB_KEY_Armenian_ke
XKB_KEY_Armenian_O
XKB_KEY_Armenian_o
XKB_KEY_Armenian_FE
XKB_KEY_Armenian_fe
XKB_KEY_Armenian_apostrophe
XKB_KEY_Armenian_section_sign

XKB_KEY_Georgian_an
XKB_KEY_Georgian_ban
XKB_KEY_Georgian_gan
XKB_KEY_Georgian_don
XKB_KEY_Georgian_en
XKB_KEY_Georgian_vin
XKB_KEY_Georgian_zen
XKB_KEY_Georgian_tan
XKB_KEY_Georgian_in
XKB_KEY_Georgian_kan
XKB_KEY_Georgian_las
XKB_KEY_Georgian_man
XKB_KEY_Georgian_nar
XKB_KEY_Georgian_on
XKB_KEY_Georgian_par
XKB_KEY_Georgian_zhar
XKB_KEY_Georgian_rae
XKB_KEY_Georgian_san
XKB_KEY_Georgian_tar
XKB_KEY_Georgian_un
XKB_KEY_Georgian_phar
XKB_KEY_Georgian_khar
XKB_KEY_Georgian_ghan
XKB_KEY_Georgian_qar
XKB_KEY_Georgian_shin
XKB_KEY_Georgian_chin
XKB_KEY_Georgian_can
XKB_KEY_Georgian_jil
XKB_KEY_Georgian_cil
XKB_KEY_Georgian_char
XKB_KEY_Georgian_xan
XKB_KEY_Georgian_jhan
XKB_KEY_Georgian_hae
XKB_KEY_Georgian_he
XKB_KEY_Georgian_hie
XKB_KEY_Georgian_we
XKB_KEY_Georgian_har
XKB_KEY_Georgian_hoe
XKB_KEY_Georgian_fi

XKB_KEY_Ccedillaabovedot
XKB_KEY_Xabovedot
XKB_KEY_Qabovedot
XKB_KEY_Ibreve
XKB_KEY_IE
XKB_KEY_UO
XKB_KEY_Zstroke
XKB_KEY_Gcaron
XKB_KEY_Obarred
XKB_KEY_ccedillaabovedot
XKB_KEY_xabovedot
XKB_KEY_Ocaron
XKB_KEY_qabovedot
XKB_KEY_ibreve
XKB_KEY_ie
XKB_KEY_uo
XKB_KEY_zstroke
XKB_KEY_gcaron
XKB_KEY_ocaron
XKB_KEY_obarred
XKB_KEY_SCHWA
XKB_KEY_Lbelowdot
XKB_KEY_Lstrokebelowdot
XKB_KEY_Gtilde
XKB_KEY_lbelowdot
XKB_KEY_lstrokebelowdot
XKB_KEY_gtilde
XKB_KEY_schwa

XKB_KEY_Abelowdot
XKB_KEY_abelowdot
XKB_KEY_Ahook
XKB_KEY_ahook
XKB_KEY_Acircumflexacute
XKB_KEY_acircumflexacute
XKB_KEY_Acircumflexgrave
XKB_KEY_acircumflexgrave
XKB_KEY_Acircumflexhook
XKB_KEY_acircumflexhook
XKB_KEY_Acircumflextilde
XKB_KEY_acircumflextilde
XKB_KEY_Acircumflexbelowdot
XKB_KEY_acircumflexbelowdot
XKB_KEY_Abreveacute
XKB_KEY_abreveacute
XKB_KEY_Abrevegrave
XKB_KEY_abrevegrave
XKB_KEY_Abrevehook
XKB_KEY_abrevehook
XKB_KEY_Abrevetilde
XKB_KEY_abrevetilde
XKB_KEY_Abrevebelowdot
XKB_KEY_abrevebelowdot
XKB_KEY_Ebelowdot
XKB_KEY_ebelowdot
XKB_KEY_Ehook
XKB_KEY_ehook
XKB_KEY_Etilde
XKB_KEY_etilde
XKB_KEY_Ecircumflexacute
XKB_KEY_ecircumflexacute
XKB_KEY_Ecircumflexgrave
XKB_KEY_ecircumflexgrave
XKB_KEY_Ecircumflexhook
XKB_KEY_ecircumflexhook
XKB_KEY_Ecircumflextilde
XKB_KEY_ecircumflextilde
XKB_KEY_Ecircumflexbelowdot
XKB_KEY_ecircumflexbelowdot
XKB_KEY_Ihook
XKB_KEY_ihook
XKB_KEY_Ibelowdot
XKB_KEY_ibelowdot
XKB_KEY_Obelowdot
XKB_KEY_obelowdot
XKB_KEY_Ohook
XKB_KEY_ohook
XKB_KEY_Ocircumflexacute
XKB_KEY_ocircumflexacute
XKB_KEY_Ocircumflexgrave
XKB_KEY_ocircumflexgrave
XKB_KEY_Ocircumflexhook
XKB_KEY_ocircumflexhook
XKB_KEY_Ocircumflextilde
XKB_KEY_ocircumflextilde
XKB_KEY_Ocircumflexbelowdot
XKB_KEY_ocircumflexbelowdot
XKB_KEY_Ohornacute
XKB_KEY_ohornacute
XKB_KEY_Ohorngrave
XKB_KEY_ohorngrave
XKB_KEY_Ohornhook
XKB_KEY_ohornhook
XKB_KEY_Ohorntilde
XKB_KEY_ohorntilde
XKB_KEY_Ohornbelowdot
XKB_KEY_ohornbelowdot
XKB_KEY_Ubelowdot
XKB_KEY_ubelowdot
XKB_KEY_Uhook
XKB_KEY_uhook
XKB_KEY_Uhornacute
XKB_KEY_uhornacute
XKB_KEY_Uhorngrave
XKB_KEY_uhorngrave
XKB_KEY_Uhornhook
XKB_KEY_uhornhook
XKB_KEY_Uhorntilde
XKB_KEY_uhorntilde
XKB_KEY_Uhornbelowdot
XKB_KEY_uhornbelowdot
XKB_KEY_Ybelowdot
XKB_KEY_ybelowdot
XKB_KEY_Yhook
XKB_KEY_yhook
XKB_KEY_Ytilde
XKB_KEY_ytilde
XKB_KEY_Ohorn
XKB_KEY_ohorn
XKB_KEY_Uhorn
XKB_KEY_uhorn
*/

const std::array<KeySym, 256> XDGKeyUtil::s_map1008FF = {
    /* 0x00 */ 0,
    0,
    kKeyBrightnessUp,
    kKeyBrightnessDown,
    0,
    0,
    0,
    0,
    /* 0x08 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0x10 */ 0,
    kKeyAudioDown,
    kKeyAudioMute,
    kKeyAudioUp,
    /* 0x14 */ kKeyAudioPlay,
    kKeyAudioStop,
    kKeyAudioPrev,
    kKeyAudioNext,
    /* 0x18 */ kKeyWWWHome,
    kKeyAppMail,
    0,
    kKeyWWWSearch,
    0,
    0,
    0,
    0,
    /* 0x20 */ 0,
    0,
    0,
    0,
    0,
    0,
    kKeyWWWBack,
    kKeyWWWForward,
    /* 0x28 */ kKeyWWWStop,
    kKeyWWWRefresh,
    0,
    0,
    kKeyEject,
    0,
    0,
    0,
    /* 0x30 */ kKeyWWWFavorites,
    0,
    kKeyAppMedia,
    0,
    0,
    0,
    0,
    0,
    /* 0x38 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0x40 */ kKeyAppUser1,
    kKeyAppUser2,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0x48 */ 0,
    0,
    kKeyMissionControl,
    kKeyLaunchpad,
    0,
    0,
    0,
    0,
    /* 0x50 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0x58 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0x60 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0x68 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0x70 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0x78 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0x80 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0x88 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0x90 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0x98 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0xa0 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0xa8 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0xb0 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0xb8 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0xc0 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0xc8 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0xd0 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0xd8 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0xe0 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0xe8 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0xf0 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    /* 0xf8 */ 0,
    0,
    0,
    0,
    0,
    0,
    0,
    0
};
// clang-format on
