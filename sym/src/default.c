/*--------------------------------------------------------------------*
 *  default.c
 *  Dec 04 (PJW)
 *
 *  Default_setup() will be called automatically just before the
 *  actual language is initialized.  The actual language only 
 *  needs to override methods that need to be changed from their
 *  default behavior.
 *--------------------------------------------------------------------*/

#include "error.h"
#include "lang.h"
#include "options.h"
#include "output.h"
#include "str.h"
#include "sym.h"
#include "symtable.h"
#include "codegen.h"
#include "cart.h"
#include "nodes.h"
#include "sets.h"
#include "eqns.h"
#include "xmalloc.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define myDEBUG 1

#define now(arg) (cur->type == arg)

void Default_begin_file(char *basename)
{
}

void Default_end_file()
{
}

void Default_declare(void *sym)
{
}

void Default_begin_block(void *eq)
{
}

void Default_begin_eqn(void *eq)
{
}

void Default_end_eqn(void *eq)
{
   fprintf(code," ;\n\n");
}

char *Default_begin_func(char *func, char *arg)
{
   if( arg )return concat(4,func,"(",arg,",");
   return concat(2,func,"("); 
}

char *Default_end_func()
{
   return strdup(")");
}

/*-------------------------------------------------------------------*
 *  Default spprint
 *
 *  Added by Geoff Shuetrim in December 2022 to facilitate more
 *  fine-grain control over how nodes are printed in equations.
 * 
 *  This default implementation is lifted from the original 
 *  spprint.c file.
 *-------------------------------------------------------------------*/
char *Default_spprint(Nodetype prevtype, Node *cur, char *indent)
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
      lstr = strdup("(");
      for (cur = cur->r; cur; cur = cur->r)
      {
         if (cur->r)
            rstr = concat(3, lstr, cur->str, ",");
         else
            rstr = concat(2, lstr, cur->str);
         free(lstr);
         lstr = rstr;
      }
      chunk = concat(2, lstr, ")");
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
         if (wrap_right)
            chunk = concat(8, "(", lstr, comma, cr, cur->str, "(", rstr, "))");
         else
            chunk = concat(7, "(", lstr, comma, cr, cur->str, rstr, ")");
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
 *  wrap_write
 *
 *  write a line to the code file but wrap it to keep the line from
 *  being too long.
 *--------------------------------------------------------------------*/
void Default_wrap_write(char *line, int addcr, int commaok)
{
   char *end, op, *dup, *rest;
   int do_wrap;

   dup = strdup(line);
   rest = dup;

   while (rest)
   {

      if (strlen(rest) <= get_line_length())
      {
         fprintf(code, "%s", rest);
         if (addcr)
            fprintf(code, "\n");
         return;
      }

      if (strcspn(rest, "\n") <= get_line_length())
      {
         end = strchr(rest, '\n');
         *end++ = '\0';
         fprintf(code, "%s\n", rest);
         rest = end;
         continue;
      }

      for (end = rest + get_line_length(); end > rest; end--)
      {
         do_wrap = isspace(*end);
         do_wrap |= (*end == '+' || *end == '-' || *end == '*' || *end == '/' || *end == '=' || *end == '^');
         do_wrap |= (commaok && *end == ',');
         if (do_wrap)
         {
            op = *end;
            *end = '\0';
            fprintf(code, "%s\n   ", rest);
            *end = op;
            break;
         }
      }

      if (rest == end)
         fatal_error("Could not wrap long line:\n%s\n", line);

      rest = end;
   }

   free(dup);
}

/*--------------------------------------------------------------------*
 *  write_file
 *
 *  Write out a file in the selected language.
 *--------------------------------------------------------------------*/
void Default_write_file(char *basename)
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

      if (hasundec(eq) || !istimeok(eq))
         continue;

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
}

/*--------------------------------------------------------------------*
*  show_eq
*
*  Generate and print a scalar equation by recursively descending
*  through the node tree.
*--------------------------------------------------------------------*/
void Default_show_eq(void *eq, List *setlist, List *sublist)
{
   Node *getlhs(), *getrhs();
   char *lstr, *rstr, *all;
   char *head, *tail;

   lstr = codegen_show_node(nul, getlhs(eq), setlist, sublist);
   rstr = codegen_show_node(nul, getrhs(eq), setlist, sublist);

   codegen_begin_eqn(eq);

   if (is_eqn_normalized())
      all = concat(4, lstr, " - (", rstr, ")");
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
char *Default_show_node(Nodetype prevtype, Node *cur, List *setlist, List *sublist)
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
         lpar = (cur->type == prd) ? "(" : "";
         rpar = (cur->type == prd) ? ")" : "";

         buf = strdup("(");
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

         newbuf = concat(2, buf, ")");
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
      addlist(augsubs, "*");

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

   lpar = (parens && isfunc == 0) ? "(" : "";
   rpar = (parens && isfunc == 0) ? ")" : "";

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

//
//  Connect up the public routines.
//

void Default_setup(void)
{
   lang_begin_file(Default_begin_file);
   lang_end_file(Default_end_file);
   lang_declare(Default_declare);
   lang_begin_block(Default_begin_block);
   lang_begin_eqn(Default_begin_eqn);
   lang_end_eqn(Default_end_eqn);
   lang_begin_func(Default_begin_func);
   lang_end_func(Default_end_func);
   lang_show_symbol(0);
   lang_show_node(Default_show_node);
   lang_show_eq(Default_show_eq);
   lang_write_file(Default_wrap_write);
   lang_write_file(Default_write_file);
   lang_spprint(Default_spprint);
}

