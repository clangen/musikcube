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

class paramlistelm {
public:
	class paramlistelm *next;

	float lower,upper,gain,gain2;
	int sortindex;

	paramlistelm(void) {
		lower = upper = gain = 0;
		next = NULL;
	};

	~paramlistelm() {
		delete next;
		next = NULL;
	};
};

class paramlist {
public:
	class paramlistelm *elm;

	paramlist(void) {
		elm = NULL;
	}

	~paramlist() {
		delete elm;
		elm = NULL;
	}

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
		
	paramlistelm *newelm(void)
	{
		paramlistelm **e;
		for(e = &elm;*e != NULL;e = &(*e)->next) ;
		*e = new paramlistelm;

		return *e;
	}

	int getnelm(void)
	{
		int i;
		paramlistelm *e;

		for(e = elm,i = 0;e != NULL;e = e->next,i++) ;

		return i;
	}
	
	void delelm(paramlistelm *p)
	{
		paramlistelm **e;
		for(e = &elm;*e != NULL && p != *e;e = &(*e)->next) ;
		if (*e == NULL) return;
		*e = (*e)->next;
		p->next = NULL;
		delete p;
	}

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

