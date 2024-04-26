/*--------------------------------------------------------------------*
 * html.c
 * Aug 05 (PJW)
 *
 * Dec 2022 (Geoff Shuetrim)
 * Adapted to use hyperlinking and MathJax display of equations.
 *
 *--------------------------------------------------------------------*
.. ### html
..
.. Write out the model in HTML for use as documentation.
..
.. + Equation descriptions will be written
 *--------------------------------------------------------------------*/

#include "../codegen.h"
#include "../assoc.h"
#include "../eqns.h"
#include "../error.h"
#include "../lang.h"
#include "../options.h"
#include "../output.h"
#include "../cart.h"
#include "../sets.h"
#include "../str.h"
#include "../sym.h"
#include "../symtable.h"
#include "../xmalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define myDEBUG 1

#define MYOBJ 3001

static int HTML_block = 1;
static int HTML_scalar = 1;

//
//  Internal variables
//

struct htmlset
{
   int obj;
   char *index;
   int istime;
};
typedef struct htmlset Htmlset;

Array *htmlsets = 0;

static char *HTML_css = "a:link { color:blue; } \
body { margin-left:2em; margin-top:2em; margin-right:2em; } \
td { padding-left: 1em; padding-right: 1em; } \
th { text-align: left; padding-left: 1em; padding-right: 1em; } \
div.heading { margin-top: 2em; font-weight: bold; font-size: 120%; } \
div.dblock { margin-top: 0em; margin-left: 0em; margin-right: 0em; } \
div.eblock { margin-top: 1em; overflow-x: scroll;  } \
div.eqn { margin-top: 1em; margin-left: 2em;} \
";

//----------------------------------------------------------------------//
//  html equation LHS and RHS printing.
//
// Geoff Shuetrim (GCS) addition in December 2022.
// Supports HTML printing of equations.
// Should be in the html.c file perhaps...?
//----------------------------------------------------------------------//
char *html_slprint_for_eqnlist(List *list)
{
   char *obuf;
   Item *cur;
   int n, size;
   char *anchorTag;

   if (list->obj != LISTOBJ)
      fatal_error("%s", "argument to html_slprint is not a list");

   if (list->n == 0)
      return "none";

   size = 0;

   for (n = 0, cur = list->first; cur; cur = cur->next)
   {
      if (size)
         size += 2;
      size += 2 * strlen(cur->str) + 19;
   }

   obuf = (char *)xmalloc(size + 1);

   n = 0;
   for (n = 0, cur = list->first; cur; cur = cur->next)
      if (0 == n++)
      {
         anchorTag = concat(5, "<a href='#", cur->str, "'>", cur->str, "</a>");
         strcpy(obuf, anchorTag);
      }
      else
      {
         strcat(obuf, ", ");
         anchorTag = concat(5, "<a href='#", cur->str, "'>", cur->str, "</a>");
         strcat(obuf, anchorTag);
      }

   return obuf;
}

char *lhsAsHtml(void *sym)
{
   if (sym == 0)
      return 0;
   validate(sym, SYMBOBJ, "symdef");
   if (((Symbol *)sym)->leqns->n == 0)
      return "none";
   return html_slprint_for_eqnlist(((Symbol *)sym)->leqns);
}

char *rhsAsHtml(void *sym)
{
   if (sym == 0)
      return 0;
   validate(sym, SYMBOBJ, "symdef");
   if (((Symbol *)sym)->reqns->n == 0)
      return "none";
   return html_slprint_for_eqnlist(((Symbol *)sym)->reqns);
}

//----------------------------------------------------------------------//
//  html_slprint
//
//  Generate a comma-separated string from a list.  Almost identical
//  to slprint but with a space after each comma.  One difference is
//  that this returns an HTML nbsp if the list is empty.
//----------------------------------------------------------------------//
char *html_slprint(list)
List *list;
{
   char *obuf;
   Item *cur;
   int n, size;

   if (list->obj != LISTOBJ)
      fatal_error("%s", "argument to html_slprint is not a list");

   if (list->n == 0)
      return xstrdup("&nbsp;");

   size = 0;

   for (n = 0, cur = list->first; cur; cur = cur->next)
   {
      if (size)
         size += 2;
      size += strlen(cur->str);
   }

   obuf = (char *)xmalloc(size + 1);

   n = 0;
   for (n = 0, cur = list->first; cur; cur = cur->next)
      if (0 == n++)
         strcpy(obuf, cur->str);
      else
      {
         strcat(obuf, ", ");
         strcat(obuf, cur->str);
      }

   return obuf;
}

char *html_slprint_for_setlist(list)
List *list;
{
   char *obuf;
   Item *cur;
   int n, size;
   char *anchorTag;

   if (list->obj != LISTOBJ)
      fatal_error("%s", "argument to html_slprint is not a list");

   if (list->n == 0)
      return xstrdup("&nbsp;");

   size = 0;

   for (n = 0, cur = list->first; cur; cur = cur->next)
   {
      if (size)
         size += 2;
      size += 2 * strlen(cur->str) + 19;
   }

   obuf = (char *)xmalloc(size + 1);

   n = 0;
   for (n = 0, cur = list->first; cur; cur = cur->next)
      if (0 == n++)
      {
         anchorTag = concat(5, "<a href='#", cur->str, "'>", cur->str, "</a>");
         strcpy(obuf, anchorTag);
      }
      else
      {
         strcat(obuf, ", ");
         anchorTag = concat(5, "<a href='#", cur->str, "'>", cur->str, "</a>");
         strcat(obuf, anchorTag);
      }

   return obuf;
}

//----------------------------------------------------------------------//
//  htmlset
//
//  Add a set to the internal list.
//----------------------------------------------------------------------//
static void htmlset(char *name)
{
   static struct htmlset *new;
   char abb[2];

   abb[0] = *name;
   abb[1] = '\0';

   new = (Htmlset *)malloc(sizeof(Htmlset));
   new->obj = MYOBJ;
   new->index = strdup(abb);
   new->istime = 0;

   if (intertemporal)
      if (isequal(name, "time") || issubset(name, "time"))
         new->istime = 1;

   if (htmlsets == 0)
      htmlsets = newarray();
   addvalue(htmlsets, name, new);
}

// https://stackoverflow.com/questions/779875/what-function-is-to-replace-a-substring-from-a-string-in-c
char *str_replace(char *orig, char *rep, char *with)
{
   char *result;  // the return string
   char *ins;     // the next insert point
   char *tmp;     // varies
   int len_rep;   // length of rep (the string to remove)
   int len_with;  // length of with (the string to replace rep with)
   int len_front; // distance between rep and end of last rep
   int count;     // number of replacements

   // sanity checks and initialization
   if (!orig || !rep)
      return NULL;
   len_rep = strlen(rep);
   if (len_rep == 0)
      return NULL; // empty rep causes infinite loop during count
   if (!with)
      with = "";
   len_with = strlen(with);

   // count the number of replacements needed
   ins = orig;
   for (count = 0; (tmp = strstr(ins, rep)); ++count)
   {
      ins = tmp + len_rep;
   }

   tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

   if (!result)
      return NULL;

   // first time through the loop, all the variable are set correctly
   // from here on,
   //    tmp points to the end of the result string
   //    ins points to the next occurrence of rep in orig
   //    orig points to the remainder of orig after "end of rep"
   while (count--)
   {
      ins = strstr(orig, rep);
      len_front = ins - orig;
      tmp = strncpy(tmp, orig, len_front) + len_front;
      tmp = strcpy(tmp, with) + len_with;
      orig += len_front + len_rep; // move to next "end of rep"
   }
   strcpy(tmp, orig);
   return result;
}

//----------------------------------------------------------------------//
//  htmlvar
//
//  Generate a variable reference from a name and a list of sets.
//----------------------------------------------------------------------//
static char *htmlvar(char *name, List *sets, int dt)
{
   Item *s;
   List *indexes;
   Htmlset *cur;
   char *result;
   char tbuf[10];

   validate(htmlsets, ARRAYOBJ, "htmlvar");
   if (name == 0)
      FAULT("null pointer passed to htmlvar");

   if (sets == 0)
      return strdup(name);
   validate(sets, LISTOBJ, "htmlvar");

   if (sets->n == 0)
      return concat(5, "\\href{#", name, "}{", name, "}");

   indexes = newsequence();
   for (s = sets->first; s; s = s->next)
   {
      if (isimplicit(s->str))
      {
         result = concat(3, "\\text{", s->str, "}");
         addlist(indexes, result);
         free(result);
         continue;
      }
      cur = (Htmlset *)getvalue(htmlsets, s->str);
      if (cur->istime && dt)
      {
         sprintf(tbuf, "%s%+d", cur->index, dt);
         addlist(indexes, tbuf);
      }
      else
         addlist(indexes, cur->index);
   }

   char *escapedName;
   escapedName = str_replace(name, "_", "\\_");
   // printf(escapedName);
   result = concat(7, "\\href{#", name, "}{", escapedName, "(", slprint(indexes), ")}");
   freelist(indexes);
   return result;
}

//----------------------------------------------------------------------//
//  htmlqualifier
//
//  Generate an html qualifier from a list of sets.
//----------------------------------------------------------------------//
static char *htmlqualifier(List *sets)
{
   Item *s;
   Htmlset *cur;
   char *result, *new;
   char *sep;

   validate(sets, LISTOBJ, "htmlqualifier");

   sep = "";
   result = strdup("");
   for (s = sets->first; s; s = s->next)
   {
      if (isimplicit(s->str))
         continue;
      cur = (Htmlset *)getvalue(htmlsets, s->str);
      new = concat(9, result, sep, "<i>", cur->index, "</i> in <b><a href='#", s->str, "'>", s->str, "</a></b>");
      free(result);
      result = new;
      sep = ", ";
   }

   return result;
}

//----------------------------------------------------------------------//
//  Begin processing the file
//----------------------------------------------------------------------//

void HTML_begin_file(char *basename)
{
   fprintf(code, "<html>\n<head>\n");
   fprintf(code, "<title>G-Cubed %s</title>\n", basename);
   fprintf(code, "<link rel='stylesheet' href='https://documentation.gcubed.com/assets/css/just-the-docs-default.css'/>");
   fprintf(code, "<script src='https://documentation.gcubed.com/assets/js/vendor/lunr.min.js'></script>");
   fprintf(code, "<script src='https://documentation.gcubed.com/assets/js/just-the-docs.js'></script>");
   fprintf(code, "<style type='text/css'>\n%s</style>\n", HTML_css);
   fprintf(code, "<script>MathJax = { jax: ['input/tex', 'output/svg'], tex: { tags: 'ams', packages: {'[+]': ['textmacros']} }, svg: { displayAlign: 'left' }, loader: {load: ['[tex]/textmacros']} };</script>");
   fprintf(code, "<script type='text/javascript' id='MathJax-script' async src='https://cdn.jsdelivr.net/npm/mathjax@3/es5/tex-svg.js'></script>\n");
   fprintf(code, "</head>\n<body>\n");
   fprintf(code, "<h1>G-Cubed %s</h1>\n", basename);
}

//----------------------------------------------------------------------//
//  End processing the file
//----------------------------------------------------------------------//

void HTML_end_file()
{
   fprintf(code, "</div>\n</body>\n</html>\n");
}

//----------------------------------------------------------------------//
//  HTML_declare
//----------------------------------------------------------------------//

void HTML_declare(void *sym)
{
   char *name;
   if (istype(sym, set))
   {
      name = symname(sym);
      htmlset(name);
      free(name);
   }
}

//----------------------------------------------------------------------//
//  HTML_writedecs
//
//  Write out the declaration part of the file.
//----------------------------------------------------------------------//

static void HTML_writedecs()
{
   Element *cur;
   Htmlset *cts;
   char buf[10], *name, *vstr, *desc, *what;
   int n;
   List *val;
   List *sofar;
   char *nbsp = "&nbsp;";

   char *lhsLinks;
   char *rhsLinks;

   List *attributes;

   //
   //  make unique set index variables
   //

   if (htmlsets)
   {
      sofar = newlist();
      for (cur = htmlsets->first; cur; cur = cur->next)
      {
         n = 1;
         cts = (Htmlset *)cur->value;
         while (lookup(cts->index) || ismember(cts->index, sofar))
         {
            sprintf(buf, "%c%d", *(cts->index), n++);
            free(cts->index);
            cts->index = strdup(buf);
         }
         addlist(sofar, cts->index);
      }
      sofar = freelist(sofar);
   }

   //
   //  write out sets
   //

   what = "Sets";
   fprintf(code, "<div class=\"heading\">%s:</div>\n", what);

   for (cur = firstsymbol(set), n = 0; cur; cur = nextsymbol(cur), n++)
   {
      name = symname(cur);
      desc = symdescrip(cur);
      val = symvalue(cur);

      if (n == 0)
      {
         fprintf(code, "<div class=\"dblock\">\n");
         fprintf(code, "<table class=\"dec\" border=1 cellspacing=0>\n");
         fprintf(code, "<tr><th>Name<th>Elements<th>Description</tr>\n");
      }

      if (strlen(desc) == 0)
         desc = nbsp;
      vstr = html_slprint(val);
      fprintf(code, "<tr><td><a id='%s'><b>%s</b></a><td>%s<td>%s</tr>\n", name, name, vstr, desc);

      xfree(vstr);
      freelist(val);
      free(name);
   }

   fprintf(code, "</table>\n</div>\n");

   //
   //  write out variable statements
   //

   what = "Variables";
   fprintf(code, "<div class=\"heading\">%s:</div>\n", what);

   for (cur = firstsymbol(var), n = 0; cur; cur = nextsymbol(cur), n++)
   {
      lhsLinks = "Not determined.";
      rhsLinks = "Not determined.";
      name = symname(cur);
      desc = symdescrip(cur);
      val = symvalue(cur);
      attributes = symattrib(cur);
      lhsLinks = lhsAsHtml(cur);
      rhsLinks = rhsAsHtml(cur);

      if (n == 0)
      {
         fprintf(code, "<div class=\"dblock\">\n");
         fprintf(code, "<table class=\"dec\" border=1 cellspacing=0>\n");
         fprintf(code, "<tr><th>Name<th>Domain<th>Description<th>Units<th>LHS<th>RHS</tr>\n");
      }

      if (strlen(desc) == 0)
         desc = nbsp;
      vstr = html_slprint_for_setlist(val);
      fprintf(code, "<tr><td><a id='%s'><b>%s</b></a><td>%s<td>%s<td>%s<td>%s<td>%s</tr>\n", name, name, vstr, desc, slprint(attributes), lhsLinks, rhsLinks);

      free(name);
      free(desc);
      freelist(val);
      freelist(attributes);
      xfree(vstr);
   }

   fprintf(code, "</table>\n</div>\n");

   //
   //  write coefficient declarations
   //

   what = "Parameters";
   fprintf(code, "<div class=\"heading\">%s:</div>\n", what);

   for (cur = firstsymbol(par), n = 0; cur; cur = nextsymbol(cur), n++)
   {
      name = symname(cur);
      desc = symdescrip(cur);
      val = symvalue(cur);

      if (n == 0)
      {
         fprintf(code, "<div class=\"dblock\">\n");
         fprintf(code, "<table class=\"dec\" border=1 cellspacing=0>\n");
         fprintf(code, "<tr><th>Name<th>Domain<th>Description</tr>\n");
      }

      if (strlen(desc) == 0)
         desc = nbsp;
      vstr = html_slprint_for_setlist(val);
      fprintf(code, "<tr><td><a id='%s'><b>%s</b></a><td><b>%s</b><td>%s</tr></a>\n", name, name, vstr, desc);

      free(name);
      free(desc);
      freelist(val);
      xfree(vstr);
   }

   fprintf(code, "</table>\n</div>\n");

   fprintf(code, "<div class=\"heading\">Equations:</div>\n");
   fprintf(code, "<div class=\"dblock\">\n");
}

//----------------------------------------------------------------------//
// Written by Geoff: GCS
//----------------------------------------------------------------------//
char *getLhsVariableName(Node *node)
{
   if (isname(node))
   {
      return node->str;
   }
   Node *left = node->l;
   if (left)
   {
      char *leftVariableName = getLhsVariableName(left);
      if (strlen(leftVariableName) > 0)
      {
         return leftVariableName;
      }
   }
   Node *right = node->r;
   if (right)
   {
      char *rightVariableName = getLhsVariableName(right);
      if (strlen(rightVariableName) > 0)
      {
         return rightVariableName;
      }
   }

   return "Not a variable";
}


//----------------------------------------------------------------------//
//  HTML_begin_block
//----------------------------------------------------------------------//

void HTML_begin_block(void *eq)
{
   char *qual;
   List *eqnsets();
   int nblk, nstart, nscalar, nend, equationNumber;
   char *label, *eqnLhsVariableName;
   void *symbol;

   if (HTML_block == 1)
      HTML_writedecs();

   nblk = HTML_block++;
   nstart = HTML_scalar;
   nscalar = eqncount(eq);
   nend = nstart + nscalar - 1;

   equationNumber = eqnumber(eq);

   HTML_scalar = nend + 1;

   label = eqnlabel(eq);
   eqnLhsVariableName = getLhsVariableName(((Node *)getlhs(eq)));

   if (strlen(eqnLhsVariableName) > 0)
   {
      symbol = lookup(eqnLhsVariableName);
   }

   fprintf(code, "<a id='%d'/>", nblk);

   if (label)
      fprintf(code, "Equation %d: <a href='#%s'>%s</a>: %s<br>\n", equationNumber, eqnLhsVariableName, eqnLhsVariableName, label);
   else
      fprintf(code, "Equation %d: <a href='#%s'>%s</a><br>\n", equationNumber, eqnLhsVariableName, eqnLhsVariableName);

   switch (nscalar)
   {
   case 0:
      fprintf(code, "Contains undeclared symbols<br>\n");
      break;

   case 1:
      break;

   default:
      qual = htmlqualifier(eqnsets(eq));
      fprintf(code, "For %s (%d total):<br>\n", qual, nscalar);
      break;
   }

   fprintf(code, "<div class=\"eblock\">\n<div class=\"eqn\"> \\[ ");
}

//----------------------------------------------------------------------//
//  HTML_end_eqn
//----------------------------------------------------------------------//

void HTML_end_eqn(void *eq)
{
   fprintf(code, " \\]\n</div>\n</div>\n");
}

//----------------------------------------------------------------------//
//  HTML_begin_func
//----------------------------------------------------------------------//

char *HTML_begin_func(char *func, char *arg)
{
   Htmlset *cur;

   if (isequal(func, "sum") || isequal(func, "prod"))
   {
      cur = (Htmlset *)getvalue(htmlsets, arg);
      if (cur == 0)
         FAULT("null pointer in HTML_begin_func");
      char *escapedArg = str_replace(arg, "_", "\\_");
      return concat(9, "\\", func, "_{", cur->index, " \\; \\text{in} \\; \\href{#", arg, "}{", escapedArg, "}} { \\left(");
   }

   if (arg)
      FAULT("unexpected function call in HTML_begin_func");

   if (isequal(func, "log"))
      return strdup("ln{ \\left(");

   return concat(2, func, "{ \\left(");
}

//----------------------------------------------------------------------//
//  HTML_end_func
//----------------------------------------------------------------------//

char *HTML_end_func()
{

   return strdup("\\right) }");
}

//----------------------------------------------------------------------//
//  HTML_show_symbol
//----------------------------------------------------------------------//

char *HTML_show_symbol(char *str, List *setlist, Context context)
{
   char *curstr, *tmpstr;
   int delta;

   curstr = htmlvar(str, setlist, context.dt);
   delta = context.dt;

   while (delta != 0)
   {
      if (delta < 0)
      {
         tmpstr = concat(3, "lag({", curstr, "})");
         delta++;
      }
      else
      {
         tmpstr = concat(3, "lead({", curstr, "})");
         delta--;
      }
      free(curstr);
      curstr = tmpstr;
   }

   return curstr;
}

/*-------------------------------------------------------------------*
 *  HTML_spprint
 *
 *  Modifies the default spprint implementation to ensure
 *-------------------------------------------------------------------*/
char *HTML_spprint(Nodetype prevtype, Node *cur, char *indent)
{

   int parens, wrap_right;
   char *comma, *cr;
   char *lstr, *rstr, *chunk;

   if (cur == 0)
      return strdup("");

   parens = 0;
   comma = "";
   switch (prevtype)
   {
   case nul:
   case add:
   case sub:
      if (now(neg))
         parens = 1;
      break;

   case mul:
      if (now(add) || now(sub))
         parens = 1;
      if (now(dvd) || now(neg))
         parens = 1;
      break;

   case neg:
      parens = 1;
      if (now(nam) || now(num) || now(mul))
         parens = 0;
      if (now(log) || now(exp) || now(pow))
         parens = 0;
      if (now(sum) || now(prd))
         parens = 0;
      break;

   case dvd:
      parens = 1;
      if (now(nam) || now(num) || now(pow))
         parens = 0;
      if (now(sum) || now(prd))
         parens = 0;
      if (now(log) || now(exp))
         parens = 0;
      break;

   case pow:
      parens = 1;
      if (now(nam) || now(num) || now(log) || now(exp))
         parens = 0;
      if (now(sum) || now(prd))
         parens = 0;
      break;

   case log:
   case exp:
   case lag:
   case led:
      parens = 1;
      break;

   case equ:
   case sum:
   case prd:
   case dom:
      break;

   case nam:
   case num:
      if (now(nam) || now(num))
         comma = ",";
      break;

   default:
      fatal_error("%s", "invalid state reached in spprint");
   }

   switch (cur->type)
   {
   case sum:
   case prd:
      lstr = codegen_spprint(cur->type, cur->l, indent);
      rstr = codegen_spprint(cur->type, cur->r, indent);
      chunk = concat(6, cur->str, "(", lstr, ",", rstr, ")");
      free(lstr);
      free(rstr);
      break;

   case lst:
      lstr = strdup("{("); // GCS Added { for HTML LaTeX output
      for (cur = cur->r; cur; cur = cur->r)
      {
         if (cur->r)
            rstr = concat(3, lstr, cur->str, ",");
         else
            rstr = concat(2, lstr, cur->str);
         free(lstr);
         lstr = rstr;
      }
      chunk = concat(2, lstr, ")}"); // GCS Added } for HTML LaTeX output
      free(lstr);
      break;

   default:
      lstr = codegen_spprint(cur->type, cur->l, indent);
      rstr = codegen_spprint(cur->type, cur->r, indent);

      cr = "";
      if (indent)
         if (strlen(lstr) + strlen(rstr) > 70 || strlen(lstr) > 40 || strlen(rstr) > 40)
            cr = concat(2, "\n", indent);

      wrap_right = 0;
      if (cur->type == sub)
         if (cur->r->type == add || cur->r->type == sub)
            wrap_right = 1;

      if (parens)
         // GCS Added {  to opening brackets for HTML LaTeX output
         if (wrap_right)
            chunk = concat(8, "{(", lstr, comma, cr, cur->str, "(", rstr, "))");
         else
            chunk = concat(7, "{(", lstr, comma, cr, cur->str, rstr, ")");
      else if (wrap_right)
         chunk = concat(7, lstr, comma, cr, cur->str, "(", rstr, ")");
      else
         chunk = concat(5, lstr, comma, cr, cur->str, rstr);

      if (strlen(cr))
         free(cr);
      free(lstr);
      free(rstr);
   }

   return chunk;
}

/*--------------------------------------------------------------------*
 *  write_file
 *
 *  
 *--------------------------------------------------------------------*/
void HTML_write_file(char *basename)
{
   void *sym;
   void *eq;
   List *eqnsets(), *eqsets;
   int eqncount();

   if (DBG)
      printf("write_file\n");

   //
   //  Allow the language module to set options and write
   //  any introductory information to the output file.
   //

   codegen_begin_file(basename);
   if (DBG)
      xcheck("after begin_file");

   //
   //  Some options do not have defaults and MUST be set
   //  by the language module.
   //

   if (is_eqn_set() == 0)
      FAULT("Equation style has not been set");
   if (is_sum_set() == 0)
      FAULT("Summation style has not been set");

   if (DBG)
   {
      printf("   eqn style: ");
      printf("scalar=%d ", is_eqn_scalar());
      printf("vector=%d\n", is_eqn_vector());
      printf("   sum style: ");
      printf("scalar=%d ", is_sum_scalar());
      printf("vector=%d\n", is_sum_vector());
   }

   //
   //  Tell the language module about the symbols.  Pass it
   //  all the sets first, then the parameters, and then the
   //  variables.
   //

   for (sym = firstsymbol(set); sym; sym = nextsymbol(sym))
      codegen_declare(sym);

   for (sym = firstsymbol(par); sym; sym = nextsymbol(sym))
      codegen_declare(sym);

   for (sym = firstsymbol(var); sym; sym = nextsymbol(sym))
      codegen_declare(sym);

   if (DBG)
      xcheck("after declares");

   //
   //  Now generate the equations.  The language module is
   //  allowed to write a preamble to each equation block, and
   //  to write a prefix and suffix to each equation, but the
   //  the main equation-writing is done by by show_eq.
   //

   for (eq = firsteqn(); eq; eq = nexteqn(eq))
   {
      List *sublist;

      // GCS These next two lines are commented out for HTML output.
      // if (hasundec(eq) || !istimeok(eq))
         // continue;

      eqsets = eqnsets(eq);

      codegen_begin_block(eq);

      if (is_eqn_vector())
      {
         sublist = newsequence();
         codegen_show_eq(eq, eqsets, sublist);
         freelist(sublist);
      }
      else
      {
         int neqns;
         neqns = eqncount(eq);
         cart_build(eqsets);
         while ((sublist = cart_next()))
         {
            codegen_show_eq(eq, eqsets, sublist);
            neqns--;
         }
         if (neqns)
            FAULT("Incorrect number of equations written. Using # with a time set?");
      }

      eqsets = freelist(eqsets);
   }

   if (DBG)
      xcheck("after equations");

   //
   //  All done; allow the language module to write a postscript
   //

   codegen_end_file();
   if (DBG)
      xcheck("after end_file");

   fclose(code);
   fclose(info);
   remove("rubbish.lis");
}

/*--------------------------------------------------------------------*
 *  show_eq
 *
 *  Generate and print a scalar equation by recursively descending
 *  through the node tree.
 *--------------------------------------------------------------------*/
void HTML_show_eq(void *eq, List *setlist, List *sublist)
{
   Node *getlhs(), *getrhs();
   char *lstr, *rstr, *all;
   char *head, *tail;

   lstr = codegen_show_node(nul, getlhs(eq), setlist, sublist);
   rstr = codegen_show_node(nul, getrhs(eq), setlist, sublist);

   codegen_begin_eqn(eq);

   if (is_eqn_normalized())
      all = concat(4, lstr, " - \\left(", rstr, "\\right)");
   else
      all = concat(3, lstr, " = ", rstr);

   free(lstr);
   free(rstr);

   if (get_line_length() == 0)
   {
      fprintf(code, "%s", all);
      free(all);
      codegen_end_eqn(eq);
      return;
   }

   if (strlen(all) <= get_line_length())
      fprintf(code, "%s", all);
   else
   {
      for (head = all; (tail = strchr(head, '\n')); head = tail)
      {
         *tail++ = '\0';
         codegen_wrap_write(head, 1, 0);
      }
      codegen_wrap_write(head, 0, 0);
   }

   free(all);
   codegen_end_eqn(eq);
}

/*--------------------------------------------------------------------*
 *  show_node
 *
 *  Generate the node recursively.  This allocates and frees a
 *  lot of small chunks of memory as it constructs nodes from the
 *  bottom up.  Avoids having any fixed buffer sizes but is a
 *  bit tedious as a result.
 *--------------------------------------------------------------------*/
char *HTML_show_node(Nodetype prevtype, Node *cur, List *setlist, List *sublist)
{
   int parens, wrap_right;
   char *buf, *newbuf;
   List *augsets, *augsubs, *sumover;
   char *beginfunc, *endfunc;
   char *lstr, *rstr, *cr;
   char *lpar, *rpar;
   char *op, *thisop;
   int isfunc;
   char *side;
   Context mycontext;

   if (cur == 0)
      return strdup("");

   mycontext.lhs = cur->lhs;
   mycontext.dt = cur->dt;
   mycontext.tsub = 0;

   side = mycontext.lhs ? "lhs" : "rhs";
   if (DBG)
      printf("show_node (%s)\n", side);

   validate(cur, NODEOBJ, "show_node");
   validate(setlist, LISTOBJ, "show_block for setlist");
   validate(sublist, LISTOBJ, "show_block for sublist");

#define now(arg) (cur->type == arg)

   //
   //  decide whether the current node should be wrapped with
   //  parentheses.  in principle this shouldn't be necessary
   //  if all software correctly obeyed precedence rules but
   //  we'll do it just out of paranoia (and experience with
   //  gauss).
   //
   //  the choice is determined by the type of the node one
   //  step higher in the parse tree.
   //

   parens = 0;
   switch (prevtype)
   {
   case nul:
   case add:
   case sub:
      if (now(neg))
         parens = 1;
      break;

   case mul:
      if (now(add) || now(sub))
         parens = 1;
      if (now(dvd) || now(neg))
         parens = 1;
      break;

   case neg:
      parens = 1;
      if (now(nam) || now(num) || now(mul))
         parens = 0;
      if (now(log) || now(exp) || now(pow))
         parens = 0;
      if (now(lag) || now(led))
         parens = 0;
      if (now(sum) || now(prd))
         parens = 0;
      break;

   case dvd:
      parens = 1;
      if (now(nam) || now(num) || now(pow))
         parens = 0;
      if (now(sum) || now(prd))
         parens = 0;
      if (now(lag) || now(led))
         parens = 0;
      if (now(log) || now(exp))
         parens = 0;
      break;

   case pow:
      parens = 1;
      if (now(nam) || now(num) || now(log) || now(exp))
         parens = 0;
      if (now(sum) || now(prd))
         parens = 0;
      if (now(lag) || now(led))
         parens = 0;
      break;

   case log:
   case exp:
   case lag:
   case led:
   case sum:
   case prd:
   case nam:
   case num:
   case equ:
   case dom:
      break;

   default:
      FAULT("Invalid state reached in show_node");
   }

   //
   //  now construct the current node
   //

   //
   //  case 1: a few straightforward items
   //

   switch (cur->type)
   {
   case nam:
      return show_symbol(cur->str, cur->domain, setlist, sublist, mycontext);

   case lag:
   case led:
      return codegen_show_node(cur->type, cur->r, setlist, sublist);

   case dom:
      return codegen_show_node(cur->type, cur->l, setlist, sublist);

   case lst:
      FAULT("Unexpected lst state in show_node");

   default:
      break;
   }

   //
   //  case 2: sum and product, scalar form
   //

   if (cur->type == sum || cur->type == prd)
      if (is_sum_scalar())
      {
         Item *ele;

         if (DBG)
            printf("scalar sum or product: %s\n", snprint(cur));

         lstr = (cur->l)->str;

         augsets = newsequence();
         catlist(augsets, setlist);
         addlist(augsets, lstr);

         op = (cur->type == prd) ? "*" : "+";
         lpar = (cur->type == prd) ? "{\\left(" : "";
         rpar = (cur->type == prd) ? "\\right)}" : "";

         buf = strdup("{\\left(");
         thisop = " ";

         sumover = setelements(lstr);
         for (ele = sumover->first; ele; ele = ele->next)
         {
            augsubs = newsequence();
            catlist(augsubs, sublist);
            addlist(augsubs, ele->str);

            if (DBG)
            {
               printf("calling show_node for %s\n", ele->str);
               printf("augsets = %s\n", slprint(augsets));
            }

            rstr = codegen_show_node(cur->type, cur->r, augsets, augsubs);
            newbuf = concat(6, buf, "\n      ", thisop, lpar, rstr, rpar);

            thisop = op;

            free(buf);
            free(rstr);
            freelist(augsubs);

            buf = newbuf;
         }

         newbuf = concat(2, buf, "\\right)}");
         free(buf);

         freelist(augsets);
         freelist(sumover);

         return newbuf;
      }

   //
   //  case 3: sum or product in vector form
   //

   if (cur->type == sum || cur->type == prd)
   {
      if (DBG)
         printf("vector sum or product: %s\n", snprint(cur));

      lstr = (cur->l)->str;

      augsets = newsequence();
      catlist(augsets, setlist);
      addlist(augsets, lstr);

      augsubs = newsequence();
      catlist(augsubs, sublist);
      addlist(augsubs, " \\times ");

      beginfunc = codegen_begin_func(cur->str, lstr);
      rstr = codegen_show_node(cur->type, cur->r, augsets, augsubs);
      endfunc = codegen_end_func();

      buf = concat(3, beginfunc, rstr, endfunc);

      free(beginfunc);
      free(rstr);
      free(endfunc);

      freelist(augsubs);
      freelist(augsets);

      return buf;
   }

   //
   //  case 3A: Handling division
   //

   if (cur->type == dvd)
   {

      isfunc = 0;
      lstr = codegen_show_node(cur->type, cur->l, setlist, sublist);
      rstr = codegen_show_node(cur->type, cur->r, setlist, sublist);
      buf = concat(5, "\\frac{", lstr, "}{", rstr, "}");

      return buf;
   }

   //
   //  case 4: everything else
   //

   switch (cur->type)
   {
   case log:
   case exp:
      isfunc = 1;
      lstr = codegen_begin_func(cur->str, 0);
      endfunc = codegen_end_func();
      op = "";
      break;

   case pow:
      isfunc = 0;
      lstr = codegen_show_node(cur->type, cur->l, setlist, sublist);
      endfunc = strdup("");
      op = "^";
      break;

   default:
      isfunc = 0;
      lstr = codegen_show_node(cur->type, cur->l, setlist, sublist);
      endfunc = strdup("");
      op = cur->str;
   }

   rstr = codegen_show_node(cur->type, cur->r, setlist, sublist);

   cr = "";
   if (strlen(lstr) + strlen(rstr) > 70 || strlen(lstr) > 40 || strlen(rstr) > 40)
      cr = " \n        ";

   lpar = (parens && isfunc == 0) ? "{(" : "";
   rpar = (parens && isfunc == 0) ? ")}" : "";

   wrap_right = 0;
   if (cur->type == sub)
      if (cur->r->type == add || cur->r->type == sub)
         wrap_right = 1;

   if (wrap_right)
      buf = concat(9, lpar, lstr, cr, op, "(", rstr, ")", rpar, endfunc);
   else
      buf = concat(7, lpar, lstr, cr, op, rstr, rpar, endfunc);

   free(lstr);
   free(rstr);
   free(endfunc);

   return buf;
}

//----------------------------------------------------------------------//
//  Connect up the public routines.
//----------------------------------------------------------------------//

void Html_setup(void)
{
   lang_begin_file(HTML_begin_file);
   lang_end_file(HTML_end_file);
   lang_declare(HTML_declare);
   lang_begin_block(HTML_begin_block);
   lang_end_eqn(HTML_end_eqn);
   lang_begin_func(HTML_begin_func);
   lang_end_func(HTML_end_func);
   lang_show_symbol(HTML_show_symbol);
   lang_spprint(HTML_spprint);
   lang_write_file(HTML_write_file);
   lang_show_eq(HTML_show_eq);
   lang_show_node(HTML_show_node);

   set_eqn_vector();
   set_sum_vector();
}

char *Html_version = "$Revision: 58 $";
