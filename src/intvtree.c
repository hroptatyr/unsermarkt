/*** intvtree.c -- interval trees based on red-black trees
 *
 * Copyright (C) 2009 Sebastian Freundt
 *
 * Author:  Sebastian Freundt <sebastian.freundt@ga-group.nl>
 *
 * This file is part of unserding.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the author nor the names of any contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ***/

#if defined HAVE_CONFIG_H
# include "config.h"
#endif	/* HAVE_CONFIG_H */
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "unserding-nifty.h"
#include "intvtree.h"


struct it_node_s {
	/* book-keeping */
	int key;
	int high;
	int max_high;
	bool redp;

	/* tree navigation */
	it_node_t left;
	it_node_t right;
	it_node_t parent;

	/* satellite data */
	void *data;
};


static inline int
max(int a, int b)
{
	return a > b ? a : b;
}


/* nodes, ctor */
static it_node_t
make_node(void)
{
	it_node_t n = xnew(struct it_node_s);
	memset(n, 0, sizeof(*n));
	return n;
}

static void
free_node(it_node_t in)
{
	memset(in, 0, sizeof(*in));
	free(in);
	return;
}


/* ctor */
itree_t
make_itree(void)
{
	itree_t res = xnew(struct itree_s);

	res->nil = make_node();
	res->nil->left = res->nil->right = res->nil->parent = res->nil;
	res->nil->redp = false;
	res->nil->key = res->nil->high = res->nil->max_high = INT_MIN;

	res->root = make_node();
	res->root->parent = res->root->left = res->root->right = res->nil;
	res->root->key = res->root->high = res->root->max_high = INT_MAX;
	res->root->redp = false;
	return res;
}

void
free_itree(itree_t it)
{
	it_node_t x = it->root->left;

	if (x != it->nil) {
#if 0
/* implement me */
		if (x->left != it->nil) {
			stuffToFree.Push(x->left);
		}
		if (x->right != nil) {
			stuffToFree.Push(x->right);
		}
		free_node(x);
		while (stuffToFree.NotEmpty()) {
			x = stuffToFree.Pop();
			if (x->left != it->nil) {
				stuffToFree.Push(x->left);
			}
			if (x->right != it->nil) {
				stuffToFree.Push(x->right);
			}
			free_node(x);
		}
#endif
	}
	free_node(it->nil);
	free_node(it->root);
	return;
}

/* opers */
static void
itree_rot_left(itree_t it, it_node_t p)
{
	it_node_t y = p->right;

	p->right = y->left;

	if (y->left != it->nil) {
		y->left->parent = p;
	}
	y->parent = p->parent;

	if (p == p->parent->left) {
		p->parent->left = y;
	} else {
		p->parent->right = y;
	}
	y->left = p;
	p->parent = y;

	p->max_high = max(p->left->max_high, max(p->right->max_high, p->high));
	y->max_high = max(p->max_high, max(y->right->max_high, y->high));
	return;
}

static void
itree_rot_right(itree_t it, it_node_t p)
{
	it_node_t x = p->left;

	p->left = x->right;

	if (it->nil != x->right) {
		x->right->parent = p;
	}
	x->parent = p->parent;

	if (p == p->parent->left) {
		p->parent->left = x;
	} else {
		p->parent->right = x;
	}

	x->right = p;
	p->parent = x;

	p->max_high = max(p->left->max_high, max(p->right->max_high, p->high));
	x->max_high = max(x->left->max_high, max(p->max_high, x->high));
	return;
}

static void
itree_ins_help(itree_t it, it_node_t z)
{
	/*  This function should only be called by InsertITTree (see above) */
	it_node_t x, y;
    
	z->left = z->right = it->nil;
	y = it->root;
	x = it->root->left;
	while (x != it->nil) {
		y = x;
		if (x->key > z->key) { 
			x = x->left;
		} else {
			x = x->right;
		}
	}
	z->parent = y;
	if ((y == it->root) || (y->key > z->key)) { 
		y->left = z;
	} else {
		y->right = z;
	}
	return;
}

static void
itree_fixup_max_high(itree_t it, it_node_t x)
{
	while (x != it->root) {
		x->max_high = max(
			x->high, max(x->left->max_high, x->right->max_high));
		x = x->parent;
	}
	return;
}

it_node_t
itree_add(itree_t it, void *data)
{
	it_node_t x, y, res;

	res = x = make_node();
	itree_ins_help(it, x);
	itree_fixup_max_high(it, x->parent);
	x->redp = true;
	while (x->parent->redp) {
		/* use sentinel instead of checking for root */
		if (x->parent == x->parent->parent->left) {
			y = x->parent->parent->right;
			if (y->redp) {
				x->parent->redp = false;
				y->redp = false;
				x->parent->parent->redp = true;
				x = x->parent->parent;
			} else {
				if (x == x->parent->right) {
					x = x->parent;
					itree_rot_left(it, x);
				}
				x->parent->redp = false;
				x->parent->parent->redp = true;
				itree_rot_right(it, x->parent->parent);
			}
		} else {
			/* case for x->parent == x->parent->parent->right */
			/* this part is just like the section above with */
			/* left and right interchanged */
			y = x->parent->parent->left;
			if (y->redp) {
				x->parent->redp = false;
				y->redp = false;
				x->parent->parent->redp = true;
				x = x->parent->parent;
			} else {
				if (x == x->parent->left) {
					x = x->parent;
					itree_rot_right(it, x);
				}
				x->parent->redp = false;
				x->parent->parent->redp = true;
				itree_rot_left(it, x->parent->parent);
			}
		}
	}
	it->root->left->redp = false;

	res->data = data;
	return res;
}

it_node_t
itree_succ_of(itree_t it, it_node_t x)
{ 
	it_node_t y;

	if (it->nil != (y = x->right)) {
		/* get the minimum of the right subtree of x */
		while (y->left != it->nil) {
			y = y->left;
		}
		return y;
	} else {
		y = x->parent;
		while (x == y->right) {
			x = y;
			y = y->parent;
		}
		if (y == it->root) {
			return it->nil;
		}
		return y;
	}
}

it_node_t
itree_pred_of(itree_t it, it_node_t x)
{
	it_node_t y;

	if (it->nil != (y = x->left)) {
		while (y->right != it->nil) {
			/* returns the maximum of the left subtree of x */
			y = y->right;
		}
		return y;
	} else {
		y = x->parent;
		while (x == y->left) { 
			if (y == it->root) {
				return it->nil;
			}
			x = y;
			y = y->parent;
		}
		return y;
	}
}

static void
itree_del_fixup(itree_t it, it_node_t x)
{
	it_node_t rl = it->root->left;

	while ((!x->redp) && (rl != x)) {
		it_node_t w;
		if (x == x->parent->left) {
			w = x->parent->right;
			if (w->redp) {
				w->redp = false;
				x->parent->redp = true;
				itree_rot_left(it, x->parent);
				w = x->parent->right;
			}
			if ((!w->right->redp) && (!w->left->redp)) { 
				w->redp = true;
				x = x->parent;
			} else {
				if (!w->right->redp) {
					w->left->redp = false;
					w->redp = true;
					itree_rot_right(it, w);
					w = x->parent->right;
				}
				w->redp = x->parent->redp;
				x->parent->redp = false;
				w->right->redp = false;
				itree_rot_left(it, x->parent);
				break;
			}
		} else { 
			/* the code below has left and right
			 * switched from above */
			w = x->parent->left;
			if (w->redp) {
				w->redp = false;
				x->parent->redp = true;
				itree_rot_right(it, x->parent);
				w = x->parent->left;
			}
			if ((!w->right->redp) && (!w->left->redp)) { 
				w->redp = true;
				x = x->parent;
			} else {
				if (!w->left->redp) {
					w->right->redp = false;
					w->redp = true;
					itree_rot_left(it, w);
					w = x->parent->left;
				}
				w->redp = x->parent->redp;
				x->parent->redp = false;
				w->left->redp = false;
				itree_rot_right(it, x->parent);
				break;
			}
		}
	}
	x->redp = false;
	return;
}

void*
itree_del_node(itree_t it, it_node_t z)
{
	it_node_t y, x;
	void *res = z->data;

	y = ((z->left == it->nil) || (z->right == it->nil))
		? z
		: itree_succ_of(it, z);
	x = (y->left == it->nil)
		? y->right
		: y->left;

	if (it->root == (x->parent = y->parent)) {
		it->root->left = x;

	} else {
		if (y == y->parent->left) {
			y->parent->left = x;
		} else {
			y->parent->right = x;
		}
	}
	if (y != z) {
		/* y should not be nil in this case */
		/* y is the node to splice out and x is its child */
		y->max_high = INT_MIN;
		y->left = z->left;
		y->right = z->right;
		y->parent = z->parent;
		z->left->parent = z->right->parent = y;

		if (z == z->parent->left) {
			z->parent->left = y; 
		} else {
			z->parent->right = y;
		}
		itree_fixup_max_high(it, x->parent);

		if (!y->redp) {
			y->redp = z->redp;
			itree_del_fixup(it, x);
		} else {
			y->redp = z->redp;
		}
		free_node(z);
	} else {
		itree_fixup_max_high(it, x->parent);
		if (!y->redp) {
			itree_del_fixup(it, x);
		}
		free_node(y);
	}
	return res;
}

static inline bool
overlapp(int a1, int a2, int b1, int b2)
{
	if (a1 <= b1) {
		return (b1 <= a2);
	} else {
		return (a1 <= b2);
	}
}


/* printer shit */
static void
it_node_print(it_node_t in, it_node_t nil, it_node_t root)
{
	printf("k=%i, h=%i, mh=%i", in->key, in->high, in->max_high);
	puts("  l->key=");
	if (in->left == nil) {
		puts("NULL");
	} else {
		printf("%i", in->left->key);
	}
	puts("  r->key=");
	if (in->right == nil) {
		puts("NULL");
	} else {
		printf("%i", in->right->key);
	}
	puts("  p->key=");
	if (in->parent == root) {
		puts("NULL");
	} else {
		printf("%i", in->parent->key);
	}
	printf("  red=%i\n", in->redp);
	return;
}

static void
itree_print_helper(itree_t it, it_node_t x)
{
	if (x != it->nil) {
		itree_print_helper(it, x->left);
		it_node_print(x, it->nil, it->root);
		itree_print_helper(it, x->right);
	}
	return;
}

void
itree_print(itree_t it)
{
	itree_print_helper(it, it->root->left);
	return;
}

/* intvtree.c ends here */
