/*-
 * Written by J.T. Conklin <jtc@netbsd.org>
 * Public domain.
 *
 *	$NetBSD: search.h,v 1.12 1999/02/22 10:34:28 christos Exp $
 * $FreeBSD: release/9.0.0/include/search.h 105250 2002-10-16 14:29:23Z robert $
 */

#ifndef _TSEARCH_H_
#define _TSEARCH_H_

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */


void	*
tdelete (const void * __restrict,
         void ** __restrict,
         int (*)(const void *, const void *));


void	*
tfind (const void *,
       void * const *,
       int (*)(const void *, const void *));


void	*
tsearch (const void *,
         void **,
         int (*)(const void *, const void *));

#if defined(__cplusplus)
};
#endif /* __cplusplus */

#endif /* !_TSEARCH_H_ */
