/* Miscellaneous Emacs functions

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
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "main.h"
#include "extern.h"


DEFUN ("suspend-emacs", suspend_emacs)
/*+
Stop Zile and return to superior process.
+*/
{
  raise (SIGTSTP);
}
END_DEFUN

DEFUN ("keyboard-quit", keyboard_quit)
/*+
Cancel current command.
+*/
{
  deactivate_mark ();
  minibuf_error ("Quit");
  ok = leNIL;
}
END_DEFUN

static void
print_buf (Buffer * old_bp, Buffer * bp)
{
  if (get_buffer_name (bp)[0] == ' ')
    return;

  bprintf ("%c%c%c %-19s %6u  %-17s",
           old_bp == bp ? '.' : ' ',
           get_buffer_readonly (bp) ? '%' : ' ',
           get_buffer_modified (bp) ? '*' : ' ',
           get_buffer_name (bp), get_buffer_size (bp), "Fundamental");
  if (get_buffer_filename (bp) != NULL)
    bprintf ("%s", astr_cstr (compact_path (astr_new_cstr (get_buffer_filename (bp)))));
  insert_newline ();
}

void
write_temp_buffer (const char *name, bool show, void (*func) (va_list ap), ...)
{
  Window *wp, *old_wp = cur_wp;
  Buffer *new_bp, *old_bp = cur_bp;
  va_list ap;

  /* Popup a window with the buffer "name". */
  wp = find_window (name);
  if (show && wp)
    set_current_window (wp);
  else
    {
      Buffer *bp = find_buffer (name);
      if (show)
        set_current_window (popup_window ());
      if (bp == NULL)
        {
          bp = buffer_new ();
          set_buffer_name (bp, name);
        }
      switch_to_buffer (bp);
    }

  /* Remove the contents of that buffer. */
  new_bp = buffer_new ();
  set_buffer_name (new_bp, get_buffer_name (cur_bp));
  kill_buffer (cur_bp);
  cur_bp = new_bp;
  set_window_bp (cur_wp, cur_bp);

  /* Make the buffer a temporary one. */
  set_buffer_needname (cur_bp, true);
  set_buffer_noundo (cur_bp, true);
  set_buffer_nosave (cur_bp, true);
  set_temporary_buffer (cur_bp);

  /* Use the "callback" routine. */
  va_start (ap, func);
  func (ap);
  va_end (ap);

  FUNCALL (beginning_of_buffer);
  set_buffer_readonly (cur_bp, true);
  set_buffer_modified (cur_bp, false);

  /* Restore old current window. */
  set_current_window (old_wp);

  /* If we're not showing the new buffer, switch back to the old one. */
  if (!show)
    switch_to_buffer (old_bp);
}

static void
write_buffers_list (va_list ap)
{
  Window *old_wp = va_arg (ap, Window *);
  Buffer *bp;

  /* FIXME: Underline next line properly. */
  bprintf ("CRM Buffer                Size  Mode             File\n");
  bprintf ("--- ------                ----  ----             ----\n");

  /* Print buffers. */
  bp = get_window_bp (old_wp);
  do
    {
      /* Print all buffers except this one (the *Buffer List*). */
      if (cur_bp != bp)
        print_buf (get_window_bp (old_wp), bp);
      bp = get_buffer_next (bp);
      if (bp == NULL)
        bp = head_bp;
    }
  while (bp != get_window_bp (old_wp));
}

DEFUN ("list-buffers", list_buffers)
/*+
Display a list of names of existing buffers.
The list is displayed in a buffer named `*Buffer List*'.
Note that buffers with names starting with spaces are omitted.

@itemize -
The @samp{M} column contains a @samp{*} for buffers that are modified.
The @samp{R} column contains a @samp{%} for buffers that are read-only.
@end itemize
+*/
{
  write_temp_buffer ("*Buffer List*", true, write_buffers_list, cur_wp);
}
END_DEFUN

DEFUN_ARGS ("overwrite-mode", overwrite_mode,
            INT_OR_UNIARG (arg))
/*+
Toggle overwrite mode.
With prefix argument @i{arg}, turn overwrite mode on if @i{arg} is positive,
otherwise turn it off.  In overwrite mode, printing characters typed
in replace existing text on a one-for-one basis, rather than pushing
it to the right.  At the end of a line, such characters extend the line.
Before a tab, such characters insert until the tab is filled in.
@kbd{C-q} still inserts characters in overwrite mode; this
is supposed to make it easier to insert characters when necessary.
+*/
{
  INT_OR_UNIARG_INIT (arg);
  set_buffer_overwrite (cur_bp, noarg ? !get_buffer_overwrite (cur_bp) :
                        uniarg > 0);
}
END_DEFUN

DEFUN ("toggle-read-only", toggle_read_only)
/*+
Change whether this buffer is visiting its file read-only.
+*/
{
  set_buffer_readonly (cur_bp, !get_buffer_readonly (cur_bp));
}
END_DEFUN

DEFUN ("auto-fill-mode", auto_fill_mode)
/*+
Toggle Auto Fill mode.
In Auto Fill mode, inserting a space at a column beyond `fill-column'
automatically breaks the line at a previous space.
+*/
{
  set_buffer_autofill (cur_bp, !get_buffer_autofill (cur_bp));
}
END_DEFUN

DEFUN ("set-fill-column", set_fill_column)
/*+
Set `fill-column' to specified argument.
Use C-u followed by a number to specify a column.
Just C-u as argument means to use the current column.
+*/
{
  long fill_col = (lastflag & FLAG_UNIARG_EMPTY) ?
    get_buffer_pt (cur_bp).o : (unsigned long) uniarg;
  char *buf = NULL;

  if (!(lastflag & FLAG_SET_UNIARG) && arglist == NULL)
    {
      fill_col = minibuf_read_number ("Set fill-column to (default %d): ", get_buffer_pt (cur_bp).o);
      if (fill_col == LONG_MAX)
        return leNIL;
      else if (fill_col == LONG_MAX - 1)
        fill_col = get_buffer_pt (cur_bp).o;
    }

  if (arglist)
    {
      if (arglist->next)
        buf = arglist->next->data;
      else
        {
          minibuf_error ("set-fill-column requires an explicit argument");
          ok = leNIL;
        }
    }
  else
    {
      buf = xasprintf ("%ld", fill_col);
      /* Only print message when run interactively. */
      minibuf_write ("Fill column set to %s (was %d)", buf,
                     get_variable_number ("fill-column"));
    }

  if (ok == leT)
    set_variable ("fill-column", buf);
}
END_DEFUN

void
set_mark_interactive (void)
{
  set_mark ();
  minibuf_write ("Mark set");
}

DEFUN_NONINTERACTIVE ("set-mark", set_mark)
/*+
Set this buffer's mark to point.
+*/
{
  set_mark_interactive ();
  activate_mark ();
}
END_DEFUN

DEFUN ("set-mark-command", set_mark_command)
/*+
Set the mark where point is.
+*/
{
  FUNCALL (set_mark);
}
END_DEFUN

DEFUN ("exchange-point-and-mark", exchange_point_and_mark)
/*+
Put the mark where point is now, and point where the mark is now.
+*/
{
  Point tmp;

  if (get_buffer_mark (cur_bp) == NULL)
    {
      minibuf_error ("No mark set in this buffer");
      return leNIL;
    }

  tmp = get_buffer_pt (cur_bp);
  goto_point (get_marker_pt (get_buffer_mark (cur_bp)));
  set_marker_o (get_buffer_mark (cur_bp), point_to_offset (tmp));
  activate_mark ();
  thisflag |= FLAG_NEED_RESYNC;
}
END_DEFUN

DEFUN ("mark-whole-buffer", mark_whole_buffer)
/*+
Put point at beginning and mark at end of buffer.
+*/
{
  FUNCALL (end_of_buffer);
  FUNCALL (set_mark_command);
  FUNCALL (beginning_of_buffer);
}
END_DEFUN

static void
quoted_insert_octal (int c1)
{
  int c2, c3;
  minibuf_write ("C-q %c-", c1);
  c2 = getkey ();

  if (!isdigit (c2) || c2 - '0' >= 8)
    {
      insert_char (c1 - '0');
      insert_char (c2);
    }
  else
    {
      minibuf_write ("C-q %c %c-", c1, c2);
      c3 = getkey ();

      if (!isdigit (c3) || c3 - '0' >= 8)
        {
          insert_char ((c1 - '0') * 8 + (c2 - '0'));
          insert_char (c3);
        }
      else
        insert_char ((c1 - '0') * 64 + (c2 - '0') * 8 + (c3 - '0'));
    }
}

DEFUN ("quoted-insert", quoted_insert)
/*+
Read next input character and insert it.
This is useful for inserting control characters.
You may also type up to 3 octal digits, to insert a character with that code.
+*/
{
  int c;

  minibuf_write ("C-q-");
  c = xgetkey (GETKEY_UNFILTERED, 0);

  if (isdigit (c) && c - '0' < 8)
    quoted_insert_octal (c);
  else
    insert_char (c);

  minibuf_clear ();
}
END_DEFUN

DEFUN ("universal-argument", universal_argument)
/*+
Begin a numeric argument for the following command.
Digits or minus sign following @kbd{C-u} make up the numeric argument.
@kbd{C-u} following the digits or minus sign ends the argument.
@kbd{C-u} without digits or minus sign provides 4 as argument.
Repeating @kbd{C-u} without digits or minus sign multiplies the argument
by 4 each time.
+*/
{
  int i = 0, arg = 1, sgn = 1;
  astr as = astr_new ();

  /* Need to process key used to invoke universal-argument. */
  pushkey (lastkey ());

  thisflag |= FLAG_UNIARG_EMPTY;

  for (;;)
    {
      size_t key = do_binding_completion (as);

      /* Cancelled. */
      if (key == KBD_CANCEL)
        {
          ok = FUNCALL (keyboard_quit);
          break;
        }
      /* Digit pressed. */
      else if (isdigit (key & 0xff))
        {
          int digit = (key & 0xff) - '0';
          thisflag &= ~FLAG_UNIARG_EMPTY;

          if (key & KBD_META)
            {
              if (astr_len (as) > 0)
                astr_cat_char (as, ' ');
              astr_cat_cstr (as, "ESC");
            }

          astr_cat (as, astr_fmt (" %d", digit));

          if (i == 0)
            arg = digit;
          else
            arg = arg * 10 + digit;

          i++;
        }
      else if (key == (KBD_CTRL | 'u'))
        {
          astr_cat_cstr (as, "C-u");
          if (i == 0)
            arg *= 4;
          else
            break;
        }
      else if (key == '-' && i == 0)
        {
          if (sgn > 0)
            {
              sgn = -sgn;
              astr_cat_cstr (as, " -");
              /* The default negative arg is -1, not -4. */
              arg = 1;
              thisflag &= ~FLAG_UNIARG_EMPTY;
            }
        }
      else
        {
          ungetkey (key);
          break;
        }
    }

  if (ok == leT)
    {
      last_uniarg = arg * sgn;
      thisflag |= FLAG_SET_UNIARG;
      minibuf_clear ();
    }
}
END_DEFUN

DEFUN ("back-to-indentation", back_to_indentation)
/*+
Move point to the first non-whitespace character on this line.
+*/
{
  goto_offset (get_buffer_line_o (cur_bp));
  while (!eolp () && isspace (following_char ()))
    forward_char ();
}
END_DEFUN

/***********************************************************************
                          Move through words
***********************************************************************/
#define ISWORDCHAR(c)	(isalnum (c) || c == '$')
static bool
move_word (int dir, int (*next) (void), bool (*move) (void), bool (*at_extreme) (void))
{
  int gotword = false;
  for (;;)
    {
      while (!at_extreme ())
        {
          int c = next ();
          Point pt;

          if (!ISWORDCHAR (c))
            {
              if (gotword)
                return true;
            }
          else
            gotword = true;
          pt = get_buffer_pt (cur_bp);
          pt.o += dir;
          goto_point (pt);
        }
      if (gotword)
        return true;
      if (!move ())
        break;
    }
  return false;
}

static bool
forward_word (void)
{
  return move_word (1, following_char, forward_char, eolp);
}

static bool
backward_word (void)
{
  return move_word (-1, preceding_char, backward_char, bolp);
}

DEFUN ("forward-word", forward_word)
/*+
Move point forward one word (backward if the argument is negative).
With argument, do this that many times.
+*/
{
  ok = execute_with_uniarg (false, uniarg, forward_word, backward_word);
}
END_DEFUN

DEFUN ("backward-word", backward_word)
/*+
Move backward until encountering the end of a word (forward if the
argument is negative).
With argument, do this that many times.
+*/
{
  ok = execute_with_uniarg (false, uniarg, backward_word, forward_word);
}
END_DEFUN

/***********************************************************************
               Move through balanced expressions (sexp)
***********************************************************************/
#define ISSEXPCHAR(c)         (isalnum (c) || c == '$' || c == '_')
#define ISOPENBRACKETCHAR(c)  ((c == '(') || (c == '[') || ( c== '{') ||\
                               ((c == '\"') && !double_quote) ||	\
                               ((c == '\'') && !single_quote))
#define ISCLOSEBRACKETCHAR(c) ((c == ')') || (c == ']') || (c == '}') ||\
                               ((c == '\"') && double_quote) ||		\
                               ((c == '\'') && single_quote))
#define ISSEXPSEPARATOR(c)    (ISOPENBRACKETCHAR (c) ||	\
                               ISCLOSEBRACKETCHAR (c))
#define PRECEDINGQUOTEDQUOTE(c)                                         \
  (c == '\\'                                                            \
   && get_buffer_pt (cur_bp).o + 1 < get_buffer_line_len (cur_bp)       \
   && ((astr_get (get_buffer_text (cur_bp).as, get_buffer_line_o (cur_bp) + 1) == '\"') || \
       (astr_get (get_buffer_text (cur_bp).as, get_buffer_line_o (cur_bp) + 1) == '\'')))
#define FOLLOWINGQUOTEDQUOTE(c)                                         \
  (c == '\\'                                                            \
   && get_buffer_pt (cur_bp).o + 1 < get_buffer_line_len (cur_bp)       \
   && ((astr_get (get_buffer_text (cur_bp).as, get_buffer_line_o (cur_bp) + 1) == '\"') || \
       (astr_get (get_buffer_text (cur_bp).as, get_buffer_line_o (cur_bp) + 1) == '\'')))

static int
move_sexp (int dir)
{
  int gotsexp = false;
  int level = 0;
  int double_quote = dir < 0;
  int single_quote = dir < 0;

  for (;;)
    {
      Point pt;

      while (dir > 0 ? !eolp () : !bolp ())
        {
          int c = dir > 0 ? following_char () : preceding_char ();

          /* Jump quotes that aren't sexp separators. */
          if (dir > 0 ? PRECEDINGQUOTEDQUOTE (c) : FOLLOWINGQUOTEDQUOTE (c))
            {
              pt = get_buffer_pt (cur_bp);
              pt.o += dir;
              goto_point (pt);
              c = 'a';		/* Treat ' and " like word chars. */
            }

          if (dir > 0 ? ISOPENBRACKETCHAR (c) : ISCLOSEBRACKETCHAR (c))
            {
              if (level == 0 && gotsexp)
                return true;

              level++;
              gotsexp = true;
              if (c == '\"')
                double_quote = !double_quote;
              if (c == '\'')
                single_quote = !double_quote;
            }
          else if (dir > 0 ? ISCLOSEBRACKETCHAR (c) : ISOPENBRACKETCHAR (c))
            {
              if (level == 0 && gotsexp)
                return true;

              level--;
              gotsexp = true;
              if (c == '\"')
                double_quote = !double_quote;
              if (c == '\'')
                single_quote = !single_quote;

              if (level < 0)
                {
                  minibuf_error ("Scan error: \"Containing "
                                 "expression ends prematurely\"");
                  return false;
                }
            }

          pt = get_buffer_pt (cur_bp);
          pt.o += dir;
          goto_point (pt);

          if (!ISSEXPCHAR (c))
            {
              if (gotsexp && level == 0)
                {
                  if (!ISSEXPSEPARATOR (c))
                    {
                      pt = get_buffer_pt (cur_bp);
                      pt.o -= dir;
                      goto_point (pt);
                    }
                  return true;
                }
            }
          else
            gotsexp = true;
        }
      if (gotsexp && level == 0)
        return true;
      if (dir > 0 ? !next_line () : !previous_line ())
        {
          if (level != 0)
            minibuf_error ("Scan error: \"Unbalanced parentheses\"");
          break;
        }
      pt = get_buffer_pt (cur_bp);
      pt.o = dir > 0 ? 0 : get_buffer_line_len (cur_bp);
      goto_point (pt);
    }
  return false;
}

static bool
forward_sexp (void)
{
  return move_sexp (1);
}

static bool
backward_sexp (void)
{
  return move_sexp (-1);
}

DEFUN ("forward-sexp", forward_sexp)
/*+
Move forward across one balanced expression (sexp).
With argument, do it that many times.  Negative arg -N means
move backward across N balanced expressions.
+*/
{
  ok = execute_with_uniarg (false, uniarg, forward_sexp, backward_sexp);
}
END_DEFUN

DEFUN ("backward-sexp", backward_sexp)
/*+
Move backward across one balanced expression (sexp).
With argument, do it that many times.  Negative arg -N means
move forward across N balanced expressions.
+*/
{
  ok = execute_with_uniarg (false, uniarg, backward_sexp, forward_sexp);
}
END_DEFUN

/***********************************************************************
                          Transpose functions
***********************************************************************/
static void
astr_append_region (astr s)
{
  activate_mark ();
  astr_cat (s, get_buffer_region (cur_bp, calculate_the_region ()).as);
}

static bool
transpose_subr (bool (*forward_func) (void), bool (*backward_func) (void))
{
  Marker *p0 = point_marker (), *m1, *m2;
  astr as1, as2 = NULL;

  /* For transpose-chars. */
  if (forward_func == forward_char && eolp ())
    backward_func ();
  /* For transpose-lines. */
  if (forward_func == next_line && get_buffer_pt (cur_bp).n == 0)
    forward_func ();

  /* Backward. */
  if (!backward_func ())
    {
      minibuf_error ("Beginning of buffer");
      unchain_marker (p0);
      return false;
    }

  /* Save mark. */
  push_mark ();

  /* Mark the beginning of first string. */
  set_mark ();
  m1 = point_marker ();

  /* Check to make sure we can go forwards twice. */
  if (!forward_func () || !forward_func ())
    {
      if (forward_func == next_line)
        { /* Add an empty line. */
          FUNCALL (end_of_line);
          FUNCALL (newline);
        }
      else
        {
          pop_mark ();
          goto_point (get_marker_pt (m1));
          minibuf_error ("End of buffer");

          unchain_marker (p0);
          unchain_marker (m1);
          return false;
        }
    }

  goto_point (get_marker_pt (m1));

  /* Forward. */
  forward_func ();

  /* Save and delete 1st marked region. */
  as1 = astr_new ();
  astr_append_region (as1);

  unchain_marker (p0);

  FUNCALL (delete_region);

  /* Forward. */
  forward_func ();

  /* For transpose-lines. */
  if (forward_func == next_line)
    m2 = point_marker ();
  else
    {
      /* Mark the end of second string. */
      set_mark ();

      /* Backward. */
      backward_func ();
      m2 = point_marker ();

      /* Save and delete 2nd marked region. */
      as2 = astr_new ();
      astr_append_region (as2);
      FUNCALL (delete_region);
    }

  /* Insert the first string. */
  goto_point (get_marker_pt (m2));
  unchain_marker (m2);
  bprintf ("%s", astr_cstr (as1));

  /* Insert the second string. */
  if (as2)
    {
      goto_point (get_marker_pt (m1));
      bprintf ("%s", astr_cstr (as2));
    }
  unchain_marker (m1);

  /* Restore mark. */
  pop_mark ();
  deactivate_mark ();

  /* Move forward if necessary. */
  if (forward_func != next_line)
    forward_func ();

  return true;
}

static le *
transpose (int uniarg, bool (*forward_func) (void), bool (*backward_func) (void))
{
  if (warn_if_readonly_buffer ())
    return leNIL;

  if (uniarg < 0)
    {
      bool (*tmp_func) (void) = forward_func;
      forward_func = backward_func;
      backward_func = tmp_func;
      uniarg = -uniarg;
    }

  bool ret = true;
  undo_save (UNDO_START_SEQUENCE, get_buffer_pt_o (cur_bp), 0, 0);
  for (int uni = 0; ret && uni < uniarg; ++uni)
    ret = transpose_subr (forward_func, backward_func);
  undo_save (UNDO_END_SEQUENCE, get_buffer_pt_o (cur_bp), 0, 0);

  return bool_to_lisp (ret);
}

DEFUN ("transpose-chars", transpose_chars)
/*+
Interchange characters around point, moving forward one character.
With prefix arg ARG, effect is to take character before point
and drag it forward past ARG other characters (backward if ARG negative).
If no argument and at end of line, the previous two chars are exchanged.
+*/
{
  ok = transpose (uniarg, forward_char, backward_char);
}
END_DEFUN

DEFUN ("transpose-words", transpose_words)
/*+
Interchange words around point, leaving point at end of them.
With prefix arg ARG, effect is to take word before or around point
and drag it forward past ARG other words (backward if ARG negative).
If ARG is zero, the words around or after point and around or after mark
are interchanged.
+*/
{
  ok = transpose (uniarg, forward_word, backward_word);
}
END_DEFUN

DEFUN ("transpose-sexps", transpose_sexps)
/*+
Like @kbd{M-x transpose-words} but applies to sexps.
+*/
{
  ok = transpose (uniarg, forward_sexp, backward_sexp);
}
END_DEFUN

DEFUN ("transpose-lines", transpose_lines)
/*+
Exchange current line and previous line, leaving point after both.
With argument ARG, takes previous line and moves it past ARG lines.
With argument 0, interchanges line point is in with line mark is in.
+*/
{
  ok = transpose (uniarg, next_line, previous_line);
}
END_DEFUN


static le *
mark (int uniarg, Function func)
{
  le * ret;
  FUNCALL (set_mark_command);
  ret = func (uniarg, true, NULL);
  if (ret)
    FUNCALL (exchange_point_and_mark);
  return ret;
}

DEFUN ("mark-word", mark_word)
/*+
Set mark argument words away from point.
+*/
{
  ok = mark (uniarg, F_forward_word);
}
END_DEFUN

DEFUN ("mark-sexp", mark_sexp)
/*+
Set mark @i{arg} sexps from point.
The place mark goes is the same place @kbd{C-M-f} would
move to with the same argument.
+*/
{
  ok = mark (uniarg, F_forward_sexp);
}
END_DEFUN

DEFUN_ARGS ("forward-line", forward_line,
            INT_OR_UNIARG (n))
/*+
Move N lines forward (backward if N is negative).
Precisely, if point is on line I, move to the start of line I + N.
+*/
{
  INT_OR_UNIARG_INIT (n);
  if (ok == leT)
    {
      FUNCALL (beginning_of_line);
      ok = execute_with_uniarg (false, n, next_line, previous_line);
    }
}
END_DEFUN

static le *
move_paragraph (int uniarg, bool (*forward) (void), bool (*backward) (void),
                     Function line_extremum)
{
  if (uniarg < 0)
    {
      uniarg = -uniarg;
      forward = backward;
    }

  while (uniarg-- > 0)
    {
      while (is_empty_line () && forward ())
        ;
      while (!is_empty_line () && forward ())
        ;
    }

  if (is_empty_line ())
    FUNCALL (beginning_of_line);
  else
    line_extremum (1, false, NULL);

  return leT;
}

DEFUN ("backward-paragraph", backward_paragraph)
/*+
Move backward to start of paragraph.  With argument N, do it N times.
+*/
{
  ok = move_paragraph (uniarg, previous_line, next_line, F_beginning_of_line);
}
END_DEFUN

DEFUN ("forward-paragraph", forward_paragraph)
/*+
Move forward to end of paragraph.  With argument N, do it N times.
+*/
{
  ok = move_paragraph (uniarg, next_line, previous_line, F_end_of_line);
}
END_DEFUN

DEFUN ("mark-paragraph", mark_paragraph)
/*+
Put point at beginning of this paragraph, mark at end.
The paragraph marked is the one that contains point or follows point.
+*/
{
  if (last_command () == F_mark_paragraph)
    {
      FUNCALL (exchange_point_and_mark);
      FUNCALL_ARG (forward_paragraph, uniarg);
      FUNCALL (exchange_point_and_mark);
    }
  else
    {
      FUNCALL_ARG (forward_paragraph, uniarg);
      FUNCALL (set_mark_command);
      FUNCALL_ARG (backward_paragraph, uniarg);
    }
}
END_DEFUN

DEFUN ("fill-paragraph", fill_paragraph)
/*+
Fill paragraph at or after point.
+*/
{
  Marker *m = point_marker ();

  undo_save (UNDO_START_SEQUENCE, get_buffer_pt_o (cur_bp), 0, 0);

  FUNCALL (forward_paragraph);
  int end = get_buffer_pt (cur_bp).n;
  if (is_empty_line ())
    end--;

  FUNCALL (backward_paragraph);
  int start = get_buffer_pt (cur_bp).n;
  if (is_empty_line ())
    { /* Move to next line if between two paragraphs. */
      next_line ();
      start++;
    }

  for (int i = start; i < end; i++)
    {
      FUNCALL (end_of_line);
      delete_char ();
      FUNCALL (just_one_space);
    }

  FUNCALL (end_of_line);
  while (get_goalc () > (size_t) get_variable_number ("fill-column") + 1
         && fill_break_line ())
    ;

  goto_point (get_marker_pt (m));
  unchain_marker (m);

  undo_save (UNDO_END_SEQUENCE, get_buffer_pt_o (cur_bp), 0, 0);
}
END_DEFUN

static bool
setcase_word (int rcase)
{
  if (!ISWORDCHAR (following_char ()))
    if (!forward_word () || !backward_word ())
      return false;

  astr as = astr_new ();
  char c;
  for (size_t i = get_buffer_pt (cur_bp).o;
       i < get_buffer_line_len (cur_bp) &&
         ISWORDCHAR ((int) (c = astr_get (get_buffer_text (cur_bp).as, get_buffer_line_o (cur_bp) + i)));
       i++)
    astr_cat_char (as, c);

  if (astr_len (as) > 0)
    {
      undo_save (UNDO_START_SEQUENCE, get_buffer_pt_o (cur_bp), 0, 0);
      astr_recase (as, rcase);
      for (size_t i = 0; i < astr_len (as); i++)
        delete_char ();
      bprintf ("%s", astr_cstr (as));
      undo_save (UNDO_END_SEQUENCE, get_buffer_pt_o (cur_bp), 0, 0);
    }

  set_buffer_modified (cur_bp, true);

  return true;
}

static bool
setcase_word_lowercase (void)
{
  return setcase_word (case_lower);
}

DEFUN_ARGS ("downcase-word", downcase_word,
            INT_OR_UNIARG (arg))
/*+
Convert following word (or @i{arg} words) to lower case, moving over.
+*/
{
  INT_OR_UNIARG_INIT (arg);
  ok = execute_with_uniarg (true, arg, setcase_word_lowercase, NULL);
}
END_DEFUN

static bool
setcase_word_uppercase (void)
{
  return setcase_word (case_upper);
}

DEFUN_ARGS ("upcase-word", upcase_word,
            INT_OR_UNIARG (arg))
/*+
Convert following word (or @i{arg} words) to upper case, moving over.
+*/
{
  INT_OR_UNIARG_INIT (arg);
  ok = execute_with_uniarg (true, arg, setcase_word_uppercase, NULL);
}
END_DEFUN

static bool
setcase_word_capitalize (void)
{
  return setcase_word (case_capitalized);
}

DEFUN_ARGS ("capitalize-word", capitalize_word,
            INT_OR_UNIARG (arg))
/*+
Capitalize the following word (or @i{arg} words), moving over.
This gives the word(s) a first character in upper case
and the rest lower case.
+*/
{
  INT_OR_UNIARG_INIT (arg);
  ok = execute_with_uniarg (true, arg, setcase_word_capitalize, NULL);
}
END_DEFUN

/*
 * Set the region case.
 */
static le *
setcase_region (int (*func) (int))
{
  if (warn_if_readonly_buffer () || warn_if_no_mark ())
    return leNIL;

  Region r = calculate_the_region ();
  undo_save (UNDO_START_SEQUENCE, r.start, 0, 0);

  Marker *m = point_marker ();
  goto_offset (r.start);
  for (size_t size = get_region_size (r); size > 0; size--)
    {
      char c = func (following_char ());
      delete_char ();
      type_char (c, get_buffer_overwrite (cur_bp));
    }
  goto_point (get_marker_pt (m));
  unchain_marker (m);

  undo_save (UNDO_END_SEQUENCE, r.start, 0, 0);

  return leT;
}

DEFUN ("upcase-region", upcase_region)
/*+
Convert the region to upper case.
+*/
{
  ok = setcase_region (toupper);
}
END_DEFUN

DEFUN ("downcase-region", downcase_region)
/*+
Convert the region to lower case.
+*/
{
  ok = setcase_region (tolower);
}
END_DEFUN

static void
write_shell_output (va_list ap)
{
  insert_estr ((estr) {.as = va_arg (ap, astr), .eol = coding_eol_lf});
}

static bool
pipe_command (const char *cmd, const char *tempfile, bool do_insert, bool do_replace)
{
  astr out;
  bool more_than_one_line = false;
  char *cmdline, *eol;
  FILE * fh;

  cmdline = xasprintf ("%s 2>&1 <%s", cmd, tempfile);
  fh = popen (cmdline, "r");
  if (fh == NULL)
    {
      minibuf_error ("Cannot open pipe to process");
      return false;
    }

  out = astr_fread (fh);
  pclose (fh);
  eol = strchr (astr_cstr (out), '\n');
  if (eol && eol != astr_cstr (out) + astr_len (out) - 1)
    more_than_one_line = true;

  if (astr_len (out) == 0)
    minibuf_write ("(Shell command succeeded with no output)");
  else
    {
      if (do_insert)
        {
          if (do_replace)
            {
              undo_save (UNDO_START_SEQUENCE, get_buffer_pt_o (cur_bp), 0, 0);
              FUNCALL (delete_region);
            }
          bprintf ("%s", astr_cstr (out));
          if (do_replace)
            undo_save (UNDO_END_SEQUENCE, get_buffer_pt_o (cur_bp), 0, 0);
        }
      else
        {
          write_temp_buffer ("*Shell Command Output*", more_than_one_line,
                             write_shell_output, out);
          if (!more_than_one_line)
            minibuf_write ("%s", astr_cstr (out));
        }
    }

  return true;
}

static castr
minibuf_read_shell_command (void)
{
  castr ms = minibuf_read ("Shell command: ", "");

  if (ms == NULL)
    {
      FUNCALL (keyboard_quit);
      return NULL;
    }
  if (astr_len (ms) == 0)
    return NULL;

  return ms;
}

DEFUN_ARGS ("shell-command", shell_command,
            STR_ARG (cmd)
            BOOL_ARG (insert))
/*+
Execute string @i{command} in inferior shell; display output, if any.
With prefix argument, insert the command's output at point.

Command is executed synchronously.  The output appears in the buffer
`*Shell Command Output*'.  If the output is short enough to display
in the echo area, it is shown there, but it is nonetheless available
in buffer `*Shell Command Output*' even though that buffer is not
automatically displayed.

The optional second argument @i{output-buffer}, if non-nil,
says to insert the output in the current buffer.
+*/
{
  STR_INIT (cmd)
  else
    cmd = minibuf_read_shell_command ();
  BOOL_INIT (insert)
  else
    insert = lastflag & FLAG_SET_UNIARG;

  if (cmd != NULL)
    ok = bool_to_lisp (pipe_command (astr_cstr (cmd), "/dev/null", insert, false));
}
END_DEFUN

/* The `start' and `end' arguments are fake, hence their string type,
   so they can be ignored. */
DEFUN_ARGS ("shell-command-on-region", shell_command_on_region,
            STR_ARG (start)
            STR_ARG (end)
            STR_ARG (cmd)
            BOOL_ARG (insert))
/*+
Execute string command in inferior shell with region as input.
Normally display output (if any) in temp buffer `*Shell Command Output*';
Prefix arg means replace the region with it.  Return the exit code of
command.

If the command generates output, the output may be displayed
in the echo area or in a buffer.
If the output is short enough to display in the echo area, it is shown
there.  Otherwise it is displayed in the buffer `*Shell Command Output*'.
The output is available in that buffer in both cases.
+*/
{
  STR_INIT (start);
  STR_INIT (end);
  STR_INIT (cmd)
  else
    cmd = minibuf_read_shell_command ();
  BOOL_INIT (insert)
  else
    insert = lastflag & FLAG_SET_UNIARG;

  if (cmd != NULL)
    {

      if (warn_if_no_mark ())
        ok = leNIL;
      else
        {
          Region r = calculate_the_region ();
          char tempfile[] = P_tmpdir "/zileXXXXXX";
          int fd = mkstemp (tempfile);

          if (fd == -1)
            {
              minibuf_error ("Cannot open temporary file");
              ok = leNIL;
            }
          else
            {
              ssize_t written = write (fd, astr_cstr (get_buffer_region (cur_bp, r).as), get_region_size (r));

              if (written != (ssize_t) get_region_size (r))
                {
                  if (written == -1)
                    minibuf_error ("Error writing to temporary file: %s",
                                   strerror (errno));
                  else
                    minibuf_error ("Error writing to temporary file");
                  ok = leNIL;
                }
              else
                ok = bool_to_lisp (pipe_command (astr_cstr (cmd), tempfile, insert, true));

              close (fd);
              remove (tempfile);
            }
        }
    }
}
END_DEFUN

DEFUN ("delete-region", delete_region)
/*+
Delete the text between point and mark.
+*/
{
  if (warn_if_no_mark () || !delete_region (calculate_the_region ()))
    ok = leNIL;
  else
    deactivate_mark ();
}
END_DEFUN

DEFUN ("delete-blank-lines", delete_blank_lines)
/*+
On blank line, delete all surrounding blank lines, leaving just one.
On isolated blank line, delete that one.
On nonblank line, delete any immediately following blank lines.
+*/
{
  Marker *m = point_marker ();
  int seq_started = false;

  /* Delete any immediately following blank lines.  */
  if (next_line ())
    {
      if (is_blank_line ())
        {
          push_mark ();
          FUNCALL (beginning_of_line);
          set_mark ();
          activate_mark ();
          while (FUNCALL (forward_line) == leT && is_blank_line ())
            ;
          seq_started = true;
          undo_save (UNDO_START_SEQUENCE, point_to_offset (get_marker_pt (m)), 0, 0);
          FUNCALL (delete_region);
          pop_mark ();
        }
      previous_line ();
    }

  /* Delete any immediately preceding blank lines.  */
  if (is_blank_line ())
    {
      int forward = true;
      push_mark ();
      FUNCALL (beginning_of_line);
      set_mark ();
      activate_mark ();
      do
        {
          if (!FUNCALL_ARG (forward_line, -1))
            {
              forward = false;
              break;
            }
        }
      while (is_blank_line ());
      if (forward)
        FUNCALL (forward_line);
      if (get_buffer_pt (cur_bp).n != get_marker_pt (m).n)
        {
          if (!seq_started)
            {
              seq_started = true;
              undo_save (UNDO_START_SEQUENCE, point_to_offset (get_marker_pt (m)), 0, 0);
            }
          FUNCALL (delete_region);
        }
      pop_mark ();
    }

  /* Isolated blank line, delete that one.  */
  if (!seq_started && is_blank_line ())
    {
      push_mark ();
      FUNCALL (beginning_of_line);
      set_mark ();
      activate_mark ();
      FUNCALL (forward_line);
      FUNCALL (delete_region);	/* Just one action, without a
                                   sequence. */
      pop_mark ();
    }

  goto_point (get_marker_pt (m));

  if (seq_started)
    undo_save (UNDO_END_SEQUENCE, get_buffer_pt_o (cur_bp), 0, 0);

  unchain_marker (m);
  deactivate_mark ();
}
END_DEFUN
