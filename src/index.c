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

void readnode(long t, node_t *pnode) {
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

void writenode(long t, node_t *pnode) {
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

/*
	Insert x in B-tree with root t.  If not completely successful, the
	integer *y and the pointer *u remain to be inserted.
*/
static status_t insert(int x, long t, int *y, long *u) {
	long tnew, p_final, *p;
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
	n = & nod.cnt;
	k = nod.key;
	p = nod.ptr;
	/*  Select pointer p[i] and try to insert x in  the subtree of whichp[i]
	  is  the root:  */
	i = get(x, k, *n);
	if (i < *n && x == k[i])
		return DUPLICATEKEY;
	code = insert(x, p[i], &xnew, &tnew);
	if (code != INSERTNOTCOMPLETE)
		return code;
	/* Insertion in subtree did not completely succeed; try to insert xnew and
	tnew in the current node:  */
	if (*n < MM){
		i = get(xnew, k, *n);
		for (j = *n; j > i; j--){
			k[j] = k[j-1];
			p[j+1] = p[j];
		}
		k[i] = xnew;
		p[i+1] = tnew;
		++*n;
		writenode(t, &nod);
		return SUCCESS;
	}
	/*  The current node was already full, so split it.  Pass item k[M] in the
	 middle of the augmented sequence back through parameter y, so that it
	 can move upward in the tree.  Also, pass a pointer to the newly created
	 node back through u.  Return INSERTNOTCOMPLETE, to report that insertion
	 was not completed:    */
	if (i == MM) {
	  k_final = xnew;
	  p_final = tnew;
	 } else {
		  k_final = k[MM-1];
		  p_final = p[MM];
		  for (j=MM-1; j>i; j--){
			  k[j] = k[j-1];
			  p[j+1] = p[j];
		  }
			k[i] = xnew;
			p[i+1] = tnew;
	}
	*y = k[M];
	*n = M;
	*u = getnode();
	newnod.cnt = M;
	for (j=0; j< M-1; j++){
		newnod.key[j] = k[j+M+1];
		newnod.ptr[j] = p[j+M+1];
	}
	newnod.ptr[M-1] = p[MM];
	newnod.key[M-1] = k_final;
	newnod.ptr[M] = p_final;
	writenode(t, &nod);
	writenode(*u, &newnod);
	return INSERTNOTCOMPLETE;
}

status_t index_insert(int x) {
	long int tnew, u;
	int xnew;
	status_t code = insert(x, root, &xnew, &tnew);

	if (code == DUPLICATEKEY)
		printf("Duplicate uid %d ignored \n", x);
	else {
	 	if (code == INSERTNOTCOMPLETE) {
			u = getnode();
			rootnode.cnt = 1;
			rootnode.key[0] = xnew;
			rootnode.ptr[0] = root;
			rootnode.ptr[1] = tnew;
			root = u;
			writenode(u, &rootnode);
			code = SUCCESS;
		}
	}
	return code;
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