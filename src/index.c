#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "index.h"

node_t rootnode;

struct {
	long int root;
	long int freelist;
} index_root = {
	.root = -1,
	.freelist = -1
};

long int root = -1;
long int freelist = -1;
FILE *fptree;

void readnode(long int offset, node_t *pnode) {
	if (offset == root) {
		*pnode = rootnode;
		return;
	}
	if (fseek(fptree, offset, SEEK_SET)) {
		lprintf("[erro] Failed to read disk\n");
		return;
	}
	if (fread(pnode, sizeof(node_t), 1, fptree) == 0)
		lprintf("[erro] Failed to read disk\n");
}

static void flush_node(long int t, node_t *pnode) {
	if (t == root)
		rootnode = *pnode;
	if (fseek(fptree, t, SEEK_SET)) {
		lprintf("[erro] Failed to write disk\n");
		return;
	}
	if (fwrite(pnode, sizeof(node_t), 1, fptree) == 0)
		lprintf("[erro] Failed to write disk\n");
}

static long int alloc_node() {
	long int t;
	node_t nod;

	if (freelist == -1) {
		if (fseek(fptree, 0, SEEK_END)) {
			lprintf("[erro] Failed to write disk\n");
			return -1;
		}
		t = ftell(fptree);
		flush_node(t, &nod);
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
	flush_node(t, &nod);
}

void rdstart() {
	if (fseek(fptree, 0, SEEK_SET)) {
		lprintf("[erro] Failed to read disk\n");
		return;
	}
	if (fread(&index_root, sizeof(index_root), 1, fptree) == 0)
		lprintf("[erro] Failed to read disk\n");
	readnode(index_root.root, &rootnode);
	root = index_root.root;
	freelist = index_root.freelist;
}

void wrstart() {
	index_root.root = root;
	index_root.freelist = freelist;
	if (fseek(fptree, 0, SEEK_SET)) {
		lprintf("[erro] Failed to write disk\n");
		return;
	}
	if (fwrite(&index_root, sizeof(index_root), 1, fptree) == 0)
		lprintf("[erro] Failed to write disk\n");
	if (root != -1)
		flush_node(root, &rootnode);
}

static int get(int64_t key, item_t *kv, int n) {
	int i, left, right;

	if (key <= kv[0].key)
		return 0;
	if (key > kv[n-1].key)
		return n;

	left = 0;
	right = n-1;
	while (right - left > 1){
		i = (right + left)/2;
		if (key <= kv[i].key)
			right = i;
		else
			left = i;
	}
	return right;
}

status_t index_get(int64_t key) {
	int i, j, n;
	item_t *kv = NULL;
	node_t node;
	long int offset = root;

	printf("Looking for: %ld\n", key);

	puts("Search path:");
	while (offset != -1) {
		readnode(offset, &node);
		kv = node.items;
		n = node.cnt;

#ifdef DEBUG
		for (j=0; j<n; ++j)
			printf("  %ld", kv[j].key);
		puts("");
#endif // DEBUG

		i = get(key, kv, n);
		if (i < n && key == kv[i].key) {
			node_t node_found;

#ifdef DEBUG
			printf("valset %ld\n", kv[i].valset);
			printf("Found in position %d of node with contents: ", i);
			readnode(offset, &node_found);
			for (i=0; i<node_found.cnt; ++i)
				printf("  %ld", node_found.items[i].key);
			puts("");
#endif // DEBUG

			return SUCCESS;
		}
		offset = node.ptr[i];
	}
	return NOTFOUND;
}

/*
	Insert x in B-tree with root offset.  If not completely successful, the
	integer *y and the pointer *u remain to be inserted.
*/
static status_t insert(int64_t key, int64_t valset, long int offset, int64_t *keynew, int64_t *valsetnew, long int *offsetnew) {
	long int tnew, p_final, *p;
	int i, j, *n;
	int64_t k_final, v_final, xnew, valsetnew_r;
	item_t *kv = NULL;
	status_t code;
	node_t nod, newnod;

	/*  Examine whether offset is a pointer member in a leaf  */
	if (offset == -1) {
		*offsetnew = -1;
		*keynew = key;
		*valsetnew = valset;
		return INSERTNOTCOMPLETE;
	}

	readnode(offset, &nod);
	n = &nod.cnt;
	kv = nod.items;
	p = nod.ptr;
	/*  Select pointer p[i] and try to insert key in  the subtree of whichp[i] is  the root:  */
	i = get(key, kv, *n);
	if (i < *n && key == kv[i].key)
		return DUPLICATEKEY;
	code = insert(key, valset, p[i], &xnew, &valsetnew_r, &tnew);
	if (code != INSERTNOTCOMPLETE)
		return code;
	/* Insertion in subtree did not completely succeed; try to insert xnew and tnew in the current node:  */
	if (*n < INDEX_SIZE) {
		i = get(xnew, kv, *n);
		for (j = *n; j > i; j--) {
			kv[j] = kv[j-1];
			p[j+1] = p[j];
		}
		kv[i].key = xnew;
		kv[i].valset = valsetnew_r;
		p[i+1] = tnew;
		++*n;
		flush_node(offset, &nod);
		return SUCCESS;
	}
	/*  The current node was already full, so split it.  Pass item kv[INDEX_SIZE/2] in the
	 middle of the augmented sequence back through parameter y, so that it
	 can move upward in the tree.  Also, pass a pointer to the newly created
	 node back through u.  Return INSERTNOTCOMPLETE, to report that insertion
	 was not completed:    */
	if (i == INDEX_SIZE) {
		k_final = xnew;
		p_final = tnew;
		v_final = valsetnew_r;
	} else {
		k_final = kv[INDEX_SIZE-1].key;
		v_final = kv[INDEX_SIZE-1].valset;
		p_final = p[INDEX_SIZE];
		for (j=INDEX_SIZE-1; j>i; j--) {
			kv[j] = kv[j-1];
			p[j+1] = p[j];
		}
		kv[i].key = xnew;
		p[i+1] = tnew;
	}
	*keynew = kv[INDEX_MSIZE].key;
	*valsetnew = kv[INDEX_MSIZE].valset;
	*n = INDEX_MSIZE;
	*offsetnew = alloc_node();
	newnod.cnt = INDEX_MSIZE;
	for (j=0; j< INDEX_MSIZE-1; j++) {
		newnod.items[j] = kv[j+INDEX_MSIZE+1];
		newnod.ptr[j] = p[j+INDEX_MSIZE+1];
	}
	newnod.ptr[INDEX_MSIZE-1] = p[INDEX_SIZE];
	newnod.items[INDEX_MSIZE-1].key = k_final;
	newnod.items[INDEX_MSIZE-1].valset = v_final;
	newnod.ptr[INDEX_MSIZE] = p_final;
	flush_node(offset, &nod);
	flush_node(*offsetnew, &newnod);
	return INSERTNOTCOMPLETE;
}

status_t index_insert(int64_t key, int64_t valset) {
	long int offsetnew, offset;
	int64_t keynew, valsetnew;
	status_t code = insert(key, valset, root, &keynew, &valsetnew, &offsetnew);

	if (code == DUPLICATEKEY)
		printf("Duplicate uid %ld ignored \n", key);
	else if (code == INSERTNOTCOMPLETE) {
		puts("HIT");
		offset = alloc_node();
		rootnode.cnt = 1;
		rootnode.items[0].key = keynew;
		rootnode.items[0].valset = valsetnew;
		rootnode.ptr[0] = root;
		rootnode.ptr[1] = offsetnew;
		root = offset;
		flush_node(offset, &rootnode);
		code = SUCCESS;
	}
	return code;
}

static status_t delete(int64_t key, long int t) {
	int i, j, *n, *nleft, *nright, borrowleft = 0, nq;
	status_t code;
	item_t *kv;
	item_t *item;
	item_t *addr;
	item_t *lkey; 
	item_t *rkey;
	long int *p, left, right, *lptr, *rptr, q, q1;
	node_t nod, nod1, nod2, nodL, nodR;

	if (t == -1)
		return NOTFOUND;
	readnode(t, &nod);
	n = &nod.cnt;
	kv = nod.items;
	p = nod.ptr;
	i = get(key, kv, *n);

	/* *t is a leaf */
	if (p[0] == -1) {
		if (i == *n || key < kv[i].key)
			return NOTFOUND;
		/* key is now equal to k[i], located in a leaf:  */
		for (j=i+1; j < *n; j++) {
			kv[j-1] = kv[j];
			p[j] = p[j+1];
		}
		--*n;
		flush_node(t, &nod);
		return *n >= (t==root ? 1 : INDEX_MSIZE) ? SUCCESS : UNDERFLOW;
	}

	/*  t is an interior node (not a leaf): */
	item = kv+i;
	left = p[i];
	readnode(left, &nod1);
	nleft = & nod1.cnt;
	/* key found in interior node.  Go to left child *p[i] and then follow a

	  path all the way to a leaf, using rightmost branches:  */
	if (i < *n && key == item->key) {
		q = p[i];
		readnode(q, &nod1);
		nq = nod1.cnt;
		while (q1 = nod1.ptr[nq], q1!= -1){
			q = q1;
			readnode(q, &nod1);
			nq = nod1.cnt;
		}

		/*  Exchange k[i] with the rightmost item in that leaf:   */
		addr = nod1.items + nq -1;
		*item = *addr;
		addr->key = key;
		flush_node(t, &nod);
		flush_node(q, &nod1);
	}

	/*  Delete key in subtree with root p[i]:  */
	code = delete(key, left);
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
		item = kv+i-1;
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
	lkey = nod1.items;
	rkey = nod2.items;
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
			flush_node(t, &nod);
			flush_node(left, &nod1);
			flush_node(right, &nod2);
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
			flush_node(t, &nod);
			flush_node(left, &nod1);
			flush_node(right, &nod2);
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
		kv[j-1] = kv[j];
		p[j] = p[j+1];
	}
	--*n;
	flush_node(t, &nod);
	flush_node(left, &nod1);

	return *n >= (t==root ? 1 : INDEX_MSIZE) ? SUCCESS : UNDERFLOW;
}

status_t index_delete(int64_t key) {
	long int newroot;

	status_t code = delete(key, root);
	if (code == UNDERFLOW) {
		newroot = rootnode.ptr[0];
		freenode(root);
		if (newroot != -1)
			readnode(newroot, &rootnode);
		root = newroot;
		code = SUCCESS;
	}
	return code;  /* Return value:  SUCCESS  or NOTFOUND   */
}

#if 1
void index_print(long int offset) {
	static int position = 0;
	int i, n;
	item_t *kv = NULL;
	node_t nod;

	if (offset != -1) {
		position += 6;
		readnode(offset, &nod);
		kv = nod.items;
		n = nod.cnt;
		printf("%*s", position, "");
		for (i=0; i<n; i++)
			printf(" %ld", kv[i].key);
		puts("");
		for (i=0; i<=n; i++)
			index_print(nod.ptr[i]);
		position -= 6;
	}
}

void index_print_root() {
	index_print(root);
}

void index_init(char *treefilnam) {
	fptree = fopen(treefilnam, "r+b");
	if (fptree == NULL) {
		fptree = fopen(treefilnam, "w+b");
		wrstart();
		puts("NEW DB");
	} else {
		puts("OPEN DB");
		rdstart();
		index_print(root);
	}
}

void index_close() {
	wrstart();
	fclose(fptree);
}
#endif
