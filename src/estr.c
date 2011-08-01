/* Dynamically allocated encoded strings

   Copyright (c) 2011 Free Software Foundation, Inc.

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

#include <stdbool.h>
#include <string.h>
#include "size_max.h"

#include "astr.h"
#include "estr.h"
#include "memrmem.h"


/* Formats of end-of-line. */
const char *coding_eol_lf = "\n";
const char *coding_eol_crlf = "\r\n";
const char *coding_eol_cr = "\r";

/* Maximum number of EOLs to check before deciding type. */
#define MAX_EOL_CHECK_COUNT 3
estr
estr_new_astr (castr as)
{
  bool first_eol = true;
  size_t total_eols = 0;
  estr es = (estr) {.as = as, .eol = coding_eol_lf};
  for (size_t i = 0; i < astr_len (as) && total_eols < MAX_EOL_CHECK_COUNT; i++)
    {
      char c = astr_get (as, i);
      if (c == '\n' || c == '\r')
        {
          const char *this_eol_type;
          total_eols++;
          if (c == '\n')
            this_eol_type = coding_eol_lf;
          else if (i == astr_len (as) - 1 || astr_get (as, i + 1) != '\n')
            this_eol_type = coding_eol_cr;
          else
            {
              this_eol_type = coding_eol_crlf;
              i++;
            }

          if (first_eol)
            { /* This is the first end-of-line. */
              es.eol = this_eol_type;
              first_eol = false;
            }
          else if (es.eol != this_eol_type)
            { /* This EOL is different from the last; arbitrarily choose LF. */
              es.eol = coding_eol_lf;
              break;
            }
        }
    }
  return es;
}

size_t
estr_prev_line (estr es, size_t o)
{
  size_t so = estr_start_of_line (es, o);
  return (so == 0) ? SIZE_MAX : estr_start_of_line (es, so - strlen (es.eol));
}

size_t
estr_next_line (estr es, size_t o)
{
  size_t eo = estr_end_of_line (es, o);
  return (eo == astr_len (es.as)) ? SIZE_MAX : eo + strlen (es.eol);
}

size_t
estr_start_of_line (estr es, size_t o)
{
  size_t eol_len = strlen (es.eol);
  const char *prev = memrmem (astr_cstr (es.as), o, es.eol, eol_len);
  return prev ? prev - astr_cstr (es.as) + eol_len : 0;
}

size_t
estr_end_of_line (estr es, size_t o)
{
  const char *next = memmem (astr_cstr (es.as) + o, astr_len (es.as) - o,
                             es.eol, strlen (es.eol));
  return next ? (size_t) (next - astr_cstr (es.as)) : astr_len (es.as);
}

size_t
estr_line_len (estr es, size_t o)
{
  return estr_end_of_line (es, o) - estr_start_of_line (es, o);
}

estr
estr_replace (estr es, size_t pos, size_t del, estr ins)
{
  astr_remove (es.as, pos, del);

  const char *s = astr_cstr (ins.as);
  size_t ins_eol_len = strlen (ins.eol), es_eol_len = strlen (es.eol);
  for (size_t len = astr_len (ins.as); len > 0;)
    {
      const char *next = memmem (s, len, ins.eol, ins_eol_len);
      size_t line_len = next ? (size_t) (next - s) : len;
      astr_insert (es.as, pos, line_len);
      astr_replace_nstr (es.as, pos, s, line_len);
      pos += line_len;
      len -= line_len;
      s = next;
      if (len > 0)
        {
          astr_insert (es.as, pos, es_eol_len);
          astr_replace_nstr (es.as, pos, es.eol, es_eol_len);
          s += ins_eol_len;
          len -= ins_eol_len;
          pos += es_eol_len;
        }
    }
  return es;
}

estr
estr_cat (estr es, estr src)
{
  return estr_replace (es, astr_len (es.as), 0, src);
}

estr
estr_readf (const char *filename)
{
  estr es = (estr) {.as = NULL};
  astr as = astr_readf (filename);
  if (as)
    es = estr_new_astr (as);
  return es;
}