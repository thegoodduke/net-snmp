/******************************************************************
	Copyright 1989, 1991, 1992 by Carnegie Mellon University

                      All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of CMU not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

CMU DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
CMU BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
******************************************************************/
/*
 * parse.c
 */
#include <config.h>

#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>

/* Wow.  This is ugly.  -- Wes */
#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include "parse.h"

/* A quoted string value-- too long for a general "token" */
char *quoted_string_buffer;

/*
 * This is one element of an object identifier with either an integer
 * subidentifier, or a textual string label, or both.
 * The subid is -1 if not present, and label is NULL if not present.
 */
struct subid {
    int subid;
    char *label;
};

#define MAXTC	256
struct tc {	/* textual conventions */
    int type;
    char descriptor[MAXTOKEN];
    struct enum_list *enums;
} tclist[MAXTC];



int Line = 1;
int save_mib_descriptions = 0;

#define SYNTAX_MASK	0x80
/* types of tokens
 Tokens wiht the SYNTAX_MASK bit set are syntax tokens */
#define	CONTINUE    -1
#define ENDOFFILE   0
#define LABEL	    1
#define SUBTREE	    2
#define SYNTAX	    3
#define OBJID	    (4 | SYNTAX_MASK)
#define OCTETSTR    (5 | SYNTAX_MASK)
#define INTEGER	    (6 | SYNTAX_MASK)
#define INTEGER32   (INTEGER | SYNTAX_MASK)
#define NETADDR	    (7 | SYNTAX_MASK)
#define	IPADDR	    (8 | SYNTAX_MASK)
#define COUNTER	    (9 | SYNTAX_MASK)
#define GAUGE	    (10 | SYNTAX_MASK)
#define TIMETICKS   (11 | SYNTAX_MASK)
#define OPAQUE	    (12 | SYNTAX_MASK)
#define NUL	    (13 | SYNTAX_MASK)
#define SEQUENCE    14
#define OF	    15	/* SEQUENCE OF */
#define OBJTYPE	    16
#define ACCESS	    17
#define READONLY    18
#define READWRITE   19
#define	WRITEONLY   20
#define NOACCESS    21
#define STATUS	    22
#define MANDATORY   23
#define OPTIONAL    24
#define OBSOLETE    25
/* #define RECOMMENDED 26 */
#define PUNCT	    27
#define EQUALS	    28
#define NUMBER	    29
#define LEFTBRACKET 30
#define RIGHTBRACKET 31
#define	LEFTPAREN   32
#define RIGHTPAREN  33
#define COMMA	    34
#define DESCRIPTION 35
#define QUOTESTRING 36
#define INDEX       37
#define DEFVAL      38
#define DEPRECATED  39
#define SIZE        40
#define BITSTRING   (41 | SYNTAX_MASK)
#define NSAPADDRESS (42 | SYNTAX_MASK)
#define COUNTER64   (43 | SYNTAX_MASK)
#define OBJGROUP    44
#define NOTIFTYPE   45
#define AUGMENTS    46
#define COMPLIANCE  47
#define READCREATE  48
#define UNITS       49
#define REFERENCE   50
#define NUM_ENTRIES 51
#define MODULEIDENTITY 52
#define LASTUPDATED 53
#define ORGANIZATION 54
#define CONTACTINFO 55
#define UINTEGER32 (56 | SYNTAX_MASK)
#define CURRENT	    57
#define DEFINITIONS 58
#define END         59
#define SEMI        60
#define TRAPTYPE    61
#define ENTERPRISE  62
/* #define DISPLAYSTR (63 | SYNTAX_MASK) */
#define BEGIN       64
#define IMPORTS     65
#define EXPORTS     66

struct tok {
	char *name;			/* token name */
	int len;			/* length not counting nul */
	int token;			/* value */
	int hash;			/* hash of name */
	struct tok *next;		/* pointer to next in hash table */
};


struct tok tokens[] = {
	{ "obsolete", sizeof ("obsolete")-1, OBSOLETE },
	{ "Opaque", sizeof ("Opaque")-1, OPAQUE },
/*	{ "recommended", sizeof("recommended")-1, RECOMMENDED },  */
	{ "optional", sizeof ("optional")-1, OPTIONAL },
	{ "LAST-UPDATED", sizeof ("LAST-UPDATED")-1, LASTUPDATED },
	{ "ORGANIZATION", sizeof ("ORGANIZATION")-1, ORGANIZATION },
	{ "CONTACT-INFO", sizeof ("CONTACT-INFO")-1, CONTACTINFO },
	{ "MODULE-IDENTITY", sizeof ("MODULE-IDENTITY")-1, MODULEIDENTITY },
	{ "MODULE-COMPLIANCE", sizeof ("MODULE-COMPLIANCE")-1, COMPLIANCE },
        { "DEFINITIONS", sizeof("DEFINITIONS")-1, DEFINITIONS},
        { "END", sizeof("END")-1, END},
        { ";", sizeof(";")-1, SEMI},
	{ "AUGMENTS", sizeof ("AUGMENTS")-1, AUGMENTS },
	{ "not-accessible", sizeof ("not-accessible")-1, NOACCESS },
	{ "write-only", sizeof ("write-only")-1, WRITEONLY },
	{ "NsapAddress", sizeof("NsapAddress")-1, NSAPADDRESS},
	{ "UNITS", sizeof("Units")-1, UNITS},
	{ "REFERENCE", sizeof("REFERENCE")-1, REFERENCE},
	{ "NUM-ENTRIES", sizeof("NUM-ENTRIES")-1, NUM_ENTRIES},
	{ "BITSTRING", sizeof("BitString")-1, BITSTRING},
	{ "BIT", sizeof("BIT")-1, CONTINUE},
	{ "Counter64", sizeof("Counter64")-1, COUNTER64},
	{ "TimeTicks", sizeof ("TimeTicks")-1, TIMETICKS },
	{ "NOTIFICATION-TYPE", sizeof ("NOTIFICATION-TYPE")-1, NOTIFTYPE },
	{ "OBJECT-GROUP", sizeof ("OBJECT-GROUP")-1, OBJGROUP },
	{ "OBJECTIDENTIFIER", sizeof ("OBJECTIDENTIFIER")-1, OBJID },
	/*
	 * This CONTINUE appends the next word onto OBJECT,
	 * hopefully matching OBJECTIDENTIFIER above.
	 */
	{ "OBJECT", sizeof ("OBJECT")-1, CONTINUE },
	{ "NetworkAddress", sizeof ("NetworkAddress")-1, NETADDR },
	{ "Gauge", sizeof ("Gauge")-1, GAUGE },
	{ "read-write", sizeof ("read-write")-1, READWRITE },
	{ "read-create", sizeof ("read-create")-1, READCREATE },
	{ "OCTETSTRING", sizeof ("OCTETSTRING")-1, OCTETSTR },
	{ "OCTET", sizeof ("OCTET")-1, -1 },
	{ "OF", sizeof ("OF")-1, OF },
	{ "SEQUENCE", sizeof ("SEQUENCE")-1, SEQUENCE },
	{ "NULL", sizeof ("NULL")-1, NUL },
	{ "IpAddress", sizeof ("IpAddress")-1, IPADDR },
	{ "UInteger32", sizeof ("UInteger32")-1, UINTEGER32 },
	{ "INTEGER", sizeof ("INTEGER")-1, INTEGER },
	{ "Counter", sizeof ("Counter")-1, COUNTER },
	{ "read-only", sizeof ("read-only")-1, READONLY },
        { "DESCRIPTION", sizeof ("DESCRIPTION")-1, DESCRIPTION },
        { "INDEX", sizeof ("INDEX")-1, INDEX },
        { "DEFVAL", sizeof ("DEFVAL")-1, DEFVAL },
        { "deprecated", sizeof ("deprecated")-1, DEPRECATED },
        { "SIZE", sizeof ("SIZE")-1, SIZE },
	{ "MAX-ACCESS", sizeof ("MAX-ACCESS")-1, ACCESS },
	{ "ACCESS", sizeof ("ACCESS")-1, ACCESS },
	{ "mandatory", sizeof ("mandatory")-1, MANDATORY },
	{ "current", sizeof ("current")-1, CURRENT },
	{ "STATUS", sizeof ("STATUS")-1, STATUS },
	{ "SYNTAX", sizeof ("SYNTAX")-1, SYNTAX },
	{ "OBJECT-TYPE", sizeof ("OBJECT-TYPE")-1, OBJTYPE },
	{ "{", sizeof ("{")-1, LEFTBRACKET },
	{ "}", sizeof ("}")-1, RIGHTBRACKET },
	{ "::=", sizeof ("::=")-1, EQUALS },
	{ "(", sizeof ("(")-1, LEFTPAREN },
	{ ")", sizeof (")")-1, RIGHTPAREN },
	{ ",", sizeof (",")-1, COMMA },
	{ "TRAP-TYPE", sizeof ("TRAP-TYPE")-1, TRAPTYPE },
	{ "ENTERPRISE", sizeof ("ENTERPRISE")-1, ENTERPRISE },
	/*	{ "DisplayString", sizeof ("DisplayString")-1, DISPLAYSTR },
	{ "Integer32", sizeof ("Integer32")-1, INTEGER32 }, */
	{ "BEGIN", sizeof ("BEGIN")-1, BEGIN },
	{ "IMPORTS", sizeof ("IMPORTS")-1, IMPORTS },
	{ "EXPORTS", sizeof ("EXPORTS")-1, IMPORTS },
	{ NULL }
};

#define	HASHSIZE	32
#define	BUCKET(x)	(x & 0x01F)

struct tok	*buckets[HASHSIZE];

static void do_subtree();
static int get_token();
static int parseQuoteString();
static int tossObjectIdentifier();

static
hash_init()
{
	register struct tok	*tp;
	register char	*cp;
	register int	h;
	register int	b;

#ifdef SVR4
	memset((char *)buckets, NULL, sizeof(buckets));
#else
	bzero((char *)buckets, sizeof(buckets));
#endif
	for (tp = tokens; tp->name; tp++) {
		for (h = 0, cp = tp->name; *cp; cp++)
			h += *cp;
		tp->hash = h;
		b = BUCKET(h);
		if (buckets[b])
		    tp->next = buckets[b]; /* BUG ??? */
		buckets[b] = tp;
	}
}

#define NHASHSIZE    128
#define NBUCKET(x)   (x & 0x7F)
struct node *nbuckets[NHASHSIZE];

init_node_hash(nodes)
     struct node *nodes;
{
     register struct node *np, *nextp;
     register char *cp;
     register int hash;

#ifdef SVR4
     memset((char *)nbuckets, NULL, sizeof(nbuckets));
#else
     bzero((char *)nbuckets,sizeof(nbuckets));
#endif
     for(np = nodes; np;){
         nextp = np->next;
         hash = 0;
	 for(cp = np->parent; *cp; cp++)
	     hash += *cp;
	 np->next = nbuckets[NBUCKET(hash)];
	 nbuckets[NBUCKET(hash)] = np;
	 np = nextp;
     }
}

static char *
Malloc(num)
    unsigned num;
{
    int malloc();
    
    /* this is to fix (what seems to be) a problem with the IBM RT C
library malloc */
    if (num < 16)
	num = 16;
    return (char *)malloc(num);
}

static
print_error(string, token, type)
    char *string;
    char *token;
    int type;
{
    if (type == ENDOFFILE)
	fprintf(stderr, "%s(EOF): On or around line %d\n", string, Line);
    else if (token)
	fprintf(stderr, "%s(%s): On or around line %d\n", string, token, Line);
    else
	fprintf(stderr, "%s: On or around line %d\n", string, Line);
}

#ifdef TEST
print_subtree(tree, count)
    struct tree *tree;
    int count;
{
    struct tree *tp;
    int i;

    for(i = 0; i < count; i++)
	printf("  ");
    printf("Children of %s:\n", tree->label);
    count++;
    for(tp = tree->child_list; tp; tp = tp->next_peer){
	for(i = 0; i < count; i++)
	    printf("  ");
	printf("%s\n", tp->label);
    }
    for(tp = tree->child_list; tp; tp = tp->next_peer){
	print_subtree(tp, count);
    }
}
#endif /* TEST */

int translation_table[256];

build_translation_table(){
    int count;

    for(count = 0; count < 256; count++){
	switch(count){
	    case OBJID:
		translation_table[count] = TYPE_OBJID;
		break;
	    case OCTETSTR:
		translation_table[count] = TYPE_OCTETSTR;
		break;
	    case INTEGER:
		translation_table[count] = TYPE_INTEGER;
		break;
	    case NETADDR:
		translation_table[count] = TYPE_IPADDR;
		break;
	    case IPADDR:
		translation_table[count] = TYPE_IPADDR;
		break;
	    case COUNTER:
		translation_table[count] = TYPE_COUNTER;
		break;
	    case GAUGE:
		translation_table[count] = TYPE_GAUGE;
		break;
	    case TIMETICKS:
		translation_table[count] = TYPE_TIMETICKS;
		break;
	    case OPAQUE:
		translation_table[count] = TYPE_OPAQUE;
		break;
	    case NUL:
		translation_table[count] = TYPE_NULL;
		break;
	    case COUNTER64:
		translation_table[count] = TYPE_COUNTER64;
		break;
	    case BITSTRING:
		translation_table[count] = TYPE_BITSTRING;
		break;
	    case NSAPADDRESS:
		translation_table[count] = TYPE_NSAPADDRESS;
		break;
	    case UINTEGER32:
		translation_table[count] = TYPE_UINTEGER;
		break;
	    default:
		translation_table[count] = TYPE_OTHER;
		break;
	}
    }
}

static struct tree *
build_tree(nodes)
    struct node *nodes;
{
    struct node *np;
    struct tree *tp, *lasttp;
    int bucket, nodes_left = 0;
    
    build_translation_table();
    /* grow tree from this root node */
    init_node_hash(nodes);

    /* build root node */
    tp = (struct tree *)Malloc(sizeof(struct tree));
    tp->parent = NULL;
    tp->next_peer = NULL;
    tp->child_list = NULL;
    tp->enums = NULL;
    strcpy(tp->label, "joint-iso-ccitt");
    tp->subid = 2;
    tp->tc_index = -1;
    tp->type = 0;
    tp->description = NULL;
    /* XXX nodes isn't needed in do_subtree() ??? */
    do_subtree(tp, &nodes);
    lasttp = tp;

    /* build root node */
    tp = (struct tree *)Malloc(sizeof(struct tree));
    tp->parent = NULL;
    tp->next_peer = lasttp;
    tp->child_list = NULL;
    tp->enums = NULL;
    strcpy(tp->label, "ccitt");
    tp->subid = 0;
    tp->tc_index = -1;
    tp->type = 0;
    tp->description = NULL;
    /* XXX nodes isn't needed in do_subtree() ??? */
    do_subtree(tp, &nodes);
    lasttp = tp;

    /* build root node */
    tp = (struct tree *)Malloc(sizeof(struct tree));
    tp->parent = NULL;
    tp->next_peer = lasttp;
    tp->child_list = NULL;
    tp->enums = NULL;
    strcpy(tp->label, "iso");
    tp->subid = 1;
    tp->tc_index = -1;
    tp->type = 0;
    tp->description = NULL;
    /* XXX nodes isn't needed in do_subtree() ??? */
    do_subtree(tp, &nodes);


#ifdef TEST
    print_subtree(tp, 0);
#endif /* TEST */
    /* If any nodes are left, the tree is probably inconsistent */
    for(bucket = 0; bucket < NHASHSIZE; bucket++){
        if (nbuckets[bucket]){
	    nodes_left = 1;
	    break;
	}
    }
    if (nodes_left){
	fprintf(stderr, "The mib description doesn't seem to be consistent.\n");
	fprintf(stderr, "Some nodes couldn't be linked under the \"iso\" tree.\n");
	fprintf(stderr, "these nodes are left:\n");
	for(bucket = 0; bucket < NHASHSIZE; bucket++){
	    for(np = nbuckets[bucket]; np; np = np->next)
	        fprintf(stderr, "%s ::= { %s %d } (%d)\n", np->label,
			np->parent, np->subid, np->type);
	}
    }
    return tp;
}

/*
 * Find all the children of root in the list of nodes.  Link them into the
 * tree and out of the nodes list.
 */
static void
do_subtree(root, nodes)
    struct tree *root;
    struct node **nodes;
{
    register struct tree *tp;
    struct tree *peer = NULL;
    register struct node *np, **headp;
    struct node *oldnp = NULL, *child_list = NULL, *childp = NULL;
    char *cp;
    int hash;
    
    tp = root;
    hash = 0;
    for(cp = tp->label; *cp; cp++)
        hash += *cp;
    headp = &nbuckets[NBUCKET(hash)];
    /*
     * Search each of the nodes for one whose parent is root, and
     * move each into a separate list.
     */
    for(np = *headp; np; np = np->next){
	if ((*tp->label != *np->parent) || strcmp(tp->label, np->parent)){
	    if ((*tp->label == *np->label) && !strcmp(tp->label, np->label)){
		/* if there is another node with the same label, assume that
		 * any children after this point in the list belong to the other node.
		 * This adds some scoping to the table and allows vendors to
		 * reuse names such as "ip".
		 */
		break;
	    }
	    oldnp = np;
	} else {
	    if (child_list == NULL){
		child_list = childp = np;   /* first entry in child list */
	    } else {
		childp->next = np;
		childp = np;
	    }
	    /* take this node out of the node list */
	    if (oldnp == NULL){
		*headp = np->next;  /* fix root of node list */
	    } else {
		oldnp->next = np->next;	/* link around this node */
	    }
	}
    }
    if (childp)
	childp->next = 0;	/* re-terminate list */
    /*
     * Take each element in the child list and place it into the tree.
     */
    for(np = child_list; np; np = np->next){
	tp = (struct tree *)Malloc(sizeof(struct tree));
	tp->parent = root;
	tp->next_peer = NULL;
	tp->child_list = NULL;
	strcpy(tp->label, np->label);
	tp->subid = np->subid;
      tp->tc_index = np->tc_index;
	tp->type = translation_table[np->type];
	tp->enums = np->enums;
	np->enums = NULL;	/* so we don't free them later */
	tp->description = np->description; /* steals memory from np */
	np->description = NULL; /* so we don't free it later */
	if (root->child_list == NULL){
	    root->child_list = tp;
	} else {
	    peer->next_peer = tp;
	}
	peer = tp;
/*	if (tp->type == TYPE_OTHER) */
	    do_subtree(tp, nodes);	/* recurse on this child if it isn't
					   an end node */
    }
    /* free all nodes that were copied into tree */
    oldnp = NULL;
    for(np = child_list; np; np = np->next){
	if (oldnp)
	    free(oldnp);
	oldnp = np;
    }
    if (oldnp)
	free(oldnp);
}


/*
 * Takes a list of the form:
 * { iso org(3) dod(6) 1 }
 * and creates several nodes, one for each parent-child pair.
 * Returns NULL on error.
 */
static int
getoid(fp, oid,  length)
    register FILE *fp;
    register struct subid *oid;	/* an array of subids */
    int length;	    /* the length of the array */
{
    register int count;
    int type;
    char token[MAXTOKEN];
    register char *cp;

    if ((type = get_token(fp, token,MAXTOKEN)) != LEFTBRACKET){
	print_error("Expected \"{\"", token, type);
	return 0;
    }
    type = get_token(fp, token,MAXTOKEN);
    for(count = 0; count < length; count++, oid++){
	oid->label = 0;
	oid->subid = -1;
	if (type == RIGHTBRACKET){
	    return count;
	} else if (type != LABEL && type != NUMBER){
	    print_error("Not valid for object identifier", token, type);
	    return 0;
	}
	if (type == LABEL){
	    /* this entry has a label */
	    cp = (char *)Malloc((unsigned)strlen(token) + 1);
	    strcpy(cp, token);
	    oid->label = cp;
	    type = get_token(fp, token,MAXTOKEN);
	    if (type == LEFTPAREN){
		type = get_token(fp, token,MAXTOKEN);
		if (type == NUMBER){
		    oid->subid = atoi(token);
		    if ((type = get_token(fp, token,MAXTOKEN)) != RIGHTPAREN){
			print_error("Unexpected a closing parenthesis", token, type);
			return 0;
		    }
		} else {
		    print_error("Expected a number", token, type);
		    return 0;
		}
	    } else {
		continue;
	    }
	} else {
	    /* this entry  has just an integer sub-identifier */
	    oid->subid = atoi(token);
	}
	type = get_token(fp, token,MAXTOKEN);
    }
    return count;


}

static 
free_tree(Tree)
    struct tree *Tree;
{
    if (Tree == NULL)
    {
	return;
    }

    if (Tree->enums)
    {
	struct enum_list *ep, *tep;

	ep = Tree->enums;
	while(ep)
	{
	    tep = ep;
	    ep = ep->next;
	    if (tep->label)
		free(tep->label);
	
	    free((char *)tep);
	}
    }

    if (Tree->description)
    {
	free(Tree->description);
    }

    free_tree(Tree->child_list);
}

static
free_node(np)
    struct node *np;
{
    struct enum_list *ep, *tep;

    ep = np->enums;
    while(ep){
	tep = ep;
	ep = ep->next;
	if (tep->label)
	    free(tep->label);
	
	free((char *)tep);
    }
    if (np->description)
	free(np->description);
    
    free((char *)np);
}

/*
 * Parse an entry of the form:
 * label OBJECT IDENTIFIER ::= { parent 2 }
 * The "label OBJECT IDENTIFIER" portion has already been parsed.
 * Returns 0 on error.
 */
static struct node *
parse_objectid(fp, name)
    FILE *fp;
    char *name;
{
    int type;
    char token[MAXTOKEN];
    register int count;
    register struct subid *op, *nop;
    int length;
    struct subid oid[32];
    struct node *np, *root, *oldnp = NULL;

    type = get_token(fp, token,MAXTOKEN);
    if (type != EQUALS){
	print_error("Bad format", token, type);
	return 0;
    }
    if ((length = getoid(fp, oid, 32)) != 0){
	np = root = (struct node *)Malloc(sizeof(struct node));
#ifdef SVR4
	memset((char *)np, NULL, sizeof(struct node));
#else
	bzero((char *)np, sizeof(struct node));
#endif
	/*
	 * For each parent-child subid pair in the subid array,
	 * create a node and link it into the node list.
	 */
	for(count = 0, op = oid, nop=oid+1; count < (length - 2); count++,
	    op++, nop++){
	    /* every node must have parent's name and child's name or number */
	    if (op->label && (nop->label || (nop->subid != -1))){
		strcpy(np->parent, op->label);
		if (nop->label)
		    strcpy(np->label, nop->label);
		if (nop->subid != -1)
		    np->subid = nop->subid;
              np->tc_index = -1;
		np->type = 0;
		np->enums = 0;
		/* set up next entry */
		np->next = (struct node *)Malloc(sizeof(*np->next));
#ifdef SVR4
		memset((char *)np->next, NULL, sizeof(struct node));
#else
		bzero((char *)np->next, sizeof(struct node));
#endif
		oldnp = np;
		np = np->next;
	    }
	}
	np->next = (struct node *)NULL;
	/*
	 * The above loop took care of all but the last pair.  This pair is taken
	 * care of here.  The name for this node is taken from the label for this
	 * entry.
	 * np still points to an unused entry.
	 */
	if (count == (length - 2)){
	    if (op->label){
		strcpy(np->parent, op->label);
		strcpy(np->label, name);
		if (nop->subid != -1)
		    np->subid = nop->subid;
		else
		    print_error("Warning: This entry is pretty silly",
				np->label, type);
	    } else {
		free_node(np);
		if (oldnp)
		    oldnp->next = NULL;
		else
		    return NULL;
	    }
	} else {
	    print_error("Missing end of oid", (char *)NULL, type);
	    free_node(np);   /* the last node allocated wasn't used */
	    if (oldnp)
		oldnp->next = NULL;
	    return NULL;
	}
	/* free the oid array */
	for(count = 0, op = oid; count < length; count++, op++){
	    if (op->label)
		free(op->label);
	    op->label = 0;
	}
	return root;
    } else {
	print_error("Bad object identifier", (char *)NULL, type);
	return 0;
    }
}

static int
get_tc(descriptor, ep)
    char *descriptor;
    struct enum_list **ep;
{
    int i;

    for(i = 0; i < MAXTC; i++){
	if (tclist[i].type == 0)
	    break;
	if (!strcmp(descriptor, tclist[i].descriptor)){
	    *ep = tclist[i].enums;
	    return tclist[i].type;
	}
    }
    return LABEL;
}

/* return index into tclist of given TC descriptor
   return -1 if not found
 */
static int
get_tc_index(descriptor)
    char *descriptor;
{
    int i;

    for(i = 0; i < MAXTC; i++){
      if (tclist[i].type == 0)
          break;
      if (!strcmp(descriptor, tclist[i].descriptor)){
          return i;
      }
    }
    return -1;
}

/*
 * Parses an asn type.  Structures are ignored by this parser.
 * Returns NULL on error.
 */
static int
parse_asntype(fp, name, ntype, ntoken)
    FILE *fp;
    char *name;
    int *ntype;
    char *ntoken;
{
    int type, i;
    char token[MAXTOKEN];
    struct enum_list *ep;
    struct tc *tcp;
    int level;
    
    type = get_token(fp, token,MAXTOKEN);
    if (type == SEQUENCE){
	while((type = get_token(fp, token,MAXTOKEN)) != ENDOFFILE){
	    if (type == RIGHTBRACKET){
		*ntype = get_token(fp, ntoken,MAXTOKEN);
		return 1;
	    }
	}
	print_error("Expected \"}\"", token, type);
	return 0;
    } else {
	if (!strcmp(token, "TEXTUAL-CONVENTION")){
          while (type != SYNTAX) 
            type = get_token(fp, token,MAXTOKEN);
          type = get_token(fp, token,MAXTOKEN);
	}

	if (type == LABEL)
	{
	    type = get_tc(token, &ep);
	}
	
	
	/* textual convention */
	for(i = 0; i < MAXTC; i++){
	    if (tclist[i].type == 0)
		break;
	}

	if (i == MAXTC){
	    print_error("No more textual conventions possible.", token, type);
	    return 0;
	}
	tcp = &tclist[i];
	strcpy(tcp->descriptor, name);
	if (!(type & SYNTAX_MASK)){
	    print_error("Textual convention doesn't map to real type.", token,
			type);
	    return 0;
	}
	tcp->type = type; 
	*ntype = get_token(fp, ntoken,MAXTOKEN);
	if (*ntype == LEFTPAREN){
	    level = 1;
	    /* don't record any constraints for now */
	    while(level > 0){
		*ntype = get_token(fp, ntoken,MAXTOKEN);
		if (*ntype == LEFTPAREN)
		    level++;
		if (*ntype == RIGHTPAREN)
		    level--;		
	    }
	    *ntype = get_token(fp, ntoken,MAXTOKEN);
	} else if (*ntype == LEFTBRACKET) {
	    /* if there is an enumeration list, parse it */
	    while((type = get_token(fp, token,MAXTOKEN)) != ENDOFFILE){
		if (type == RIGHTBRACKET)
		    break;
		if (type == LABEL){
		    /* this is an enumerated label */
		    if (tcp->enums == 0){
			ep = tcp->enums = (struct enum_list *)
			    Malloc(sizeof(struct enum_list));
		    } else {
			ep->next = (struct enum_list *)
			    Malloc(sizeof(struct enum_list));
			ep = ep->next;
		    }
		    ep->next = 0;
		    /* a reasonable approximation for the length */
		    ep->label =
			(char *)Malloc((unsigned)strlen(token) + 1);
		    strcpy(ep->label, token);
		    type = get_token(fp, token,MAXTOKEN);
		    if (type != LEFTPAREN){
			print_error("Expected \"(\"", token, type);
			/* free_node(np); */
			return 0;
		    }
		    type = get_token(fp, token,MAXTOKEN);
		    if (type != NUMBER){
			print_error("Expected integer", token, type);
			/* free_node(np); */
			return 0;
		    }
		    ep->value = atoi(token);
		    type = get_token(fp, token,MAXTOKEN);
		    if (type != RIGHTPAREN){
			print_error("Expected \")\"", token, type);
			/* free_node(np); */
			return 0;
		    }
		}
	    }
	    if (type == ENDOFFILE){
		print_error("Expected \"}\"", token, type);
		/* free_node(np); */
		return 0;
	    }
	    *ntype = get_token(fp, ntoken,MAXTOKEN);
	}
	return 1;
    }
}


/*
 * Parses an OBJECT TYPE macro.
 * Returns 0 on error.
 */
static struct node *
parse_objecttype(fp, name)
    register FILE *fp;
    char *name;
{
    register int type;
    char token[MAXTOKEN];
    int count, length;
    struct subid oid[32];
    char syntax[MAXTOKEN];
    int nexttype, tctype;
    char nexttoken[MAXTOKEN];
    register struct node *np;
    register struct enum_list *ep;

    type = get_token(fp, token,MAXTOKEN);
    if (type != SYNTAX){
	print_error("Bad format for OBJECT TYPE", token, type);
	return 0;
    }
    np = (struct node *)Malloc(sizeof(struct node));
    np->next = 0;
    np->tc_index = -1;
    np->enums = 0;
    np->description = NULL;        /* default to an empty description */
    type = get_token(fp, token,MAXTOKEN);
    if (type == LABEL){
	tctype = get_tc(token, &(np->enums));
#if 0
	if (tctype == LABEL){
	    print_error("No known translation for type", token, type);
	    return 0;
	}
#endif
	type = tctype;
        np->tc_index = get_tc_index(token); /* store TC for later reference */
    }
    np->type = type;
    nexttype = get_token(fp, nexttoken,MAXTOKEN);
    switch(type){
	case SEQUENCE:
	    strcpy(syntax, token);
	    if (nexttype == OF){
		strcat(syntax, " ");
		strcat(syntax, nexttoken);
		nexttype = get_token(fp, nexttoken,MAXTOKEN);
		strcat(syntax, " ");
		strcat(syntax, nexttoken);
		nexttype = get_token(fp, nexttoken,MAXTOKEN);
	    }
	    break;
	case INTEGER:
	case UINTEGER32:
	    strcpy(syntax, token);
	    if (nexttype == LEFTBRACKET) {
		/* if there is an enumeration list, parse it */
		while((type = get_token(fp, token,MAXTOKEN)) != ENDOFFILE){
		    if (type == RIGHTBRACKET)
			break;
		    if (type == LABEL){
			/* this is an enumerated label */
			if (np->enums == 0){
			    ep = np->enums = (struct enum_list *)
					Malloc(sizeof(struct enum_list));
			} else {
			    ep->next = (struct enum_list *)
					Malloc(sizeof(struct enum_list));
			    ep = ep->next;
			}
			ep->next = 0;
			/* a reasonable approximation for the length */
			ep->label =
			    (char *)Malloc((unsigned)strlen(token) + 1);
			strcpy(ep->label, token);
			type = get_token(fp, token,MAXTOKEN);
			if (type != LEFTPAREN){
			    print_error("Expected \"(\"", token, type);
			    free_node(np);
			    return 0;
			}
			type = get_token(fp, token,MAXTOKEN);
			if (type != NUMBER){
			    print_error("Expected integer", token, type);
			    free_node(np);
			    return 0;
			}
			ep->value = atoi(token);
			type = get_token(fp, token,MAXTOKEN);
			if (type != RIGHTPAREN){
			    print_error("Expected \")\"", token, type);
			    free_node(np);
			    return 0;
			}
		    }
		}
		if (type == ENDOFFILE){
		    print_error("Expected \"}\"", token, type);
		    free_node(np);
		    return 0;
		}
		nexttype = get_token(fp, nexttoken,MAXTOKEN);
	    } else if (nexttype == LEFTPAREN){
		/* ignore the "constrained integer" for now */
		nexttype = get_token(fp, nexttoken,MAXTOKEN);
		if (nexttype == SIZE)
		{
		    /* LEFTPAREN */
		    nexttype = get_token(fp, nexttoken,MAXTOKEN);
		    /* Range */
		    nexttype = get_token(fp, nexttoken,MAXTOKEN);
		    /* RIGHTPAREN */
		    nexttype = get_token(fp, nexttoken,MAXTOKEN);
		}
		/* RIGHTPAREN */
		nexttype = get_token(fp, nexttoken,MAXTOKEN);
		nexttype = get_token(fp, nexttoken,MAXTOKEN);
	    }
	    break;
	case BITSTRING:
	    strcpy(syntax, token);
	    if (nexttype == LEFTBRACKET) {
		/* if there is an enumeration list, parse it */
		while((type = get_token(fp, token,MAXTOKEN)) != ENDOFFILE){
		    if (type == RIGHTBRACKET)
			break;
		    if (type == LABEL){
			/* this is an enumerated label */
			if (np->enums == 0){
			    ep = np->enums = (struct enum_list *)
					Malloc(sizeof(struct enum_list));
			} else {
			    ep->next = (struct enum_list *)
					Malloc(sizeof(struct enum_list));
			    ep = ep->next;
			}
			ep->next = 0;
			/* a reasonable approximation for the length */
			ep->label =
			    (char *)Malloc((unsigned)strlen(token) + 1);
			strcpy(ep->label, token);
			type = get_token(fp, token,MAXTOKEN);
			if (type != LEFTPAREN){
			    print_error("Expected \"(\"", token, type);
			    free_node(np);
			    return 0;
			}
			type = get_token(fp, token,MAXTOKEN);
			if (type != NUMBER){
			    print_error("Expected integer", token, type);
			    free_node(np);
			    return 0;
			}
			ep->value = atoi(token);
			type = get_token(fp, token,MAXTOKEN);
			if (type != RIGHTPAREN){
			    print_error("Expected \")\"", token, type);
			    free_node(np);
			    return 0;
			}
		    }
		}
		if (type == ENDOFFILE){
		    print_error("Expected \"}\"", token, type);
		    free_node(np);
		    return 0;
		}
		nexttype = get_token(fp, nexttoken,MAXTOKEN);
	    } else if (nexttype == LEFTPAREN){
		/* ignore the "constrained integer" for now */
		nexttype = get_token(fp, nexttoken,MAXTOKEN);
		nexttype = get_token(fp, nexttoken,MAXTOKEN);
		nexttype = get_token(fp, nexttoken,MAXTOKEN);
	    }
	    break;
	case OCTETSTR:
	    strcpy(syntax, token);
            /* ignore the "constrained octet string" for now */
            if (nexttype == LEFTPAREN) {
                nexttype = get_token(fp, nexttoken,MAXTOKEN);
                if (nexttype == SIZE) {
                    nexttype = get_token(fp, nexttoken,MAXTOKEN);
                    if (nexttype == LEFTPAREN) {
                        nexttype = get_token(fp, nexttoken,MAXTOKEN); /* 0..255 */
                        nexttype = get_token(fp, nexttoken,MAXTOKEN); /* ) */
                        nexttype = get_token(fp, nexttoken,MAXTOKEN); /* ) */
                        if (nexttype == RIGHTPAREN)
                        {
                            nexttype = get_token(fp, nexttoken,MAXTOKEN);
                            break;
                        }
                    }
                }
                print_error("Bad syntax", token, type);
                free_node(np);
                return 0;
            }
	    break;
	case OBJID:
	case NETADDR:
	case IPADDR:
	case COUNTER:
	case GAUGE:
	case TIMETICKS:
	case OPAQUE:
	case NUL:
	case LABEL:
	case NSAPADDRESS:
	case COUNTER64:
	    strcpy(syntax, token);
	    break;
	default:
	    print_error("Bad syntax", token, type);
	    free_node(np);
	    return 0;
    }
    if (nexttype == UNITS){
	type = get_token(fp, quoted_string_buffer,MAXQUOTESTR);
	if (type != QUOTESTRING) {
	    print_error("Bad DESCRIPTION", quoted_string_buffer, type);
	    free_node(np);
	    return 0;
	}
	nexttype = get_token(fp, nexttoken,MAXTOKEN);
    }
    if (nexttype != ACCESS){
	print_error("Should be ACCESS", nexttoken, nexttype);
	free_node(np);
	return 0;
    }
    type = get_token(fp, token,MAXTOKEN);
    if (type != READONLY && type != READWRITE && type != WRITEONLY
	&& type != NOACCESS && type != READCREATE){
	print_error("Bad access type", nexttoken, nexttype);
	free_node(np);
	return 0;
    }
    type = get_token(fp, token,MAXTOKEN);
    if (type != STATUS){
	print_error("Should be STATUS", token, nexttype);
	free_node(np);
	return 0;
    }
    type = get_token(fp, token,MAXTOKEN);
    if (type != MANDATORY && type != CURRENT && type != OPTIONAL && type != OBSOLETE && type != DEPRECATED){
	print_error("Bad status", token, type);
	free_node(np);
	return 0;
    }
    /*
     * Optional parts of the OBJECT-TYPE macro
     */
    type = get_token(fp, token,MAXTOKEN);
    while (type != EQUALS) {
      switch (type) {
        case DESCRIPTION:
          type = get_token(fp, quoted_string_buffer, MAXQUOTESTR);
          if (type != QUOTESTRING) {
              print_error("Bad DESCRIPTION", quoted_string_buffer, type);
              free_node(np);
              return 0;
          }
#ifdef TEST
printf("Description== \"%.50s\"\n", quoted_string_buffer);
#endif
          if (save_mib_descriptions) {
	      np->description = quoted_string_buffer;
	      quoted_string_buffer = (char *)malloc(MAXQUOTESTR);
	  }
          break;

	case REFERENCE:
	  type = get_token(fp, quoted_string_buffer, MAXQUOTESTR);
	  if (type != QUOTESTRING) {
	      print_error("Bad DESCRIPTION", quoted_string_buffer, type);
	      free_node(np);
	      return 0;
	  }
	  break;
        case INDEX:
        case DEFVAL:
	case AUGMENTS:
	case NUM_ENTRIES:
          if (tossObjectIdentifier(fp) != OBJID) {
              print_error("Bad Object Identifier", token, type);
              free_node(np);
              return 0;
          }
          break;

        default:
          print_error("Bad format of optional clauses", token,type);
          free_node(np);
          return 0;

      }
      type = get_token(fp, token,MAXTOKEN);
    }
    if (type != EQUALS){
	print_error("Bad format", token, type);
	free_node(np);
	return 0;
    }
    length = getoid(fp, oid, 32);
    if (length > 1 && length <= 32){
	/* just take the last pair in the oid list */
	if (oid[length - 2].label)
	    strncpy(np->parent, oid[length - 2].label, MAXLABEL);
	strcpy(np->label, name);
	if (oid[length - 1].subid != -1)
	    np->subid = oid[length - 1].subid;
	else
	    print_error("Warning: This entry is pretty silly", np->label, type);
    } else {
	print_error("No end to oid", (char *)NULL, type);
	free_node(np);
	np = 0;
    }
    /* free oid array */
    for(count = 0; count < length; count++){
	if (oid[count].label)
	    free(oid[count].label);
	oid[count].label = 0;
    }
    return np;
}


/*
 * Parses an OBJECT GROUP macro.
 * Returns 0 on error.
 */
static struct node *
parse_objectgroup(fp, name)
    register FILE *fp;
    char *name;
{
    register int type;
    char token[MAXTOKEN];
    int count, length;
    struct subid oid[32];
    char syntax[MAXTOKEN];
    int nexttype, tctype;
    char nexttoken[MAXTOKEN];
    register struct node *np;
    register struct enum_list *ep;

    np = (struct node *)Malloc(sizeof(struct node));
    np->tc_index = -1;
    np->type = 0;
    np->next = 0;
    np->enums = 0;
    np->description = NULL;        /* default to an empty description */
    type = get_token(fp, token,MAXTOKEN);
    while (type != EQUALS) {
      switch (type) {
        case DESCRIPTION:
          type = get_token(fp, quoted_string_buffer, MAXQUOTESTR);
          if (type != QUOTESTRING) {
              print_error("Bad DESCRIPTION", quoted_string_buffer, type);
              free_node(np);
              return 0;
          }
#ifdef TEST
printf("Description== \"%.50s\"\n", quoted_string_buffer);
#endif
          if (save_mib_descriptions) {
	      np->description = quoted_string_buffer;
	      quoted_string_buffer = (char *)malloc(MAXQUOTESTR);
          }
          break;

        default:
	  /* NOTHING */
	  break;
      }
      type = get_token(fp, token,MAXTOKEN);
    }
    length = getoid(fp, oid, 32);
    if (length > 1 && length <= 32){
	/* just take the last pair in the oid list */
	if (oid[length - 2].label)
	    strncpy(np->parent, oid[length - 2].label, MAXLABEL);
	strcpy(np->label, name);
	if (oid[length - 1].subid != -1)
	    np->subid = oid[length - 1].subid;
	else
	    print_error("Warning: This entry is pretty silly", np->label, type);
    } else {
	print_error("No end to oid", (char *)NULL, type);
	free_node(np);
	np = 0;
    }
    /* free oid array */
    for(count = 0; count < length; count++){
	if (oid[count].label)
	    free(oid[count].label);
	oid[count].label = 0;
    }
    return np;
}

/*
 * Parses a NOTIFICATION-TYPE macro.
 * Returns 0 on error.
 */
static struct node *
parse_notificationDefinition(fp, name)
    register FILE *fp;
    char *name;
{
    register int type;
    char token[MAXTOKEN];
    int count, length;
    struct subid oid[32];
    char syntax[MAXTOKEN];
    int nexttype, tctype;
    char nexttoken[MAXTOKEN];
    register struct node *np;
    register struct enum_list *ep;

    np = (struct node *)Malloc(sizeof(struct node));
    np->tc_index = -1;
    np->type = 0;
    np->next = 0;
    np->enums = 0;
    np->description = NULL;        /* default to an empty description */
    type = get_token(fp, token,MAXTOKEN);
    while (type != EQUALS) {
      switch (type) {
        case DESCRIPTION:
          type = get_token(fp, quoted_string_buffer, MAXQUOTESTR);
          if (type != QUOTESTRING) {
              print_error("Bad DESCRIPTION", quoted_string_buffer, type);
              free_node(np);
              return 0;
          }
#ifdef TEST
printf("Description== \"%.50s\"\n", quoted_string_buffer);
#endif
          if (save_mib_descriptions) {
	      np->description = quoted_string_buffer;
	      quoted_string_buffer = (char *)malloc(MAXQUOTESTR);
          }
          break;

        default:
	  /* NOTHING */
	  break;
      }
      type = get_token(fp, token,MAXTOKEN);
    }
    length = getoid(fp, oid, 32);
    if (length > 1 && length <= 32){
	/* just take the last pair in the oid list */
	if (oid[length - 2].label)
	    strncpy(np->parent, oid[length - 2].label, MAXLABEL);
	strcpy(np->label, name);
	if (oid[length - 1].subid != -1)
	    np->subid = oid[length - 1].subid;
	else
	    print_error("Warning: This entry is pretty silly", np->label, type);
    } else {
	print_error("No end to oid", (char *)NULL, type);
	free_node(np);
	np = 0;
    }
    /* free oid array */
    for(count = 0; count < length; count++){
	if (oid[count].label)
	    free(oid[count].label);
	oid[count].label = 0;
    }
    return np;
}

/*
 * Parses a TRAP-TYPE macro.
 * Returns 0 on error.
 */
static struct node *
parse_trapDefinition(fp, name)
    register FILE *fp;
    char *name;
{
    register int type;
    char token[MAXTOKEN];
    int count, length;
    struct subid oid[32];
    char syntax[MAXTOKEN];
    int nexttype, tctype;
    char nexttoken[MAXTOKEN];
    register struct node *np;
    register struct enum_list *ep;

    np = (struct node *)Malloc(sizeof(struct node));
    np->tc_index = -1;
    np->type = 0;
    np->next = 0;
    np->enums = 0;
    np->description = NULL;        /* default to an empty description */
    type = get_token(fp, token, MAXTOKEN);
    while (type != EQUALS) {
	switch (type) {
	    case DESCRIPTION:
		type = get_token(fp, quoted_string_buffer, MAXQUOTESTR);
		if (type != QUOTESTRING) {
		    print_error("Bad DESCRIPTION", quoted_string_buffer, type);
		    free_node(np);
		    return 0;
		}

#ifdef TEST
                printf("Description== \"%.50s\"\n", quoted_string_buffer);
#endif
                if (save_mib_descriptions) {
		    np->description = quoted_string_buffer;
		    quoted_string_buffer = (char *)malloc(MAXQUOTESTR);
                }
		break;
	    case ENTERPRISE:
		type = get_token(fp, quoted_string_buffer, MAXQUOTESTR);
		if (type == LEFTBRACKET)
		{
		    type = get_token(fp, quoted_string_buffer, MAXQUOTESTR);
		    if (type != LABEL)
		    {
			print_error("Bad Trap Format", 
				    quoted_string_buffer, type);
			free_node(np);
			return 0;
		    }
		    strcpy(np->parent, quoted_string_buffer);
		    /* Get right bracket */
		    type = get_token(fp, quoted_string_buffer, MAXQUOTESTR);
		}
		else if (type == LABEL)
		{
		    strcpy(np->parent, quoted_string_buffer);
		}
		
		break;
	    default:
		/* NOTHING */
		break;
	}
	type = get_token(fp, token, MAXTOKEN);
    }
    type = get_token(fp, token, MAXTOKEN);

    strcpy(np->label, name);
    
    if (type != NUMBER)
    {
	print_error("Expected a Number", token, type);
	free_node(np);
	np = 0;
	
    }
    else
    {
	np->subid = atoi(token);
    }

    return np;
}


/*
 * Parses a compliance macro
 * Returns 0 on error.
 */
static struct node *
parse_compliance(fp, name)
    register FILE *fp;
    char *name;
{
    register int type;
    char token[MAXTOKEN];
    int count, length;
    struct subid oid[32];
    char syntax[MAXTOKEN];
    int nexttype, tctype;
    char nexttoken[MAXTOKEN];
    register struct node *np;
    register struct enum_list *ep;

    np = (struct node *)Malloc(sizeof(struct node));
    np->tc_index = -1;
    np->type = 0;
    np->next = 0;
    np->enums = 0;
    np->description = NULL;        /* default to an empty description */
    type = get_token(fp, token,MAXTOKEN);
    while (type != EQUALS) 
      type = get_token(fp, quoted_string_buffer,MAXQUOTESTR);
    length = getoid(fp, oid, 32);
    if (length > 1 && length <= 32){
	/* just take the last pair in the oid list */
	if (oid[length - 2].label)
	    strncpy(np->parent, oid[length - 2].label, MAXLABEL);
	strcpy(np->label, name);
	if (oid[length - 1].subid != -1)
	    np->subid = oid[length - 1].subid;
	else
	    print_error("Warning: This entry is pretty silly", np->label, type);
    } else {
	print_error("No end to oid", (char *)NULL, type);
	free_node(np);
	np = 0;
    }
    /* free oid array */
    for(count = 0; count < length; count++){
	if (oid[count].label)
	    free(oid[count].label);
	oid[count].label = 0;
    }
    return np;
}



/*
 * Parses a module identity macro
 * Returns 0 on error.
 */
static struct node *
parse_moduleIdentity(fp, name)
    register FILE *fp;
    char *name;
{
    register int type;
    char token[MAXTOKEN];
    int count, length;
    struct subid oid[32];
    char syntax[MAXTOKEN];
    int nexttype, tctype;
    char nexttoken[MAXTOKEN];
    register struct node *np;
    register struct enum_list *ep;

    np = (struct node *)Malloc(sizeof(struct node));
    np->tc_index = -1;
    np->type = 0;
    np->next = 0;
    np->enums = 0;
    np->description = NULL;        /* default to an empty description */
    type = get_token(fp, token,MAXTOKEN);
    while (type != EQUALS) {
	type = get_token(fp, token,MAXTOKEN);
    }
    length = getoid(fp, oid, 32);
    if (length > 1 && length <= 32){
	/* just take the last pair in the oid list */
	if (oid[length - 2].label)
	    strncpy(np->parent, oid[length - 2].label, MAXLABEL);
	strcpy(np->label, name);
	if (oid[length - 1].subid != -1)
	    np->subid = oid[length - 1].subid;
	else
	    print_error("Warning: This entry is pretty silly", np->label, type);
    } else {
	print_error("No end to oid", (char *)NULL, type);
	free_node(np);
	np = 0;
    }
    /* free oid array */
    for(count = 0; count < length; count++){
	if (oid[count].label)
	    free(oid[count].label);
	oid[count].label = 0;
    }
    return np;
}

int parse_mib_header(fp, name)
    register FILE *fp;
    char *name;
{
    int type = DEFINITIONS;
    char token[MAXQUOTESTR];
    
    /* This probably isn't good enough.  If there is no
       imports clause we can't go around waiting (forever) for a semicolon.
       We need to check for semi following an EXPORTS clause or an IMPORTS
       clause of both.  Look for BEGIN; in my initial MIBs to see those
       that I needed to hack to get to parse because they didn't have
       an IMPORTS or and EXPORTS clause.
       */
    while(type != BEGIN) {
      type = get_token(fp, token, MAXQUOTESTR);
    }

    if (type == IMPORTS) {
      while(1) {
        type = get_token(fp, token, MAXQUOTESTR);
        if (type == SEMI)
          break;
      }
      type = get_token(fp, token, MAXQUOTESTR);
    }


    if (type == EXPORTS) {
      while(1) {
        type = get_token(fp, token, MAXQUOTESTR);
        if (type == SEMI)
          break;
      }
      type = get_token(fp, token, MAXQUOTESTR);
    }
    return 1;
}



/*
 * Parses a mib file and returns a linked list of nodes found in the file.
 * Returns NULL on error.
 */
static struct node *
parse(fp, root)
    FILE *fp;
    struct node *root;
{
    char token[MAXTOKEN];
    char name[MAXTOKEN];
    int	type = 1;
    int lasttype;
    
#define BETWEEN_MIBS  	      1
#define IN_MIB                2
    int state = BETWEEN_MIBS;
    struct node *np;

    np = root;
    if (np != NULL) {
      /* now find end of chain */
      while(np->next)
        np = np->next;
    } else {
      hash_init();
      quoted_string_buffer = (char *)malloc(MAXQUOTESTR);  /* free this later */
      memset(tclist, NULL, 64 * sizeof(struct tc));
    }
    
    while(type != ENDOFFILE){
	type = lasttype = get_token(fp, token,MAXTOKEN);
skipget:
	if (type == END){
	    if (state != IN_MIB){
		print_error("Error, end before start of MIB.", (char *)NULL, type);
		return NULL;
	    }
	    state = BETWEEN_MIBS;
	    continue;
	}
	else if (type == IMPORTS)
	{
	    while (type != SEMI)
		type = get_token(fp, token, MAXTOKEN);

	    type = get_token(fp, token, MAXTOKEN);
	} 
	else if (type == EXPORTS)
	{
	    while (type != SEMI)
		type = get_token(fp, token, MAXTOKEN);

	    type = get_token(fp, token, MAXTOKEN);
	} 
	else if (type != LABEL){
	    if (type == ENDOFFILE){
		return root;
	    }
	    print_error(token, "is a reserved word", type);
	    return NULL;
	}
	strncpy(name, token, MAXTOKEN);
	type = get_token(fp, token,MAXTOKEN);
	if (type == DEFINITIONS){
	    if (state != BETWEEN_MIBS){
		print_error("Error, nested MIBS.", (char *)NULL, type);
		return NULL;
	    }
	    state = IN_MIB;
	    if (!parse_mib_header(fp, name)){
		print_error("Bad parse of module header", (char *)NULL, type);
		return NULL;
	    }
       } else if (type == OBJTYPE){
	    if (root == NULL){
		/* first link in chain */
		np = root = parse_objecttype(fp, name);
		if (np == NULL){
		    print_error("Bad parse of object type", (char *)NULL,
				type);
		    return NULL;
		}
	    } else {
		np->next = parse_objecttype(fp, name);
		if (np->next == NULL){
		    print_error("Bad parse of objecttype", (char *)NULL,
				type);
		    return NULL;
		}
	    }
	    /* now find end of chain */
	    while(np->next)
		np = np->next;
	} else if (type == OBJGROUP){
	    if (root == NULL){
		/* first link in chain */
		np = root = parse_objectgroup(fp, name);
		if (np == NULL){
		    print_error("Bad parse of object group", (char *)NULL,
				type);
		    return NULL;
		}
	    } else {
		np->next = parse_objectgroup(fp, name);
		if (np->next == NULL){
		    print_error("Bad parse of objectgroup", (char *)NULL,
				type);
		    return NULL;
		}
	    }
	    /* now find end of chain */
	    while(np->next)
		np = np->next;
	} else if (type == TRAPTYPE){
	    if (root == NULL){
		/* first link in chain */
		np = root = parse_trapDefinition(fp, name);
		if (np == NULL){
		    print_error("Bad parse of trap definition",
				(char *)NULL, type);
		    return NULL;
		}
	    } else {
		np->next = parse_trapDefinition(fp, name);
		if (np->next == NULL){
		    print_error("Bad parse of trap definition",
				(char *)NULL, type);
		    return NULL;
		}
	    }
	    /* now find end of chain */
	    while(np->next)
		np = np->next;
	} else if (type == NOTIFTYPE){
	    if (root == NULL){
		/* first link in chain */
		np = root = parse_notificationDefinition(fp, name);
		if (np == NULL){
		    print_error("Bad parse of notification definition",
				(char *)NULL, type);
		    return NULL;
		}
	    } else {
		np->next = parse_notificationDefinition(fp, name);
		if (np->next == NULL){
		    print_error("Bad parse of notification definition",
				(char *)NULL, type);
		    return NULL;
		}
	    }
	    /* now find end of chain */
	    while(np->next)
		np = np->next;
	} else if (type == COMPLIANCE){
	    if (root == NULL){
		/* first link in chain */
		np = root = parse_compliance(fp, name);
		if (np == NULL){
		    print_error("Bad parse of module compliance", (char *)NULL,
				type);
		    return NULL;
		}
	    } else {
		np->next = parse_compliance(fp, name);
		if (np->next == NULL){
		    print_error("Bad parse of module compliance", (char *)NULL,
				type);
		    return NULL;
		}
	    }
	    /* now find end of chain */
	    while(np->next)
		np = np->next;
	} else if (type == MODULEIDENTITY){
	    if (root == NULL){
		/* first link in chain */
		np = root = parse_moduleIdentity(fp, name);
		if (np == NULL){
		    print_error("Bad parse of module identity", (char *)NULL,
				type);
		    return NULL;
		}
	    } else {
		np->next = parse_moduleIdentity(fp, name);
		if (np->next == NULL){
		    print_error("Bad parse of module identity", (char *)NULL,
				type);
		    return NULL;
		}
	    }
	    /* now find end of chain */
	    while(np->next)
		np = np->next;
	} else if (type == OBJID){
	    if (root == NULL){
		/* first link in chain */
		np = root = parse_objectid(fp, name);
		if (np == NULL){
		    print_error("Bad parse of object id", (char *)NULL, type);
		    return NULL;
		}
	    } else {
		np->next = parse_objectid(fp, name);
		if (np->next == NULL){
		    print_error("Bad parse of object type", (char *)NULL,
				type);
		
		    return NULL;
		}
	    }
	    /* now find end of chain */
	    while(np->next)
		np = np->next;
	} else if (type == EQUALS){
	    if (!parse_asntype(fp, name, &type, token)){
		print_error("Bad parse of ASN type definition.", NULL, EQUALS);
		return NULL;
	    }
	    goto skipget;
	} else if (type == ENDOFFILE){
	    break;
 	} else if (lasttype == LABEL) {
 	    while (type != RIGHTBRACKET)
 	    {
 		type = get_token(fp, token, MAXTOKEN);
 	    }
 	    type = lasttype;
	    
 	    goto skipget;
	} else {
	    print_error("Bad operator", (char *)NULL, type);
	    return NULL;
	}
    }
#ifdef TEST
{
    struct enum_list *ep;
    
    for(np = root; np; np = np->next){
	printf("%s ::= { %s %d } (%d)\n", np->label, np->parent, np->subid,
		np->type);
        if (np->tc_index >= 0)
            printf("TC = %s\n",tclist[np->tc_index].descriptor);
	if (np->enums){
	    printf("Enums: \n");
	    for(ep = np->enums; ep; ep = ep->next){
		printf("%s(%d)\n", ep->label, ep->value);
	    }
	}
    }
}
#endif /* TEST */
    return root;
}

/*
 * Parses a token from the file.  The type of the token parsed is returned,
 * and the text is placed in the string pointed to by token.
 */
static int
get_token(fp, token,maxtlen)
    register FILE *fp;
    register char *token;
    int maxtlen;
{
    static char last = ' ';
    register int ch;
    register char *cp = token;
    register int hash = 0;
    register struct tok *tp;
    
    *cp = 0;
    ch = last;
    /* skip all white space */
    while(isspace(ch) && ch != -1){
	ch = getc(fp);
	if (ch == '\n')
	    Line++;
    }
    if (ch == -1) {
	return ENDOFFILE;
    } else if (ch == '"') {
	return parseQuoteString(fp, token, maxtlen);
    }

    /*
     * Accumulate characters until end of token is found.  Then attempt to
     * match this token as a reserved word.  If a match is found, return the
     * type.  Else it is a label.
     */
    do {
	if (ch == '\n')
	    Line++;
	if (isspace(ch) || ch == '(' || ch == ')' || ch == '{' || ch == '}' ||
	    ch == ',' || ch == ';'){
	    if (!isspace(ch) && *token == 0){
		hash += ch;
		if (cp-token < maxtlen-1)
                  *cp++ = ch;
		last = ' ';
	    } else {
		last = ch;
	    }
	    *cp = '\0';

	    for (tp = buckets[BUCKET(hash)]; tp; tp = tp->next) {
		if ((tp->hash == hash) && (strcmp(tp->name, token) == 0))
			break;
	    }
	    if (tp){
		if (tp->token == CONTINUE)
		    continue;
		return (tp->token);
	    }

	    if (token[0] == '-' && token[1] == '-'){
		/* strip comment */
		if (ch != '\n'){
		    while ((ch = getc(fp)) != -1)
			if (ch == '\n'){
			    Line++;
			    break;
			}
		}
		if (ch == -1)
		    return ENDOFFILE;
		last = ch;
		return get_token(fp, token,maxtlen);		
	    }
	    for(cp = token; *cp; cp++)
		if (!isdigit(*cp))
		    return LABEL;
	    return NUMBER;
	} else {
	    hash += ch;
	    if (cp-token < maxtlen)
              *cp++ = ch;
	    if (ch == '\n')
		Line++;
	}
    
    } while ((ch = getc(fp)) != -1);
    return ENDOFFILE;
}

struct tree *
read_mib(filename)
    char *filename;
{
    FILE *fp;
    struct node *nodes = 0;
    struct tree *tree;
    DIR *dir, *dir2;
    struct dirent *file;
    char tmpstr[300];

    fp = fopen(filename, "r");
    if (fp == NULL)
	return NULL;
    nodes = parse(fp, nodes);
    if (!nodes){
	fprintf(stderr, "Mib table is bad.  Exiting\n");
	exit(1);
    }
    fclose(fp);

    sprintf(tmpstr,"%s/mibs",SNMPLIBPATH);
    if (nodes != NULL && (dir = opendir(tmpstr))) {
      while (nodes != NULL && (file = readdir(dir))) {
        /* Only parse file names not beginning with a '.' */
        if (file->d_name != NULL && file->d_name[0] != '.') {
          sprintf(tmpstr,"%s/mibs/%s",SNMPLIBPATH,file->d_name);
          if (dir2 = opendir(tmpstr)) {
            /* file is a directory, don't read it */
            closedir(dir2);
          } else {
            /* parse it */
            DEBUGP1("Parsing mib file:  %s... ",tmpstr);
            if ((fp = fopen(tmpstr, "r")) == NULL) {
              perror("fopen");
              exit(1);
            }
            nodes = parse(fp, nodes);
            if (nodes == NULL) {
              fprintf(stderr, "Mib table is bad.  Exiting\n");
              exit(1);
            }
            DEBUGP("done\n");
          }
        }
      }
      closedir(dir);
    }
    tree = build_tree(nodes);
    return tree;
}


#ifdef TEST
main(argc, argv)
    int argc;
    char *argv[];
{
    FILE *fp;
    struct node *nodes;
    struct tree *tp = NULL;
    

    fp = fopen("mib.txt", "r");
    if (fp == NULL){
	fprintf(stderr, "open failed\n");
	return 1;
    }
    nodes = parse(fp);
    if (nodes != NULL)
    {
	tp = build_tree(nodes);
	print_subtree(tp, 0);
    }

    free_tree(tp);
    
    fclose(fp);
}

#endif /* TEST */

static int
parseQuoteString(fp, token,maxtlen)
    register FILE *fp;
    register char *token;
    int maxtlen;
{
    register int ch;
    int eat_space = 0, count = 0;
    
    ch = getc(fp);
    while(ch != -1) {
      if (ch == '\n') {
        Line++;
      }
      else if (ch == '"') {
        *token = '\0';
        return QUOTESTRING;
      }
      /* maximum description length check.  If greater, keep parsing
         but truncate the string */
      if (count++ < maxtlen-1)
	*token++ = ch;
      ch = getc(fp);
    }

    return 0;
}

/*
 * This routine parses a string like  { blah blah blah } and returns OBJID if
 * it is well formed, and NULL if not.
 */
static int
tossObjectIdentifier(fp)
    register FILE *fp;
{
    int type;
    char token[MAXTOKEN];
    
    type = get_token(fp, token, MAXTOKEN);
    
    if (type != LEFTBRACKET)
	return 0;
    while (type != RIGHTBRACKET && type != ENDOFFILE)
    {
	type = get_token(fp, token, MAXTOKEN);
    }
    
    if (type == RIGHTBRACKET)
	return OBJID;
    else
	return 0;
}
