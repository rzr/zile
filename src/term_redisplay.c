/* Redisplay engine

   Copyright (c) 1997-2011 Free Software Foundation, Inc.

   This file is part of GNU Zile.

   GNU Zile is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   GNU Zile is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GNU Zile; see the file COPYING.  If not, write to the
   Free Software Foundation, Fifth Floor, 51 Franklin Street, Boston,
   MA 02111-1301, USA.  */

#include <config.h>

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include <config.h>
#include "extern.h"

static char *
make_char_printable (int c, int x, int cur_tab_width)
{
  if (c == '\t')
    return xasprintf ("%*s", cur_tab_width - x % cur_tab_width, "");
  if (isprint (c))
    return xasprintf ("%c", c);
  if (c == '\0')
    return xasprintf ("^@");
  else if (c > 0 && c <= '\32')
    return xasprintf ("^%c", 'A' + c - 1);
  else
    return xasprintf ("\\%o", c & 0xff);
}

static bool
in_region (size_t o, size_t x, Region r)
{
  return o + x >= r.start && o + x < r.end;
}

static void
draw_line (size_t line, size_t startcol, Window * wp,
           size_t o, Region r, int highlight, size_t cur_tab_width)
{
  term_move (line, 0);

  /* Draw body of line. */
  size_t x, i;
  for (x = 0, i = startcol;; i++)
    {
      term_attrset (highlight && in_region (o, i, r) ? FONT_REVERSE : FONT_NORMAL);
      if (i >= estr_line_len (get_buffer_text (get_window_bp (wp)), o) ||
          x >= get_window_ewidth (wp))
        break;
      char *s = make_char_printable (astr_get (get_buffer_text (get_window_bp (wp)).as, o + i),
                                     x, cur_tab_width);
      term_addstr (s);
      x += strlen (s);
    }

  /* Draw end of line. */
  if (x >= term_width ())
    {
      term_move (line, term_width () - 1);
      term_attrset (FONT_NORMAL);
      term_addstr ("$");
    }
  else
    term_addstr (xasprintf ("%*s", (int) (get_window_ewidth (wp) - x), ""));
  term_attrset (FONT_NORMAL);
}

static int
calculate_highlight_region (Window * wp, Region * rp)
{
  if ((wp != cur_wp
       && !get_variable_bool ("highlight-nonselected-windows"))
      || get_buffer_mark (get_window_bp (wp)) == NULL
      || !get_buffer_mark_active (get_window_bp (wp)))
    return false;

  *rp = region_new (window_o (wp), get_marker_o (get_buffer_mark (get_window_bp (wp))));
  return true;
}

static void
draw_window (size_t topline, Window * wp)
{
  size_t i, o;
  Region r;
  int highlight = calculate_highlight_region (wp, &r);

  /* Find the first line to display on the first screen line. */
  for (o = estr_start_of_line (get_buffer_text (get_window_bp (wp)), window_o (wp)), i = get_window_topdelta (wp);
       i > 0 && o > 0;
       assert ((o = estr_prev_line (get_buffer_text (get_window_bp (wp)), o)) != SIZE_MAX), --i)
    ;

  /* Draw the window lines. */
  size_t cur_tab_width = tab_width (get_window_bp (wp));
  for (i = topline; i < get_window_eheight (wp) + topline; ++i)
    {
      /* Clear the line. */
      term_move (i, 0);
      term_clrtoeol ();

      /* If at the end of the buffer, don't write any text. */
      if (o == SIZE_MAX)
        continue;

      draw_line (i, get_window_start_column (wp), wp, o, r, highlight, cur_tab_width);

      if (get_window_start_column (wp) > 0)
        {
          term_move (i, 0);
          term_addstr("$");
        }

      o = estr_next_line (get_buffer_text (get_window_bp (wp)), o);
    }

  set_window_all_displayed (wp, o >= get_buffer_size (get_window_bp (wp)));
}

static const char *
make_mode_line_flags (Window * wp)
{
  if (get_buffer_modified (get_window_bp (wp)) && get_buffer_readonly (get_window_bp (wp)))
    return "%*";
  else if (get_buffer_modified (get_window_bp (wp)))
    return "**";
  else if (get_buffer_readonly (get_window_bp (wp)))
    return "%%";
  return "--";
}

static char *
make_screen_pos (Window * wp)
{
  bool tv = window_top_visible (wp);
  bool bv = window_bottom_visible (wp);

  if (tv && bv)
    return xasprintf ("All");
  else if (tv)
    return xasprintf ("Top");
  else if (bv)
    return xasprintf ("Bot");
  else
    return xasprintf ("%2d%%",
                      (int) ((float) (window_o (wp) / get_buffer_size (get_window_bp (wp))) * 100.0));
}

static void
draw_status_line (size_t line, Window * wp)
{
  term_attrset (FONT_REVERSE);

  term_move (line, 0);
  for (size_t i = 0; i < get_window_ewidth (wp); ++i)
    term_addstr ("-");

  const char *eol_type;
  if (get_buffer_text (cur_bp).eol == coding_eol_cr)
    eol_type = "(Mac)";
  else if (get_buffer_text (cur_bp).eol == coding_eol_crlf)
    eol_type = "(DOS)";
  else
    eol_type = ":";

  term_move (line, 0);
  Point pt = offset_to_point (get_window_bp (wp), window_o (wp));
  astr as = astr_fmt ("--%s%2s  %-15s   %s %-9s (Fundamental",
                      eol_type, make_mode_line_flags (wp), get_buffer_name (get_window_bp (wp)),
                      make_screen_pos (wp), astr_cstr (astr_fmt ("(%d,%d)", pt.n + 1, get_goalc_bp (get_window_bp (wp), pt))));

  if (get_buffer_autofill (get_window_bp (wp)))
    astr_cat_cstr (as, " Fill");
  if (get_buffer_overwrite (get_window_bp (wp)))
    astr_cat_cstr (as, " Ovwrt");
  if (thisflag & FLAG_DEFINING_MACRO)
    astr_cat_cstr (as, " Def");
  if (get_buffer_isearch (get_window_bp (wp)))
    astr_cat_cstr (as, " Isearch");

  astr_cat_char (as, ')');
  term_addstr (astr_cstr (as));

  term_attrset (FONT_NORMAL);
}

void
term_redisplay (void)
{
  /* Calculate the start column if the line at point has to be truncated. */
  Buffer *bp = get_window_bp (cur_wp);
  size_t col, lastcol = 0, t = tab_width (bp);
  size_t o = window_o (cur_wp);
  size_t lineo = o - estr_start_of_line (get_buffer_text (bp), o);

  o -= lineo;
  set_window_start_column (cur_wp, 0);

  size_t ew = get_window_ewidth (cur_wp);
  for (size_t lp = lineo; lp != SIZE_MAX; --lp)
    {
      col = 0;
      for (size_t p = lp; p < lineo; ++p)
        col += strlen (make_char_printable (astr_get (get_buffer_text (bp).as, o + p), col, t));

      if (col >= ew - 1 || (lp / (ew / 3)) + 2 < lineo / (ew / 3))
        {
          set_window_start_column (cur_wp, lp + 1);
          col = lastcol;
          break;
        }

      lastcol = col;
    }

  /* Draw the window. */
  size_t cur_topline = 0, topline = 0;
  for (Window *wp = head_wp; wp != NULL; wp = get_window_next (wp))
    {
      if (wp == cur_wp)
        cur_topline = topline;

      draw_window (topline, wp);

      /* Draw the status line only if there is available space after the
         buffer text space. */
      if (get_window_fheight (wp) - get_window_eheight (wp) > 0)
        draw_status_line (topline + get_window_eheight (wp), wp);

      topline += get_window_fheight (wp);
    }

  /* Redraw cursor. */
  term_move (cur_topline + get_window_topdelta (cur_wp), col);
}

/*
 * Tidy and close the terminal ready to leave Zile.
 */
void
term_finish (void)
{
  term_move (term_height () - 1, 0);
  term_clrtoeol ();
  term_attrset (FONT_NORMAL);
  term_refresh ();
  term_close ();
}
