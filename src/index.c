#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "index.h"

node_t rootnode;
long int start[2], root = NIL, freelist = NIL;
FILE *fptree;

void readnode(long int t, node_t *pnode) {
	if (t == root) {
		*pnode = rootnode;
		return;
	}
	if (fseek(fptree, t, SEEK_SET)) {
		lprintf("[erro] Failed to read disk\n");
		return;
	}
	if (fread(pnode, sizeof(node_t), 1, fptree) == 0)
		lprintf("[erro] Failed to read disk\n");
}

void writenode(long int t, node_t *pnode) {
	if (t == root)
		rootnode = *pnode;
	if (fseek(fptree, t, SEEK_SET)) {
		lprintf("[erro] Failed to write disk\n");
		return;
	}
	if (fwrite(pnode, sizeof(node_t), 1, fptree) == 0)
		lprintf("[erro] Failed to write disk\n");
}

long int getnode() {
	long int t;
	node_t nod;

	if (freelist == NIL) {
		if (fseek(fptree, 0, SEEK_END)) {
			lprintf("[erro] Failed to write disk\n");
			return -1;
		}
		t = ftell(fptree);
		writenode(t, &nod);
	} else { /*  Allocate space on disk  */
		t = freelist;
		readnode(t, &nod);             /*  To update freelist      */
		freelist = nod.ptr[0];
	}
	return t;
}

void freenode(long int t) {
	node_t nod;

	readnode(t, &nod);
	nod.ptr[0] = freelist;
	freelist = t;
	writenode(t, &nod);
}

void rdstart() {
	if (fseek(fptree, 0, SEEK_SET)) {
		lprintf("[erro] Failed to read disk\n");
		return;
	}
	if (fread(start, sizeof(long), 2, fptree) == 0)
		lprintf("[erro] Failed to read disk\n");
	readnode(start[0], &rootnode);
	root = start[0];
	freelist = start[1];
}

void wrstart() {
	start[0] = root;
	start[1] = freelist;
	if (fseek(fptree, 0, SEEK_SET)) {
		lprintf("[erro] Failed to write disk\n");
		return;
	}
	if (fwrite(start, sizeof(long int), 2, fptree) == 0)
		lprintf("[erro] Failed to write disk\n");
	if (root != NIL)
		writenode(root, &rootnode);
}

static int get(int x, int *a, int n) {
	int i, left, right;

	if (x <= a[0])
		return 0;
	if (x > a[n-1])
		return n;

	left = 0;
	right = n-1;
	while (right -  left > 1){
		i = (right + left)/2;
		if (x <= a[i])
			right = i;
		else
			left = i;
	}
	return right;
}

status_t index_get(int x) {
	int i, j,*k, n;
	node_t nod;
	long int t = root;

	puts("Search path:");
	while (t != NIL){
		readnode(t, &nod);
		k = nod.key;
		n = nod.cnt;
		for (j=0; j < n; j++)
			printf("  %d", k[j]);
		puts("");
		i = get(x, k, n);
		if (i < n && x == k[i]) {
			node_t nodf;

			printf("Found in position %d of node with contents:  ", i);
			readnode(t, &nodf);
			for (i=0; i < nodf.cnt; i++)
				printf("  %d", nodf.key[i]);
			puts("");

			return SUCCESS;
		}
		t = nod.ptr[i];
	}
	return NOTFOUND;
}

/*
	Insert x in B-tree with root t.  If not completely successful, the
	integer *y and the pointer *u remain to be inserted.
*/
static status_t insert(int x, long int t, int *y, long int *u) {
	long int tnew, p_final, *p;
	int i, j, *n, k_final, *k, xnew;
	status_t code;
	node_t nod, newnod;

	/*  Examine whether t is a pointer member in a leaf  */
	if (t == NIL) {
		*u = NIL;
		*y = x;
		return INSERTNOTCOMPLETE;
	}

	readnode(t, &nod);
	n = &nod.cnt;
	k = nod.key;
	p = nod.ptr;
	/*  Select pointer p[i] and try to insert x in  the subtree of whichp[i] is  the root:  */
	i = get(x, k, *n);
	if (i < *n && x == k[i])
		return DUPLICATEKEY;
	code = insert(x, p[i], &xnew, &tnew);
	if (code != INSERTNOTCOMPLETE)
		return code;
	/* Insertion in subtree did not completely succeed; try to insert xnew and tnew in the current node:  */
	if (*n < INDEX_SIZE) {
		i = get(xnew, k, *n);
		for (j = *n; j > i; j--) {
			k[j] = k[j-1];
			p[j+1] = p[j];
		}
		k[i] = xnew;
		p[i+1] = tnew;
		++*n;
		writenode(t, &nod);
		return SUCCESS;
	}
	/*  The current node was already full, so split it.  Pass item k[INDEX_SIZE/2] in the
	 middle of the augmented sequence back through parameter y, so that it
	 can move upward in the tree.  Also, pass a pointer to the newly created
	 node back through u.  Return INSERTNOTCOMPLETE, to report that insertion
	 was not completed:    */
	if (i == INDEX_SIZE) {
		k_final = xnew;
		p_final = tnew;
	} else {
		k_final = k[INDEX_SIZE-1];
		p_final = p[INDEX_SIZE];
		for (j=INDEX_SIZE-1; j>i; j--) {
			k[j] = k[j-1];
			p[j+1] = p[j];
		}
		k[i] = xnew;
		p[i+1] = tnew;
	}
	*y = k[INDEX_MSIZE];
	*n = INDEX_MSIZE;
	*u = getnode();
	newnod.cnt = INDEX_MSIZE;
	for (j=0; j< INDEX_MSIZE-1; j++) {
		newnod.key[j] = k[j+INDEX_MSIZE+1];
		newnod.ptr[j] = p[j+INDEX_MSIZE+1];
	}
	newnod.ptr[INDEX_MSIZE-1] = p[INDEX_SIZE];
	newnod.key[INDEX_MSIZE-1] = k_final;
	newnod.ptr[INDEX_MSIZE] = p_final;
	writenode(t, &nod);
	writenode(*u, &newnod);
	return INSERTNOTCOMPLETE;
}

status_t index_insert(int key) {
	long int tnew, u;
	int keynew;
	status_t code = insert(key, root, &keynew, &tnew);

	if (code == DUPLICATEKEY)
		printf("Duplicate uid %d ignored \n", key);
	else {
	 	if (code == INSERTNOTCOMPLETE) {
			u = getnode();
			rootnode.cnt = 1;
			rootnode.key[0] = keynew;
			rootnode.ptr[0] = root;
			rootnode.ptr[1] = tnew;
			root = u;
			writenode(u, &rootnode);
			code = SUCCESS;
		}
	}
	return code;
}

static status_t delete(int x, long int t) {
	int i, j, *k, *n,*item, *nleft, *nright, *lkey, *rkey, borrowleft = 0, nq, *addr;
	status_t code;
	long int *p, left, right, *lptr, *rptr, q, q1;
	node_t nod, nod1, nod2, nodL, nodR;

	if (t == NIL)
		return NOTFOUND;
	readnode(t, &nod);
	n = &nod.cnt;
	k = nod.key;
	p = nod.ptr;
	i = get(x, k, *n);

	/* *t is a leaf */
	if (p[0] == NIL) {
		if (i == *n || x < k[i])
			return NOTFOUND;
		/* x is now equal to k[i], located in a leaf:  */
		for (j=i+1; j < *n; j++) {
			k[j-1] = k[j];
			p[j] = p[j+1];
		}
		--*n;
		writenode(t, &nod);
		return *n >= (t==root ? 1 : INDEX_MSIZE) ? SUCCESS : UNDERFLOW;
	}

	/*  t is an interior node (not a leaf): */
	item = k+i;
	left = p[i];
	readnode(left, &nod1);
	nleft = & nod1.cnt;
	/* x found in interior node.  Go to left child *p[i] and then follow a

	  path all the way to a leaf, using rightmost branches:  */
	if (i < *n && x == *item) {
		q = p[i];
		readnode(q, &nod1);
		nq = nod1.cnt;
		while (q1 = nod1.ptr[nq], q1!= NIL){
			q = q1;
			readnode(q, &nod1);
			nq = nod1.cnt;
		}

		/*  Exchange k[i] with the rightmost item in that leaf:   */
		addr = nod1.key + nq -1;
		*item = *addr;
		*addr = x;
		writenode(t, &nod);
		writenode(q, &nod1);
	}

	/*  Delete x in subtree with root p[i]:  */
	code = delete(x, left);
	if (code != UNDERFLOW)
		return code;

	/*  Underflow, borrow, and , if necessary, merge:  */
	if (i < *n)
		readnode(p[i+1], &nodR);
	if (i == *n || nodR.cnt == INDEX_MSIZE) {
		if (i > 0) {
			readnode(p[i-1], &nodL);
			if (i == *n || nodL.cnt > INDEX_MSIZE)
				borrowleft = 1;
		}
	}

	/* borrow from left sibling */
	if (borrowleft) {
		item = k+i-1;
		left = p[i-1];
		right = p[i];
		nod1 = nodL;
		readnode(right, &nod2);
		nleft = & nod1.cnt;
	} else {
		right = p[i+1];        /*  borrow from right sibling   */
		readnode(left, &nod1);
		nod2 = nodR;
	}
	nright = & nod2.cnt;
	lkey = nod1.key;
	rkey = nod2.key;
	lptr = nod1.ptr;
	rptr = nod2.ptr;
	if (borrowleft) {
		rptr[*nright + 1] = rptr[*nright];
		for (j=*nright; j>0; j--){
			rkey[j] = rkey[j-1];
			rptr[j] = rptr[j-1];
		}
		++*nright;
		rkey[0] = *item;
		rptr[0] = lptr[*nleft];
		*item = lkey[*nleft - 1];
		if (--*nleft >= INDEX_MSIZE) {
			writenode(t, &nod);
			writenode(left, &nod1);
			writenode(right, &nod2);
			return SUCCESS;
		}
	} else {
		/* borrow from right sibling */
		if (*nright > INDEX_MSIZE){
			lkey[INDEX_MSIZE-1] = *item;
			lptr[INDEX_MSIZE] = rptr[0];
			*item = rkey[0];
			++*nleft;
			--*nright;
			for (j=0; j < *nright; j++) {
				rptr[j] = rptr[j+1];
				rkey[j] = rkey[j+1];
			}
			rptr[*nright] = rptr[*nright + 1];
			writenode(t, &nod);
			writenode(left, &nod1);
			writenode(right, &nod2);
			return SUCCESS;
		}
	}

	/*  Merge   */
	lkey[INDEX_MSIZE-1] = *item;
	lptr[INDEX_MSIZE] = rptr[0];
	for (j=0; j<INDEX_MSIZE; j++){
		lkey[INDEX_MSIZE+j] = rkey[j];
		lptr[INDEX_MSIZE+j+1] = rptr[j+1];
	}
	*nleft = INDEX_SIZE;
	freenode(right);
	for (j=i+1; j < *n; j++){
		k[j-1] = k[j];
		p[j] = p[j+1];
	}
	--*n;
	writenode(t, &nod);
	writenode(left, &nod1);

	return *n >= (t==root ? 1 : INDEX_MSIZE) ? SUCCESS : UNDERFLOW;
}

status_t index_delete(int x) {
	long int newroot;

	status_t code = delete(x, root);
	if (code == UNDERFLOW) {
		newroot = rootnode.ptr[0];
		freenode(root);
		if (newroot != NIL)
			readnode(newroot, &rootnode);
		root = newroot;
		code = SUCCESS;
	}
	return code;  /* Return value:  SUCCESS  or NOTFOUND   */
}

#if 1
void index_print(long int t) {
	static int position=0;
	int i, *k, n;
	node_t nod;

	if (t != NIL) {
		position += 6;
		readnode(t, &nod);
		k = nod.key;
		n = nod.cnt;
		printf("%*s", position, "");
		for (i=0; i<n; i++)
			printf(" %d", k[i]);
		puts("");
		for (i=0; i<=n; i++)
			index_print(nod.ptr[i]);
		position -= 6;
	}
}
#endif