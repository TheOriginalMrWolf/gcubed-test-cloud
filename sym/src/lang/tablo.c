/*--------------------------------------------------------------------*
 *  tablo.c
 *  Dec 04 (PJW)
 *--------------------------------------------------------------------*
.. ### tablo
..
.. Support GEMPACK's TABLO language.
..
.. + All variables are implicitly subscripted by time. However,
..   parameters are not.
..
.. + The left side of an equation may be an expression, not just a
..   variable name.
..
.. + At most one attribute is allowed in variable and parameter
..   declarations. If present, it is used as the name of an HAR header
..   and should have the form: c###.
..
.. + Parameters are read from TABLO logical name 'param'. Variables
..   are read from one of the following files depending on the value
..   of the first letter of the header: B = 'base', K = 'kalman',
..   M = 'make', N = 'endog', and X = 'exog'. If the first letter does
..   not match one of the above 'base' will be used as the file name.
 *--------------------------------------------------------------------*/

#include "../assoc.h"
#include "../eqns.h"
#include "../error.h"
#include "../lang.h"
#include "../options.h"
#include "../output.h"
#include "../sets.h"
#include "../str.h"
#include "../sym.h"
#include "../symtable.h"
#include "../codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define  myDEBUG 1

char *getsupset(char*);

//
//  Flag for checking local Tablo objects for corruption
//

#define MYOBJ 3001


//
//  Internal variables
//

struct tabset {
   int obj;
   char *index;
   int  istime;
   };
typedef struct tabset Tabset;

Array *tabsets=0;
List *calcvars=0;

static int Tablo_eqn=0;
static int Tablo_var=0;
static int Tablo_par=0;
static int Tablo_scalar_eqn=0;

enum har_type {
   h_int, h_kal, h_mak, h_end,
   h_iot, h_par, h_ext, h_exo,
   h_aen, h_aex, h_apa, h_unk };


//----------------------------------------------------------------------//
//  tablo_error()
//
//  Print an error message and crash.  Use this routine instead of
//  fatal_error in order to indicate that the problem was with the
//  Tablo back end.
//----------------------------------------------------------------------//
static void tablo_error(char *fmt, char *str)
{
   show_error("Fatal Error Writing Tablo File",fmt,str);
}


//----------------------------------------------------------------------//
//  tabloset
//
//  Add a set to the internal list.
//----------------------------------------------------------------------//
static void tabloset(char *name)
{
   static struct tabset *new;
   char abb[2];

   abb[0] = *name;
   abb[1] = '\0';

   new = (Tabset *) malloc( sizeof(Tabset) );
   new->obj    = MYOBJ;
   new->index  = strdup(abb);
   new->istime = 0;

   if( isequal(name,"time") || issubset(name,"time") )
      new->istime = 1;

   if( tabsets==0 )tabsets = newarray();
   addvalue(tabsets,name,new);
}


//----------------------------------------------------------------------//
//  tablovar
//
//  Generate a variable reference from a name and a list of sets.
//----------------------------------------------------------------------//
static char *tablovar(char *name, List *sets, int dt)
{
   Item *s;
   List *indexes;
   Tabset *cur;
   char *result;
   char tbuf[10];

   validate( tabsets, ARRAYOBJ, "tablovar" );
   if( name==0 )FAULT("null pointer passed to tablovar");

   if( sets==0 )return strdup(name);
   validate( sets, LISTOBJ, "tablovar" );

   if( sets->n == 0 )return strdup(name);

   indexes = newsequence();
   for( s=sets->first ; s ; s=s->next )
      {
      if( isimplicit(s->str) )
         {
         result = concat(3,"\"",s->str,"\"");
         addlist(indexes,result);
         free(result);
         continue;
         }
      cur = (Tabset *) getvalue(tabsets,s->str);
      if( cur->istime && dt )
         {
         sprintf(tbuf,"%s%+d",cur->index,dt);
         addlist(indexes,tbuf);
         }
      else
         addlist(indexes,cur->index);
      }

   result = concat(4,name,"(",slprint(indexes),")");
   freelist(indexes);
   return result;
}


//----------------------------------------------------------------------//
//  tabloqualifier
//
//  Generate a tablo qualifier from a list of sets.
//----------------------------------------------------------------------//
static char *tabloqualifier(List *sets)
{
   Item *s;
   Tabset *cur;
   char *result,*new;

   validate( sets, LISTOBJ, "tabloqualifier" );

   result = strdup("");
   for( s=sets->first ; s ; s=s->next )
      {
      if( isimplicit(s->str) )continue;
      cur = (Tabset *) getvalue(tabsets,s->str);
      new = concat(6,result,"(all,",cur->index,",",s->str,") ");
      free(result);
      result = new;
      }

   return result;
}


//----------------------------------------------------------------------//
//  tablo_type()
//
//  Return an enum indicating what type of header was associated with
//  a given variable or parameter.
//----------------------------------------------------------------------//
enum har_type tablo_type(void *symbol)
{
   List *atts;
   enum har_type this;

   this = h_unk ;
   atts = symattrib(symbol);
   if( atts->n >0 )
      switch( *atts->first->str ) {
          case 'A': this = h_aen ; break ;
          case 'B': this = h_aex ; break ;
          case 'C': this = h_apa ; break ;
          case 'I': this = h_int ; break ;
          case 'K': this = h_kal ; break ;
          case 'M': this = h_mak ; break ;
          case 'N': this = h_end ; break ;
          case 'O': this = h_iot ; break ;
          case 'P': this = h_par ; break ;
          case 'T': this = h_ext ; break ;
          case 'X': this = h_exo ; break ;
      }

   free(atts);
   return this;
}


//----------------------------------------------------------------------//
//  tablo_show_symbol()
//
//  Return a value indicating whether the current symbol should be
//  shown in the output file.
//----------------------------------------------------------------------//
static int tablo_show_symbol(void *symbol)
{
   validate( symbol, SYMBOBJ, "tablo_show_symbol" );
   return do_calc==0 || isused(symbol);
}


//----------------------------------------------------------------------//
//  tablo_need_read()
//
//  Return a value indicating whether a read statement is needed
//  for a given symbol.
//----------------------------------------------------------------------//
static int tablo_need_read(void *symbol)
{
   validate( symbol, SYMBOBJ, "tablo_need_read" );
   if( do_calc )
      return isrhs(symbol) && symattrib(symbol)->n > 0 ;
   if( symattrib(symbol)->n < 1 )
      tablo_error("Header required for symbol: %s",symname(symbol));
   return 1 ;
}

//----------------------------------------------------------------------//
//  tablo_set_used()
//
//  Mark a set and all of its supersets as used.
//----------------------------------------------------------------------//
static void tablo_set_used(char *name)
{
   List *sups;
   Item *supset;
   void setused();

   setused(lookup(name),0,0);

   sups = find_immediate_sups(name);
   if( sups->n == 0)return;

   for( supset=sups->first ; supset ; supset=supset->next )
      tablo_set_used(supset->str);
 }


//----------------------------------------------------------------------//
//  tablo_mark_used_sets()
//
//  Mark all sets actually used by parameters or variables.
//----------------------------------------------------------------------//
static void tablo_mark_used_sets(Symboltype kind)
{
   Element *cur;
   Item *curset;

   for( cur=firstsymbol(kind) ; cur ; cur=nextsymbol(cur) )
      if( tablo_show_symbol(cur) )
         for( curset=symvalue(cur)->first ; curset ; curset=curset->next )
            tablo_set_used(curset->str) ;
}


//----------------------------------------------------------------------//
//  tablo_filename()
//
//  Return a string with the logical Tablo file name to be used
//  for reading and writing a given symbol. If no header has been
//  specified, or if the initial letter of the header is not in
//  the usual list, return "other".
//----------------------------------------------------------------------//
char *tablo_filename(enum har_type har)
{
   char *filename;

   filename = "other";

   switch( har ) {
      case h_aen: filename = "impl"    ; break ;
      case h_aex: filename = "impl"    ; break ;
      case h_apa: filename = "addpar"  ; break ;
      case h_int: filename = "inter"   ; break ;
      case h_kal: filename = "kalman"  ; break ;
      case h_mak: filename = "make"    ; break ;
      case h_end: filename = "endog"   ; break ;
      case h_iot: filename = "iotable" ; break ;
      case h_par: filename = "param"   ; break ;
      case h_ext: filename = "extra"   ; break ;
      case h_exo: filename = "exog"    ; break ;
      default: break;
      }

   return filename;
}


//----------------------------------------------------------------------//
//  Tablo_declare
//----------------------------------------------------------------------//
void Tablo_declare(void *sym)
{
   char *name;
   if( istype(sym,set) )
      {
      name = symname(sym);
      tabloset(name);
      free(name);
      }
   if( istype(sym,var) )Tablo_var++;
   if( istype(sym,par) )Tablo_par++;
}


//----------------------------------------------------------------------//
//  Tablo_writedecs
//
//  Write out the declaration part of the file.
//----------------------------------------------------------------------//

static void Tablo_writedecs()
{
   Element *cur;
   Tabset *cts;
   char buf[10],*name,*qual,*ref,*iqual,*stmt;
   int  n;
   List *val;
   List *sofar;
   int  hdr=0;
   List *atts;
   char *filename;
   List *files;
   Item *itm;

   files = newlist();

   //
   //  make unique set index variables
   //

   if( tabsets )
      {
      sofar = newlist();
      for( cur=tabsets->first ; cur ; cur=cur->next )
         {
         n=1;
         cts = (Tabset *) cur->value;
         while( lookup(cts->index) || ismember(cts->index,sofar) )
            {
            sprintf(buf,"%c%d",*(cts->index),n++);
            free(cts->index);
            cts->index = strdup(buf);
            }
         addlist( sofar, cts->index );
         }
      sofar = freelist(sofar);
      }

   //
   //  write out sets
   //

   tablo_mark_used_sets(par);
   tablo_mark_used_sets(var);

   for( cur=firstsymbol(set), n=0 ; cur ; cur=nextsymbol(cur), n++ )
      if( tablo_show_symbol(cur) )
         {
         name = symname(cur);
         val  = symvalue(cur);
         cts = (Tabset *) getvalue(tabsets,name);

         iqual = cts->istime ? "(intertemporal) " : "" ;
         stmt  = concat(6,"set ",iqual,name," (",slprint(val),") ;");
         codegen_wrap_write(stmt,1,1);
         free(stmt);

         freelist(val);
         free(name);
         }

   if( n )fprintf(code,"\n");

   //
   //  write out subset statements
   //

   for( cur=firstsymbol(set), n=0 ; cur ; cur=nextsymbol(cur) )
      if( tablo_show_symbol(cur) )
         {
         List *sups;
         Item *c;
         name = symname(cur);
         sups = find_immediate_sups( name );
         if( sups->n )
            for( c=sups->first ; c ; c=c->next )
               {
               fprintf(code,"subset %s is subset of %s ;\n",name,c->str);
               n++;
               }
         free(name);
         }

   if( n )fprintf(code,"\n");

   //
   //  write coefficient declarations
   //

   for( cur=firstsymbol(par), n=0 ; cur ; cur=nextsymbol(cur), n++ )
      if( tablo_show_symbol(cur) )
         {
         name = symname(cur);
         val  = symvalue(cur);
         qual = tabloqualifier(val);
         ref  = tablovar(name,val,0);

         stmt = concat(4,"coefficient ",qual,ref," ;");
         codegen_wrap_write(stmt, 1, 0);
         free(stmt);

         freelist(val);
         free(name);
         free(qual);
         free(ref);
         }

   if( n )fprintf(code,"\n");

   //
   //  write out read statements for parameters
   //

   if( n )
      if( firstsymbol(par) )
         fprintf(code,"file param ;\n\n");

   for( cur=firstsymbol(par), n=0 ; cur ; cur=nextsymbol(cur), n++ )
      if( tablo_show_symbol(cur) )
         {
         name = symname(cur);
         val  = symvalue(cur);
         qual = tabloqualifier(val);
         ref  = tablovar(name,val,0);

         atts = symattrib(cur);
         if( atts->n == 1 )
            strncpy(buf,atts->first->str,10);
         else
            sprintf(buf,"H%3.3d",hdr++);

         stmt = concat(7,"read ",qual,"\n   ",ref," from file param header \"",buf,"\" ;");
         codegen_wrap_write(stmt, 1, 0);
         free(stmt);

         free(name);
         freelist(val);
         free(qual);
         free(ref);
         }

   if( n )fprintf(code,"\n");

   //
   //  write out variable statements; keep track of which files
   //  we'll need along the way
   //

   for( cur=firstsymbol(var) ; cur ; cur=nextsymbol(cur) )
      if( tablo_show_symbol(cur) )
         {
         name = symname(cur);
         val  = symvalue(cur);
         qual = tabloqualifier(val);
         ref  = tablovar(name,val,0);

         if( do_calc == 0 )
            stmt = concat(4,"variable ",qual,ref," ;");
         else
            stmt = concat(4,"coefficient ",qual,ref," ;");

         codegen_wrap_write(stmt, 1, 0);
         free(stmt);

         filename = tablo_filename(tablo_type(cur));
         if( tablo_need_read(cur) )
            addlist(files,filename);

         free(name);
         freelist(val);
         free(qual);
         free(ref);
         }

   //
   //  write out declarations of logical file names
   //

   fprintf(code,"\n");
   if( files->n )
      {
      for( itm=files->first ; itm ; itm=itm->next )
         fprintf(code,"file %s ;\n",itm->str);
      fprintf(code,"\n");
      }

   //
   //  write out read statements
   //

   for( cur=firstsymbol(var) ; cur ; cur=nextsymbol(cur) )
      if( tablo_show_symbol(cur) && tablo_need_read(cur) )
         {
         name = symname(cur);
         val  = symvalue(cur);
         qual = tabloqualifier(val);
         ref  = tablovar(name,val,0);

         atts = symattrib(cur);
         filename = tablo_filename(tablo_type(cur));
         strncpy(buf,atts->first->str,10);

         stmt = concat(9,"read ",qual,"\n   ",ref," from file ",filename," header \"",buf,"\" ;");
         codegen_wrap_write(stmt, 1, 0);
         free(stmt);

         free(name);
         freelist(val);
         free(qual);
         free(ref);
         }

}


//----------------------------------------------------------------------//
//  Tablo_begin_file()
//
//  Begin processing the file
//----------------------------------------------------------------------//

void Tablo_begin_file(char *basename)
{
   if( do_calc == 0 )
      {
      fprintf(code,"equation    (default=levels)       ;\n");
      fprintf(code,"equation    (default=add_homotopy) ;\n");
      fprintf(code,"variable    (default=levels)       ;\n");
      }
   else
      {
      fprintf(code,"formula     (default=initial)      ;\n");
      calcvars = newlist();
      }

   fprintf(code,"coefficient (default=parameter)    ;\n");
   fprintf(code,"\n");
}


//----------------------------------------------------------------------//
//  Tablo_end_file()
//
//  End processing the file and print a lot of diagnostic information.
//----------------------------------------------------------------------//

void Tablo_end_file()
{
   int ucount;
   void *cur;
   List *atts;
   int nv[h_unk+1];
   int i,n,nv_end,nv_exo,nv_unk,nv_tot;
   enum har_type har;
   int ndiff;
   Item *itm;
   List *val;
   char *qual,*ref;
   char buf[10];
   char *stmt;

   //
   //  count unused variables and scalar variables of each type
   //

   for( i=0 ; i<=h_unk ; i++)
      nv[i] = 0;

   ucount = 0;
   for( cur=firstsymbol(var) ; cur ; cur=nextsymbol(cur) )
      if( !isused(cur) )
         ucount++;
      else {
         har = tablo_type(cur);
         nv[har] += symsize(cur);
      }

   //
   //  if we're in calc mode then add some write statements
   //

   if( do_calc )
      {

      fprintf(code,"\nfile (new) calc ;\n\n");

      validate( calcvars, LISTOBJ, "Tablo_end_file" );
      for( itm=calcvars->first ; itm ; itm=itm->next )
         {
         cur  = lookup(itm->str);
         val  = symvalue(cur);
         qual = tabloqualifier(val);
         ref  = tablovar(itm->str,val,0);
         atts = symattrib(cur) ;
         if( atts->n == 1 )
            {
            strncpy(buf,atts->first->str,10);
            stmt = concat(7,"write ",qual,"\n   ",ref," to file calc header \"",buf,"\" ;\n");
            codegen_wrap_write(stmt, 1, 0);
            free(stmt);
            }
         }

      fprintf(code,"\n");

      }

   //
   //  talk about the vector model
   //

   fprintf(info,"\nVector information:\n");
   fprintf(info,"\n   Equations: %d\n",Tablo_eqn);
   fprintf(info,"   Variables, Used: %d\n",Tablo_var-ucount);
   fprintf(info,"   Variables, Unused: %d\n",ucount);
   fprintf(info,"   Parameters: %d\n",Tablo_par);

   fprintf(info,"\nTime information:\n");
   fprintf(info,"\n   Periods used: %d\n",setsize("time"));

   //
   //  tabulate and talk about the scalar model
   //

   nv_tot = 0;
   nv_end = 0;
   nv_exo = 0;
   nv_unk = 0;

   for( i=0 ; i<=h_unk ; i++)
      switch( (enum har_type) i ) {
         case h_aen:
         case h_end:
         case h_int:
         case h_iot:
         case h_ext:
            nv_end += nv[i];
            nv_tot += nv[i];
            break;

         case h_unk:
            nv_unk += nv[i];
            nv_tot += nv[i];
            break;

         case h_aex:
         case h_exo:
         case h_kal:
         case h_mak:
            nv_exo += nv[i];
            nv_tot += nv[i];
            break;

         case h_apa:
         case h_par:
            break;

         default:
            nv_tot += nv[i];
      }

   fprintf(info,"\nScalar information:\n");
   fprintf(info,"\n   Equations: %d\n",Tablo_scalar_eqn);
   fprintf(info,"\n   Endogenous variables: %d\n",nv_end);
   fprintf(info,"      Type %s: %d\n",tablo_filename(h_end),nv[h_end]);
   fprintf(info,"      Type %s: %d\n",tablo_filename(h_int),nv[h_int]);
   fprintf(info,"      Type %s: %d\n",tablo_filename(h_iot),nv[h_iot]);
   fprintf(info,"      Type %s: %d\n",tablo_filename(h_ext),nv[h_ext]);
   fprintf(info,"      Type %s: %d\n",tablo_filename(h_aen),nv[h_aen]);

   ndiff = Tablo_scalar_eqn - nv_end;

   fprintf(info,"\n   Closure:\n");
   if( ndiff == 0 )fprintf(info,"      Equations and variables match\n");
   if( ndiff >  0 )fprintf(info,"      Excess equations: %d\n",ndiff);
   if( ndiff <  0 )fprintf(info,"      Excess variables: %d\n",-ndiff);

   fprintf(info,"\n   Exogenous variables: %d\n",nv_exo);
   fprintf(info,"      Type %s: %d\n",tablo_filename(h_exo),nv[h_exo]);
   fprintf(info,"      Type %s: %d\n",tablo_filename(h_kal),nv[h_kal]);
   fprintf(info,"      Type %s: %d\n",tablo_filename(h_mak),nv[h_mak]);
   fprintf(info,"      Type %s: %d\n",tablo_filename(h_aex),nv[h_aex]);

   fprintf(info,"\n   Undetermined variables: %d\n",nv_unk);
   fprintf(info,"      Type %s: %d\n",tablo_filename(h_unk),nv[h_unk]);

   for( cur=firstsymbol(var) ; cur ; cur=nextsymbol(cur) )
      if( isused(cur) && tablo_type(cur) == h_unk )
         fprintf(info,"      %-13s: %d\n",symname(cur),symsize(cur));

   if( nv_tot != (nv_end + nv_exo + nv_unk) )
      fprintf(info,"\nWarning: inconsistent variable count\n");

}

//----------------------------------------------------------------------//
//
//  Begin an equation
//
//----------------------------------------------------------------------//

void Tablo_begin_block(void *eq)
{
   char *qual;
   List *eqnsets();
   char *name,*eqname();
   char tbuf[10];
   Node *getlhs();

   if( Tablo_eqn == 0 )
      Tablo_writedecs();

   Tablo_eqn++;
   Tablo_scalar_eqn += eqncount(eq);

   qual = tabloqualifier(eqnsets(eq));
   name = eqname(eq);

   if( do_calc == 0 )
      if( name )
         fprintf(code,"\nequation %s %s\n   ",name,qual);
      else
         fprintf(code,"\nequation EQN%d %s\n   ",Tablo_eqn,qual);
   else
      {
      if( islvalue(eq)==0 )
         {
         sprintf(tbuf,"%d",Tablo_eqn);
         tablo_error("LHS of equation %s in calc mode is not a variable",tbuf);
         }
      else
         {
         fprintf(code,"\nformula %s\n   ",qual);
         validate( calcvars, LISTOBJ, "Tablo_bgn_block" );
         addlist(calcvars, getlhs(eq)->str);
         }
      }

   free(qual);
}


//----------------------------------------------------------------------//
//
//  End an equation
//
//----------------------------------------------------------------------//

void Tablo_end_eqn(void *eq)
{

   fprintf(code," ;\n");

}

//----------------------------------------------------------------------//
//
//  Begin a function
//
//----------------------------------------------------------------------//

char *Tablo_begin_func(char *func, char *arg)
{
   Tabset *cur;

   if( isequal(func,"sum") || isequal(func,"prod") )
      {
      cur = (Tabset *) getvalue(tabsets,arg);
      if( cur==0 )FAULT("null pointer in Tablo_begin_func");
      return concat(6,func,"(",cur->index,",",arg,",");
      }

   if( arg )
      FAULT("unexpected function call in Tablo_begin_func");

   if( isequal(func,"log") )
      return strdup("loge(");

   return concat(2,func,"(");
}

//----------------------------------------------------------------------//
//
//  Show a symbol
//
//----------------------------------------------------------------------//

char *Tablo_show_symbol(char *str, List *setlist, Context context)
{
   return tablovar(str,setlist,context.dt);
}


//----------------------------------------------------------------------//
//
//  Connect up public routines
//
//----------------------------------------------------------------------//

void Tablo_setup(void)
{
   lang_begin_file ( Tablo_begin_file  );
   lang_end_file   ( Tablo_end_file    );
   lang_declare    ( Tablo_declare     );
   lang_begin_block( Tablo_begin_block );
   lang_end_eqn    ( Tablo_end_eqn     );
   lang_begin_func ( Tablo_begin_func  );
   lang_show_symbol( Tablo_show_symbol );

   set_eqn_vector();
   set_sum_vector();

   set_line_length(75);
   set_alpha_elements();
   set_explicit_time();
   set_reserved_word("to");
   set_reserved_word("from");
}

char *Tablo_version = "$Revision: 57 $";

