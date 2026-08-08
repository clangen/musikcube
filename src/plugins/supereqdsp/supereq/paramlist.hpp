/*
    SuperEQ DSP plugin for DeaDBeeF Player
    Copyright (C) 2009-2014 Alexey Yakovenko <waker@users.sourceforge.net>
    Original SuperEQ code (C) Naoki Shibata <shibatch@users.sf.net>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// @file paramlist.hpp
/// @brief Linked-list of equalizer band parameters for SuperEQ.
/// @details Vendored from the DeaDBeeF player. Defines paramlistelm (one
/// frequency band with a lower/upper bound and gain) and paramlist, a
/// singly-linked list of bands sorted by frequency that feeds equ_makeTable.

/** @brief A single equalizer band definition.
 *  @details One element of a paramlist. A band is described by its lower and
 *  upper frequency bounds and the gain applied across it. */
class paramlistelm {
public:
	/** @brief Pointer to the next band in the list. */
	class paramlistelm *next;

	/** @brief Lower/upper frequency bounds, current and target gain. */
	float lower,upper,gain,gain2;
	/** @brief Position used to keep the sort stable. */
	int sortindex;

	/** @brief Constructs an empty band element. */
	paramlistelm(void) {
		lower = upper = gain = 0;
		next = NULL;
	};

	/** @brief Deletes the remaining list. */
	~paramlistelm() {
		delete next;
		next = NULL;
	};
};

/** @brief Singly-linked list of equalizer bands.
 *  @details Bands can be appended, removed, counted and sorted by their lower
 *  frequency bound. The sorted list is passed to the equalizer to rebuild its
 *  filter table. */
class paramlist {
public:
	/** @brief Head of the band list. */
	class paramlistelm *elm;

	/** @brief Constructs an empty parameter list. */
	paramlist(void) {
		elm = NULL;
	}

	/** @brief Deletes the entire list. */
	~paramlist() {
		delete elm;
		elm = NULL;
	}

	/** @brief Replaces this list with a copy of another.
	 *  @param src The source list to copy. */
	void copy(paramlist &src)
	{
		delete elm;
		elm = NULL;

		paramlistelm **p,*q;
		for(p=&elm,q=src.elm;q != NULL;q = q->next,p = &(*p)->next)
		{
			*p = new paramlistelm;
			(*p)->lower = q->lower;
			(*p)->upper = q->upper;
			(*p)->gain  = q->gain;
		}
	}
		
	/** @brief Appends a new empty band to the list.
	 *  @return Pointer to the newly appended element. */
	paramlistelm *newelm(void)
	{
		paramlistelm **e;
		for(e = &elm;*e != NULL;e = &(*e)->next) ;
		*e = new paramlistelm;

		return *e;
	}

	/** @brief Returns the number of bands in the list.
	 *  @return The element count. */
	int getnelm(void)
	{
		int i;
		paramlistelm *e;

		for(e = elm,i = 0;e != NULL;e = e->next,i++) ;

		return i;
	}
	
	/** @brief Removes a band from the list.
	 *  @param p The element to remove. */
	void delelm(paramlistelm *p)
	{
		paramlistelm **e;
		for(e = &elm;*e != NULL && p != *e;e = &(*e)->next) ;
		if (*e == NULL) return;
		*e = (*e)->next;
		p->next = NULL;
		delete p;
	}

	/** @brief Sorts the bands by their lower frequency bound. */
	void sortelm(void)
	{
		int i=0;

		if (elm == NULL) return;

		for(paramlistelm *r = elm;r	!= NULL;r = r->next) r->sortindex = i++;

		paramlistelm **p,**q;

		for(p=&elm->next;*p != NULL;)
		{
			for(q=&elm;*q != *p;q = &(*q)->next)
				if ((*p)->lower < (*q)->lower ||
					((*p)->lower == (*q)->lower && (*p)->sortindex < (*q)->sortindex)) break;

			if (p == q) {p = &(*p)->next; continue;}

			paramlistelm **pn = p;
			paramlistelm *pp = *p;
			*p = (*p)->next;
			pp->next = *q;
			*q = pp;

			p = pn;
	    }
	}
};

