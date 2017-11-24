/*
 * Tree search generalized from Knuth (6.2.2) Algorithm T just like
 * the AT&T man page says.
 *
 * The node_t structure is for internal use only, lint doesn't grok it.
 *
 * Written by reading the System V Interface Definition, not the code.
 *
 * Totally public domain.
 */

#include "tsearch.h"
#include <stdlib.h>


typedef	struct node
{
  const void   *key;
  struct node  *llink;
  struct node  *rlink;
} node_t;


/*	$NetBSD: tsearch.c,v 1.5 2005/11/29 03:12:00 christos Exp $	*/
/* find or insert datum into search tree */
void *
tsearch (const void *vkey,		/* key to be located */
         void **vrootp,			/* address of tree root */
         int (*compar)(const void *, const void *))
{
  node_t *q;
  node_t **rootp = (node_t **)vrootp;

  if (NULL == rootp)
    return NULL;

  while (*rootp != NULL)
    {	/* Knuth's T1: */
      int r;

      if ((r = (*compar)(vkey, (*rootp)->key)) == 0)	/* T2: */
        return *rootp;		/* we found it! */

      rootp = (r < 0) ?
        &(*rootp)->llink :		/* T3: follow left branch */
        &(*rootp)->rlink;		/* T4: follow right branch */
    }

  q = malloc (sizeof(node_t));		/* T5: key not found */
  if (q)
    {				/* make new node */
      *rootp = q;			/* link new node to old */
      q->key = vkey;		/* initialize new node */
      q->llink = q->rlink = NULL;
    }
  return q;
}


/*	$NetBSD: tfind.c,v 1.5 2005/03/23 08:16:53 kleink Exp $	*/
/* find a node, or return NULL */
void *
tfind (const void *vkey,         /* key to be found */
       void * const *vrootp,     /* address of the tree root */
       int (*compar)(const void *, const void *))
{
  node_t * const *rootp = (node_t * const*)vrootp;

  if (NULL == rootp)
    return NULL;

  while (*rootp != NULL)
    {		/* T1: */
      int r;

      if ((r = (*compar)(vkey, (*rootp)->key)) == 0)	/* T2: */
        return *rootp;		/* key found */
      rootp = (r < 0) ?
        &(*rootp)->llink :		/* T3: follow left branch */
        &(*rootp)->rlink;		/* T4: follow right branch */
    }
  return NULL;
}


/*	$NetBSD: tdelete.c,v 1.2 1999/09/16 11:45:37 lukem Exp $	*/
/*
 * delete node with given key
 *
 * vkey:   key to be deleted
 * vrootp: address of the root of the tree
 * compar: function to carry out node comparisons
 */
void *
tdelete (const void * __restrict vkey,
         void ** __restrict vrootp,
         int (*compar)(const void *, const void *))
{
  node_t **rootp = (node_t **)vrootp;
  node_t *p;
  node_t *q;
  node_t *r;
  int cmp;

  if (rootp == NULL || (p = *rootp) == NULL)
    return NULL;

  while ((cmp = (*compar)(vkey, (*rootp)->key)) != 0)
    {
      p = *rootp;
      rootp = (cmp < 0) ?
        &(*rootp)->llink :		/* follow llink branch */
        &(*rootp)->rlink;		/* follow rlink branch */
      if (*rootp == NULL)
        return NULL;		/* key not found */
    }
  r = (*rootp)->rlink;			/* D1: */
  if ((q = (*rootp)->llink) == NULL)	/* Left NULL? */
    {
      q = r;
    }
  else if (r != NULL)
    {			/* Right link is NULL? */
      if (r->llink == NULL)
        {		/* D2: Find successor */
          r->llink = q;
          q = r;
        }
      else
        {			/* D3: Find NULL link */
          for (q = r->llink; q->llink != NULL; q = r->llink)
            r = q;
          r->llink = q->rlink;
          q->llink = (*rootp)->llink;
          q->rlink = (*rootp)->rlink;
        }
    }
  free(*rootp);				/* D4: Free node */
  *rootp = q;				/* link parent to new node */
  return p;
}

/* end of tsearch.c */
