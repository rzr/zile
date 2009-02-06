/* Key bindings and extended commands

   Copyright (c) 2008, 2009 Free Software Foundation, Inc.
   Copyright (c) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004 Sandro Sigala.
   Copyright (c) 2003, 2004, 2005, 2006, 2007, 2008 Reuben Thomas.

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

#include "config.h"

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gl_array_list.h"
#include "gl_linked_list.h"

#include "zile.h"
#include "extern.h"

/*--------------------------------------------------------------------------
 * Key binding.
 *--------------------------------------------------------------------------*/

struct Binding
{
  size_t key; /* The key code (for every level except the root). */
  Function func; /* The function for this key (if a leaf node). */

  /* Branch vector, number of items, max number of items. */
  Binding *vec;
  size_t vecnum, vecmax;
};

Binding root_bindings;

static Binding
node_new (int vecmax)
{
  Binding p = (Binding) XZALLOC (struct Binding);

  p->vecmax = vecmax;
  p->vec = (Binding *) XCALLOC (vecmax, struct Binding);

  return p;
}

static Binding
search_node (Binding tree, size_t key)
{
  size_t i;

  for (i = 0; i < tree->vecnum; ++i)
    if (tree->vec[i]->key == key)
      return tree->vec[i];

  return NULL;
}

static void
add_node (Binding tree, Binding p)
{
  size_t i;

  /* Erase any previous binding the current key might have had in case
     it was non-prefix and is now being made prefix, as we don't want
     to accidentally create a default for the prefix map. */
  if (tree->vecnum == 0)
    tree->func = NULL;

  /* Reallocate vector if there is not enough space. */
  if (tree->vecnum + 1 >= tree->vecmax)
    {
      tree->vecmax += 5;
      tree->vec = (Binding *) xrealloc (tree->vec, sizeof (*p) * tree->vecmax);
    }

  /* Insert the node at the sorted position. */
  for (i = 0; i < tree->vecnum; i++)
    if (tree->vec[i]->key > p->key)
      {
        memmove (&tree->vec[i + 1], &tree->vec[i],
                 sizeof (p) * (tree->vecnum - i));
        tree->vec[i] = p;
        break;
      }
  if (i == tree->vecnum)
    tree->vec[tree->vecnum] = p;
  ++tree->vecnum;
}

static void
bind_key_vec (Binding tree, gl_list_t keys, size_t from, Function func)
{
  Binding p, s = search_node (tree, (size_t) gl_list_get_at (keys, from));
  size_t n = gl_list_size (keys) - from;

  if (s == NULL)
    {
      p = node_new (n == 1 ? 1 : 5);
      p->key = (size_t) gl_list_get_at (keys, from);
      add_node (tree, p);
      if (n == 1)
        p->func = func;
      else if (n > 0)
        bind_key_vec (p, keys, from + 1, func);
    }
  else if (n > 1)
    bind_key_vec (s, keys, from + 1, func);
  else
    s->func = func;
}

static void
bind_key_string (Binding bindings, char *key, Function func)
{
  gl_list_t keys = keystrtovec (key);

  if (keys && gl_list_size (keys) > 0)
    bind_key_vec (bindings, keys, 0, func);
  else
    {
      fprintf (stderr,
               "%s: Key sequence %s is invalid in default bindings\n\n",
               prog_name, key);
      exit (1);
    }
  gl_list_free (keys);
}

static Binding
search_key (Binding tree, gl_list_t keys, size_t from)
{
  Binding p = search_node (tree, (size_t) gl_list_get_at (keys, from));

  if (p != NULL)
    {
      if (gl_list_size (keys) - from == 1)
        return p;
      else
        return search_key (p, keys, from + 1);
    }

  return NULL;
}

size_t
do_binding_completion (astr as)
{
  size_t key;
  astr bs = astr_new ();

  if (lastflag & FLAG_SET_UNIARG)
    {
      int arg = last_uniarg;

      if (arg < 0)
        {
          astr_cat_cstr (bs, "- ");
          arg = -arg;
        }

      do {
        astr_insert_char (bs, 0, ' ');
        astr_insert_char (bs, 0, arg % 10 + '0');
        arg /= 10;
      } while (arg != 0);
    }

  minibuf_write ("%s%s%s",
                 lastflag & (FLAG_SET_UNIARG | FLAG_UNIARG_EMPTY) ? "C-u " : "",
                 astr_cstr (bs),
                 astr_cstr (as));
  astr_delete (bs);
  key = getkey ();
  minibuf_clear ();

  return key;
}

static astr
make_completion (gl_list_t keys)
{
  astr as = astr_new (), key;
  size_t i, len = 0;

  for (i = 0; i < gl_list_size (keys); i++)
    {
      if (i > 0)
        {
          astr_cat_char (as, ' ');
          len++;
        }
      key = chordtostr ((size_t) gl_list_get_at (keys, i));
      astr_cat (as, key);
      astr_delete (key);
    }

  return astr_cat_char (as, '-');
}

static Function
completion_scan (Binding bindings, size_t key, gl_list_t * keys)
{
  *keys = gl_list_create_empty (GL_ARRAY_LIST,
                                NULL, NULL, NULL, true);

  gl_list_add_last (*keys, (void *) key);

  for (;;)
    {
      astr as;
      Binding p = search_key (bindings, *keys, 0);
      if (p == NULL)
        return NULL;
      if (p->func != NULL)
        return p->func;
      as = make_completion (*keys);
      gl_list_add_last (*keys, (void *) do_binding_completion (as));
      astr_delete (as);
    }
}

static int
self_insert_command (void)
{
  int ret = true;
  /* Mask out ~KBD_CTRL to allow control sequences to be themselves. */
  int key = (int) (lastkey () & ~KBD_CTRL);
  deactivate_mark ();
  if (key <= 0xff)
    {
      if (isspace (key) && cur_bp->flags & BFLAG_AUTOFILL &&
          get_goalc () > (size_t) get_variable_number ("fill-column"))
        fill_break_line ();
      insert_char (key);
    }
  else
    {
      ding ();
      ret = false;
    }

  return ret;
}

DEFUN ("self-insert-command", self_insert_command)
/*+
Insert the character you type.
Whichever character you type to run this command is inserted.
+*/
{
  ok = execute_with_uniarg (true, uniarg, self_insert_command, NULL);
}
END_DEFUN

static Function _last_command;

void
process_key (Binding bindings, size_t key)
{
  if (key == KBD_NOKEY)
    return;

  if (key & KBD_META && isdigit ((int) (key & 0xff)))
    /* Got an ESC x sequence where `x' is a digit. */
    universal_argument (KBD_META, (int) ((key & 0xff) - '0'));
  else
    {
      gl_list_t keys;
      Function f = completion_scan (bindings, key, &keys);
      if (f != NULL)
        {
          f (last_uniarg, NULL);
          _last_command = f;
        }
      else
        {
          astr as = keyvectostr (keys);
          minibuf_error ("%s is undefined", astr_cstr (as));
          astr_delete (as);
        }
      gl_list_free (keys);
    }

  /* Only add keystrokes if we were already in macro defining mode
     before the function call, to cope with start-kbd-macro. */
  if (lastflag & FLAG_DEFINING_MACRO && thisflag & FLAG_DEFINING_MACRO)
    add_cmd_to_macro ();
}

Function
last_command (void)
{
  return _last_command;
}

Binding
init_bindings (void)
{
  return node_new (10);
}

void
init_default_bindings (void)
{
  size_t i;
  gl_list_t keys = gl_list_create_empty (GL_ARRAY_LIST,
                                         NULL, NULL, NULL, true);

  root_bindings = init_bindings ();

  /* Bind all printing keys to self_insert_command */
  gl_list_add_last (keys, NULL);
  for (i = 0; i <= 0xff; i++)
    {
      if (isprint (i))
        {
          gl_list_set_at (keys, 0, (void *) i);
          bind_key_vec (root_bindings, keys, 0, F_self_insert_command);
        }
    }
  gl_list_free (keys);

#define X(c_name, key)                 \
  bind_key_string (root_bindings, key, F_ ## c_name);
#include "tbl_bind.h"
#undef X
}

void
free_bindings (Binding binding)
{
  size_t i;
  for (i = 0; i < binding->vecnum; ++i)
    free_bindings (binding->vec[i]);
  free (binding->vec);
  free (binding);
}

DEFUN_ARGS ("global-set-key", global_set_key,
            STR_ARG (keystr)
            STR_ARG (name))
/*+
Bind a command to a key sequence.
Read key sequence and function name, and bind the function to the key
sequence.
+*/
{
  gl_list_t keys;
  Function func;

  STR_INIT (keystr);
  if (keystr != NULL)
    {
      keys = keystrtovec (keystr);
      if (keys == NULL)
        {
          minibuf_error ("Key sequence %s is invalid", keystr);
          return leNIL;
        }
    }
  else
    {
      astr as;
      size_t key;

      minibuf_write ("Set key globally: ");
      key = getkey ();
      completion_scan (root_bindings, key, &keys);
      as = keyvectostr (keys);
      keystr = xstrdup (astr_cstr (as));
      astr_delete (as);
    }

  STR_INIT (name)
  else
    name = minibuf_read_function_name ("Set key %s to command: ",
                                       keystr);
  if (name == NULL)
    return leNIL;

  func = get_function (name);
  if (func == NULL) /* Possible if called non-interactively */
    {
      minibuf_error ("No such function `%s'", name);
      return leNIL;
    }
  bind_key_vec (root_bindings, keys, 0, func);

  gl_list_free (keys);
  STR_FREE (keystr);
  STR_FREE (name);
}
END_DEFUN

static void
walk_bindings_tree (Binding tree, gl_list_t keys,
                    void (*process) (astr key, Binding p, void *st), void *st)
{
  size_t i, j;
  astr as = chordtostr (tree->key);

  gl_list_add_last (keys, as);

  for (i = 0; i < tree->vecnum; ++i)
    {
      Binding p = tree->vec[i];
      if (p->func != NULL)
        {
          astr key = astr_new ();
          astr as = chordtostr (p->key);
          for (j = 1; j < gl_list_size (keys); j++)
            {
              astr_cat (key, (astr) gl_list_get_at (keys, j));
              astr_cat_char (key, ' ');
            }
          astr_cat_delete (key, as);
          process (key, p, st);
          astr_delete (key);
        }
      else
        walk_bindings_tree (p, keys, process, st);
    }

  astr_delete ((astr) gl_list_get_at (keys, gl_list_size (keys) - 1));
  assert (gl_list_remove_at (keys, gl_list_size (keys) - 1));
}

static void
walk_bindings (Binding tree, void (*process) (astr key, Binding p, void *st),
               void *st)
{
  gl_list_t l = gl_list_create_empty (GL_LINKED_LIST,
                                      NULL, NULL, NULL, true);
  walk_bindings_tree (tree, l, process, st);
  gl_list_free (l);
}

typedef struct
{
  Function f;
  astr bindings;
} gather_bindings_state;

static void
gather_bindings (astr key, Binding p, void *st)
{
  gather_bindings_state *g = (gather_bindings_state *) st;

  if (p->func == g->f)
    {
      if (astr_len (g->bindings) > 0)
        astr_cat_cstr (g->bindings, ", ");
      astr_cat (g->bindings, key);
    }
}

DEFUN ("where-is", where_is)
/*+
Print message listing key sequences that invoke the command DEFINITION.
Argument is a command name.  If the prefix arg is non-nil, insert the
message in the buffer.
+*/
{
  const char *name = minibuf_read_function_name ("Where is command: ");
  gather_bindings_state g;

  ok = leNIL;

  if (name)
    {
      g.f = get_function (name);
      if (g.f)
        {
          g.bindings = astr_new ();
          walk_bindings (root_bindings, gather_bindings, &g);

          if (astr_len (g.bindings) == 0)
            minibuf_write ("%s is not on any key", name);
          else
            {
              astr as = astr_new ();
              astr_afmt (as, "%s is on %s", name, astr_cstr (g.bindings));
              if (lastflag & FLAG_SET_UNIARG)
                bprintf ("%s", astr_cstr (as));
              else
                minibuf_write ("%s", astr_cstr (as));
              astr_delete (as);
            }
          astr_delete (g.bindings);
          ok = leT;
        }
    }

  free ((char *) name);
}
END_DEFUN

const char *
get_function_by_key (size_t key)
{
  Function f;
  gl_list_t keys;

  if (key & KBD_META && isdigit ((int) (key & 0xff)))
    return "universal-argument";

  f = completion_scan (root_bindings, key, &keys);
  gl_list_free (keys);
  return f ? get_function_name (f) : NULL;
}

static void
print_binding (astr key, Binding p, void *st GCC_UNUSED)
{
  bprintf ("%-15s %s\n", astr_cstr (key), get_function_name (p->func));
}

static void
write_bindings_list (va_list ap GCC_UNUSED)
{
  bprintf ("Key translations:\n");
  bprintf ("%-15s %s\n", "key", "binding");
  bprintf ("%-15s %s\n", "---", "-------");

  walk_bindings (root_bindings, print_binding, NULL);
}

DEFUN ("describe-bindings", describe_bindings)
/*+
Show a list of all defined keys, and their definitions.
+*/
{
  write_temp_buffer ("*Help*", true, write_bindings_list);
}
END_DEFUN
