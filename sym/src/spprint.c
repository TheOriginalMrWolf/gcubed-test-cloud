/*-------------------------------------------------------------------*
 *  SPPRINT.C 2.25
 *  20 Mar 90 (PJW)
 *
 *  Print a node into a character buffer following precedence rules
 *-------------------------------------------------------------------*/

#include "spprint.h"

#include "codegen.h"
#include "error.h"
#include "nodes.h"
#include "str.h"
#include "sym.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*-------------------------------------------------------------------*
 *  node2string
 * 
 *  Return a new string corresponding to the node.  Don't do any
 *  fancy indenting or line breaking.
 *  Used for logging errors and debugging messages.
 *-------------------------------------------------------------------*/
char *node2string(Node *node)
{
   return codegen_spprint(nul, node, 0);
}

/*-------------------------------------------------------------------*
 *  snprint
 *
 *  Streamlined interface; returns a newly-allocated string.
 *-------------------------------------------------------------------*/
char *snprint(Node *node)
{
   return codegen_spprint(nul, node, "   ");
}


/*-------------------------------------------------------------------*
 *  sniprint
 *
 *  Selectable indenting.
 *-------------------------------------------------------------------*/
char *sniprint(Node *node,char *indent)
{
   return codegen_spprint(nul, node, indent);
}

