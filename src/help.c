/* Self documentation facility functions
   Copyright (c) 1997-2004 Sandro Sigala.
   Copyright (c) 2003-2004 Reuben Thomas.
   All rights reserved.

   This file is part of Zile.

   Zile is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2, or (at your option) any later
   version.

   Zile is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with Zile; see the file COPYING.  If not, write to the Free
   Software Foundation, 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/*	$Id: help.c,v 1.21 2004/11/14 21:05:04 rrt Exp $	*/

#include "config.h"

#include <assert.h>
#include <ctype.h>
#if HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zile.h"
#include "extern.h"
#include "paths.h"

DEFUN("zile-version", zile_version)
/*+
Show the zile version.
+*/
{
	minibuf_write("Zile " VERSION " of " CONFIGURE_DATE " on " CONFIGURE_HOST);

	return TRUE;
}

static int minihelp_page = 1;

/*
 * Replace each occurrence of `C-h' in buffer with `M-h'.
 */
static void fix_alternative_keys(Buffer *bp)
{
	Line *lp;
	int i;
	for (lp = bp->limitp->next; lp != bp->limitp; lp = lp->next)
		for (i = 0; i < astr_len(lp->text) - 2; i++)
			if (*astr_char(lp->text, i) == 'C' && *astr_char(lp->text, i + 1) == '-' &&
                            *astr_char(lp->text, i + 2) == 'h')
                                *astr_char(lp->text, i) = 'M', i += 2;
}

/*
 * Switch to the `bp' buffer and replace any contents with the current
 * Mini Help page (read from disk).
 */
static int read_minihelp_page(Buffer *bp)
{
	astr fname;
	int delta;

	switch_to_buffer(bp);
	zap_buffer_content();
	bp->flags = BFLAG_NOUNDO | BFLAG_READONLY | BFLAG_NOSAVE
		| BFLAG_NEEDNAME | BFLAG_TEMPORARY | BFLAG_MODIFIED;
	set_temporary_buffer(bp);

	fname = astr_new();
	astr_afmt(fname, "%s%d", PATH_DATA "/MINIHELP", minihelp_page);
	if (!exist_file(astr_cstr(fname))) {
		minihelp_page = 1;
                astr_truncate(fname, 0);
		astr_afmt(fname, "%s%d", PATH_DATA "/MINIHELP",
                          minihelp_page);
		if (!exist_file(astr_cstr(fname))) {
			minibuf_error("Unable to read file `%s'",
				      astr_cstr(fname));
			astr_delete(fname);
			return FALSE;
		}
	}

	read_from_disk(astr_cstr(fname));
	if (lookup_bool_variable("alternative-bindings"))
		fix_alternative_keys(bp);
	gotobob();

	while ((delta = cur_wp->eheight - head_wp->bp->num_lines) < 1) {
		FUNCALL(enlarge_window);
		/* Break if cannot enlarge further. */
		if (delta == cur_wp->eheight - head_wp->bp->num_lines)
			break;
	}

	astr_delete(fname);

	return TRUE;
}

DEFUN("minihelp-toggle-window", minihelp_toggle_window)
/*+
Toggle the mini help window.
+*/
{
	const char *bname = "*Mini Help*";
	Window *wp;
	int delta;

	if ((wp = find_window(bname)) != NULL) {
		set_current_window(wp);
		FUNCALL(delete_window);
	} else {
		FUNCALL(delete_other_windows);
		FUNCALL(split_window);
		set_current_window(head_wp);
		read_minihelp_page(find_buffer(bname, TRUE));
		set_current_window(head_wp->next);
		while ((delta = head_wp->eheight - head_wp->bp->num_lines) > 1) {
			FUNCALL(enlarge_window);
			/* Break if cannot enlarge further. */
			if (delta == head_wp->eheight - head_wp->bp->num_lines)
				break;
		}
	}

	return TRUE;
}

DEFUN("minihelp-rotate-contents", minihelp_rotate_contents)
/*+
Show the next mini help entry.
+*/
{
	const char *bname = "*Mini Help*";

	if (find_window(bname) == NULL)
		FUNCALL(minihelp_toggle_window);
	else { /* Easy hack */
		FUNCALL(minihelp_toggle_window);
		++minihelp_page;
		FUNCALL(minihelp_toggle_window);
	}

	return TRUE;
}

static int show_file(char *filename)
{
	if (!exist_file(filename)) {
		minibuf_error("Unable to read file `%s'", filename);
		return FALSE;
	}

	open_file(filename, 0);
	cur_bp->flags = BFLAG_READONLY | BFLAG_NOSAVE | BFLAG_NEEDNAME
		| BFLAG_NOUNDO;

	return TRUE;
}

DEFUN("help", help)
/*+
Show a help window.
+*/
{
	return show_file(PATH_DATA "/HELP");
}

DEFUN("help-config-sample", help_config_sample)
/*+
Show a configuration file sample.
+*/
{
	return show_file(PATH_DATA "/zilerc.sample");
}

DEFUN("help-faq", help_faq)
/*+
Show the Zile Frequently Asked Questions (FAQ).
+*/
{
	return show_file(PATH_DATA "/FAQ");
}

DEFUN("help-tutorial", help_tutorial)
/*+
Show a tutorial window.
+*/
{
	if (show_file(PATH_DATA "/TUTORIAL")) {
		astr buf;
		buf = astr_new();
		cur_bp->flags = 0;
		astr_cpy_cstr(buf, getenv("HOME"));
		astr_cat_cstr(buf, "/TUTORIAL");
		set_buffer_filename(cur_bp, astr_cstr(buf));

		astr_delete(buf);
		return TRUE;
	}

	return FALSE;
}

/*
 * Fetch the documentation of a function or variable from the
 * AUTODOC automatically generated file.
 */
static astr get_funcvar_doc(char *name, astr defval, int isfunc)
{
	FILE *f;
	astr buf, match, doc;
	int reading_doc = 0;

	if ((f = fopen(PATH_DATA "/AUTODOC", "r")) == NULL) {
		minibuf_error("Unable to read file `%s'",
			      PATH_DATA "/AUTODOC");
		return NULL;
	}

	match = astr_new();
	if (isfunc)
		astr_afmt(match, "\fF_%s", name);
	else
		astr_afmt(match, "\fV_%s", name);

	doc = astr_new();
	while ((buf = astr_fgets(f)) != NULL) {
                if (reading_doc) {
                        if (*astr_char(buf, 0) == '\f') {
                                astr_delete(buf);
                                break;
                        }
			if (isfunc || astr_len(defval) > 0) {
				astr_cat(doc, buf);
				astr_cat_cstr(doc, "\n");
			} else
				astr_cpy(defval, buf);
		} else if (!astr_cmp(buf, match))
			reading_doc = 1;
                astr_delete(buf);
        }

	fclose(f);
	astr_delete(match);

	if (!reading_doc) {
		minibuf_error("Cannot find documentation for `%s'", name);
		astr_delete(doc);
		return NULL;
	}

	return doc;
}

static void write_function_description(va_list ap)
{
	const char *name = va_arg(ap, const char *);
	astr doc = va_arg(ap, astr);

	bprintf("Function: %s\n\n"
		"Documentation:\n%s",
		name, astr_cstr(doc));
}

DEFUN("describe-function", describe_function)
/*+
Display the full documentation of a function.
+*/
{
	char *name;
	astr bufname, doc;

	name = minibuf_read_function_name("Describe function: ");
	if (name == NULL)
		return FALSE;

	if ((doc = get_funcvar_doc(name, NULL, TRUE)) == NULL)
		return FALSE;

	bufname = astr_new();
	astr_afmt(bufname, "*Help: function `%s'*", name);
	write_temp_buffer(astr_cstr(bufname), write_function_description,
			  name, doc);
	astr_delete(bufname);
	astr_delete(doc);

	return TRUE;
}

static void write_variable_description(va_list ap)
{
	char *name = va_arg(ap, char *);
	astr defval = va_arg(ap, astr);
	astr doc = va_arg(ap, astr);
	bprintf("Variable: %s\n\n"
		"Default value: %s\n"
		"Current value: %s\n\n"
		"Documentation:\n%s",
		name, astr_cstr(defval), get_variable(name), astr_cstr(doc));
}

DEFUN("describe-variable", describe_variable)
/*+
Display the full documentation of a variable.
+*/
{
	char *name;
	astr bufname, defval, doc;

	name = minibuf_read_variable_name("Describe variable: ");
	if (name == NULL)
		return FALSE;

	defval = astr_new();
	if ((doc = get_funcvar_doc(name, defval, FALSE)) == NULL) {
                astr_delete(defval);
		return FALSE;
        }

	bufname = astr_new();
	astr_afmt(bufname, "*Help: variable `%s'*", name);
	write_temp_buffer(astr_cstr(bufname), write_variable_description,
			  name, defval, doc);
	astr_delete(bufname);
	astr_delete(doc);
	astr_delete(defval);

	return TRUE;
}

DEFUN("describe-key", describe_key)
/*+
Display documentation of the command invoked by a key sequence.
+*/
{
	char *name;
	astr bufname, doc;

	minibuf_write("Describe key:");
	if ((name = get_function_by_key_sequence()) == NULL) {
		minibuf_error("Key sequence is undefined");
		return FALSE;
	}

	minibuf_write("Key sequence runs the command `%s'", name);

	if ((doc = get_funcvar_doc(name, NULL, TRUE)) == NULL)
		return FALSE;

	bufname = astr_new();
	astr_afmt(bufname, "*Help: function `%s'*", name);
	write_temp_buffer(astr_cstr(bufname), write_function_description,
			  name, doc);
	astr_delete(bufname);
	astr_delete(doc);

	return TRUE;
}
