/**
 * @license MIT
 */

#include <vt.h>


/**
 * The character sets supported by the terminal. These enable several languages
 * to be represented within the terminal with only 8-bit encoding. See ISO 2022
 * for a discussion on character sets. Only VT100 character sets are supported.
 */
wchar_t CHARSETS[256][256] = {

  /**
   * DEC Special Character and Line Drawing Set.
   * Reference] = http://vt100.net/docs/vt102-ug/table5-13.html
   * A lot of curses apps use this if they see TERM=xterm.
   * testing] = echo -e '\e(0a\e(B'
   * The xterm output sometimes seems to conflict with the
   * reference above. xterm seems in line with the reference
   * when running vttest however.
   * The table below now uses xterm's output from vttest.
   */
  ['0'] = {
    ['`'] = L'\u25c6', // '◆'
    ['a'] = L'\u2592', // '▒'
    ['b'] = L'\t', // '\t'
    ['c'] = L'\f', // '\f'
    ['d'] = L'\r', // '\r'
    ['e'] = L'\n', // '\n'
    ['f'] = L'\u00b0', // '°'
    ['g'] = L'\u00b1', // '±'
    ['h'] = L'\u2424', // '\u2424' (NL)
    ['i'] = L'\v', // '\v'
    ['j'] = L'\u2518', // '┘'
    ['k'] = L'\u2510', // '┐'
    ['l'] = L'\u250c', // '┌'
    ['m'] = L'\u2514', // '└'
    ['n'] = L'\u253c', // '┼'
    ['o'] = L'\u23ba', // '⎺'
    ['p'] = L'\u23bb', // '⎻'
    ['q'] = L'\u2500', // '─'
    ['r'] = L'\u23bc', // '⎼'
    ['s'] = L'\u23bd', // '⎽'
    ['t'] = L'\u251c', // '├'
    ['u'] = L'\u2524', // '┤'
    ['v'] = L'\u2534', // '┴'
    ['w'] = L'\u252c', // '┬'
    ['x'] = L'\u2502', // '│'
    ['y'] = L'\u2264', // '≤'
    ['z'] = L'\u2265', // '≥'
    ['{'] = L'\u03c0', // 'π'
    ['|'] = L'\u2260', // '≠'
    ['}'] = L'\u00a3', // '£'
    ['~'] = L'\u00b7'  // '·'
  },

  /**
   * British character set
   * ESC (A
   * Reference] = http://vt100.net/docs/vt220-rm/table2-5.html
   */
  ['A'] = {
    ['#'] = L'£'
  },

  /**
   * United States character set
   * ESC (B
   */
  ['B'] = {},

  /**
   * Dutch character set
   * ESC (4
   * Reference] = http://vt100.net/docs/vt220-rm/table2-6.html
   */
  ['4'] = {
    ['#'] = L'£',
    ['@'] = L'¾',
    // ['['] = L'ij',
    ['\\'] = L'½',
    [']'] = L'|',
    ['{'] = L'¨',
    ['|'] = L'f',
    ['}'] = L'¼',
    ['~'] = L'´'
  },

  /**
   * Finnish character set
   * ESC (C or ESC (5
   * Reference] = http://vt100.net/docs/vt220-rm/table2-7.html
   */
  ['C'] = {
    ['['] = L'Ä',
    ['\\'] = L'Ö',
    [']'] = L'Å',
    ['^'] = L'Ü',
    ['`'] = L'é',
    ['{'] = L'ä',
    ['|'] = L'ö',
    ['}'] = L'å',
    ['~'] = L'ü'
  },
  ['5'] = {
    ['['] = L'Ä',
    ['\\'] = L'Ö',
    [']'] = L'Å',
    ['^'] = L'Ü',
    ['`'] = L'é',
    ['{'] = L'ä',
    ['|'] = L'ö',
    ['}'] = L'å',
    ['~'] = L'ü'
  },

  /**
   * French character set
   * ESC (R
   * Reference] = http://vt100.net/docs/vt220-rm/table2-8.html
   */
  ['R'] = {
    ['#'] = L'£',
    ['@'] = L'à',
    ['['] = L'°',
    ['\\'] = L'ç',
    [']'] = L'§',
    ['{'] = L'é',
    ['|'] = L'ù',
    ['}'] = L'è',
    ['~'] = L'¨'
  },

  /**
   * French Canadian character set
   * ESC (Q
   * Reference] = http://vt100.net/docs/vt220-rm/table2-9.html
   */
  ['Q'] = {
    ['@'] = L'à',
    ['['] = L'â',
    ['\\'] = L'ç',
    [']'] = L'ê',
    ['^'] = L'î',
    ['`'] = L'ô',
    ['{'] = L'é',
    ['|'] = L'ù',
    ['}'] = L'è',
    ['~'] = L'û'
  },

  /**
   * German character set
   * ESC (K
   * Reference] = http://vt100.net/docs/vt220-rm/table2-10.html
   */
  ['K'] = {
    ['@'] = L'§',
    ['['] = L'Ä',
    ['\\'] = L'Ö',
    [']'] = L'Ü',
    ['{'] = L'ä',
    ['|'] = L'ö',
    ['}'] = L'ü',
    ['~'] = L'ß'
  },

  /**
   * Italian character set
   * ESC (Y
   * Reference] = http://vt100.net/docs/vt220-rm/table2-11.html
   */
  ['Y'] = {
    ['#'] = L'£',
    ['@'] = L'§',
    ['['] = L'°',
    ['\\'] = L'ç',
    [']'] = L'é',
    ['`'] = L'ù',
    ['{'] = L'à',
    ['|'] = L'ò',
    ['}'] = L'è',
    ['~'] = L'ì'
  },

  /**
   * Norwegian/Danish character set
   * ESC (E or ESC (6
   * Reference] = http://vt100.net/docs/vt220-rm/table2-12.html
   */
  ['E'] = {
    ['@'] = L'Ä',
    ['['] = L'Æ',
    ['\\'] = L'Ø',
    [']'] = L'Å',
    ['^'] = L'Ü',
    ['`'] = L'ä',
    ['{'] = L'æ',
    ['|'] = L'ø',
    ['}'] = L'å',
    ['~'] = L'ü'
  },
  ['6'] = {
    ['@'] = L'Ä',
    ['['] = L'Æ',
    ['\\'] = L'Ø',
    [']'] = L'Å',
    ['^'] = L'Ü',
    ['`'] = L'ä',
    ['{'] = L'æ',
    ['|'] = L'ø',
    ['}'] = L'å',
    ['~'] = L'ü'
  },

  /**
   * Spanish character set
   * ESC (Z
   * Reference] = http://vt100.net/docs/vt220-rm/table2-13.html
   */
  ['Z'] = {
    ['#'] = L'£',
    ['@'] = L'§',
    ['['] = L'¡',
    ['\\'] = L'Ñ',
    [']'] = L'¿',
    ['{'] = L'°',
    ['|'] = L'ñ',
    ['}'] = L'ç'
  },

  /**
   * Swedish character set
   * ESC (H or ESC (7
   * Reference] = http://vt100.net/docs/vt220-rm/table2-14.html
   */
  ['H'] = {
    ['@'] = L'É',
    ['['] = L'Ä',
    ['\\'] = L'Ö',
    [']'] = L'Å',
    ['^'] = L'Ü',
    ['`'] = L'é',
    ['{'] = L'ä',
    ['|'] = L'ö',
    ['}'] = L'å',
    ['~'] = L'ü'
  },
  ['7'] = {
    ['@'] = L'É',
    ['['] = L'Ä',
    ['\\'] = L'Ö',
    [']'] = L'Å',
    ['^'] = L'Ü',
    ['`'] = L'é',
    ['{'] = L'ä',
    ['|'] = L'ö',
    ['}'] = L'å',
    ['~'] = L'ü'
  },

  /**
   * Swiss character set
   * ESC (=
   * Reference] = http://vt100.net/docs/vt220-rm/table2-15.html
   */
  ['='] = {
    ['#'] = L'ù',
    ['@'] = L'à',
    ['['] = L'é',
    ['\\'] = L'ç',
    [']'] = L'ê',
    ['^'] = L'î',
    ['_'] = L'è',
    ['`'] = L'ô',
    ['{'] = L'ä',
    ['|'] = L'ö',
    ['}'] = L'ü',
    ['~'] = L'û'
  },
};

/**
 * The default character set, US.
 */
wchar_t *DEFAULT_CHARSET = CHARSETS['='];
