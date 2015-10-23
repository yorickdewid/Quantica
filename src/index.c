#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "index.h"
#include "zmalloc.h"

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
static FILE *fptree;

void get_node(long int offset, node_t *pnode) {
	if (offset == root) {
		*pnode = rootnode;
		return;
	}
	if (fseek(fptree, offset, SEEK_SET)) {
		lprint("[erro] Failed to read disk\n");
		return;
	}
	if (fread(pnode, sizeof(node_t), 1, fptree) == 0)
		lprint("[erro] Failed to read disk\n");
}

static void flush_node(long int t, node_t *pnode) {
	if (t == root)
		rootnode = *pnode;
	if (fseek(fptree, t, SEEK_SET)) {
		lprint("[erro] Failed to write disk\n");
		return;
	}
	if (fwrite(pnode, sizeof(node_t), 1, fptree) == 0)
		lprint("[erro] Failed to write disk\n");
}

static long int alloc_node() {
	long int offset;
	node_t node;

	/* Check freelist */
	if (freelist == -1) {
		if (fseek(fptree, 0, SEEK_END)) {
			lprint("[erro] Failed to write disk\n");
			return -1;
		}
		offset = ftell(fptree);
		flush_node(offset, &node);
	} else {
		offset = freelist;
		get_node(offset, &node);
		freelist = node.ptr[0];
	}
	return offset;
}

static void free_node(long int offset) {
	node_t node;

	get_node(offset, &node);
	node.ptr[0] = freelist;
	freelist = offset;
	flush_node(offset, &node);
}

static void index_read() {
	if (fseek(fptree, 0, SEEK_SET)) {
		lprint("[erro] Failed to read disk\n");
		return;
	}
	if (fread(&index_root, sizeof(index_root), 1, fptree) == 0)
		lprint("[erro] Failed to read disk\n");
	get_node(index_root.root, &rootnode);
	root = index_root.root;
	freelist = index_root.freelist;
}

static void index_write() {
	index_root.root = root;
	index_root.freelist = freelist;
	if (fseek(fptree, 0, SEEK_SET)) {
		lprint("[erro] Failed to write disk\n");
		return;
	}
	if (fwrite(&index_root, sizeof(index_root), 1, fptree) == 0)
		lprint("[erro] Failed to write disk\n");
	if (root != -1)
		flush_node(root, &rootnode);
}

void index_init(char *treefilname) {
	fptree = fopen(treefilname, "r+b");
	if (fptree == NULL) {
		fptree = fopen(treefilname, "w+b");
		index_write();
	} else {
		index_read();
	}
}

void index_close() {
	index_write();
	fclose(fptree);
}

static int get(char *key, item_t *kv, int n) {
	int i, left, right;

	if (strcmp(key, kv[0].key) <= 0)
		return 0;
	if (strcmp(key, kv[n - 1].key) > 0)
		return n;

	left = 0;
	right = n - 1;
	while (right - left > 1) {
		i = (right + left) / 2;
		if (strcmp(key, kv[i].key) <= 0)
			right = i;
		else
			left = i;
	}
	return right;
}

status_t index_get(char *key) {
	int i, j, n;
	item_t *kv = NULL;
	node_t node;
	long int offset = root;

#if 1
	printf("Looking for: %s\n", key);
	puts("Search path:");
#endif
	while (offset != -1) {
		get_node(offset, &node);
		kv = node.items;
		n = node.cnt;

#if 1
		for (j = 0; j < n; ++j)
			printf("  %s", kv[j].key);
		puts("");
#endif

		i = get(key, kv, n);
		if (i < n && !strcmp(key, kv[i].key)) {

#if 1
			printf("valset %llu\n", kv[i].valset);
			printf("Found in position %d\n", i);
#endif

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
static status_t insert(char *key, size_t key_size, long long int valset, long int offset, char **keynew, size_t *key_sizenew, long long int *valsetnew, long int *offsetnew) {
	long int offsetnew_r, *p;
	int i, j, *count;
	char *keynew_r = NULL;
	size_t keynew_r_size;
	long long int valsetnew_r;
	item_t *kv = NULL;
	status_t code;
	node_t node;

	/* If leaf return to recursive caller */
	if (offset == -1) {
		*offsetnew = -1;
		*keynew = zstrdup(key);
		*key_sizenew = key_size;
		*valsetnew = valset;
		return INSERTNOTCOMPLETE;
	}

	get_node(offset, &node);
	count = &node.cnt;
	kv = node.items;
	p = node.ptr;

	/*  Select pointer p[i] and try to insert key in the subtree of whichp[i] is the root:  */
	i = get(key, kv, *count);
	if (i < *count && !strcmp(key, kv[i].key))
		return DUPLICATEKEY;
	code = insert(key, key_size, valset, p[i], &keynew_r, &keynew_r_size, &valsetnew_r, &offsetnew_r);
	if (code != INSERTNOTCOMPLETE) {
		zfree(keynew_r);
		return code;
	}
	/* Insertion in subtree did not completely succeed; try to insert keynew_r and offsetnew_r in the current node:  */
	if (*count < INDEX_SIZE) {
		i = get(keynew_r, kv, *count);
		for (j = *count; j > i; j--) {
			kv[j] = kv[j - 1];
			p[j + 1] = p[j];
		}
		strlcpy(kv[i].key, keynew_r, keynew_r_size + 1);
		kv[i].key_size = keynew_r_size;
		kv[i].valset = valsetnew_r;
		p[i + 1] = offsetnew_r;
		++*count;
		zfree(keynew_r);
		flush_node(offset, &node);
		return SUCCESS;
	}
	/*  The current node was already full, so split it.  Pass item kv[INDEX_SIZE/2] in the
	 middle of the augmented sequence back through parameter y, so that it
	 can move upward in the tree.  Also, pass a pointer to the newly created
	 node back through u.  Return INSERTNOTCOMPLETE, to report that insertion
	 was not completed:    */
	long int p_final;
	char *k_final = NULL;
	long long int v_final;
	size_t s_final;
	node_t newnode;
	if (i == INDEX_SIZE) {
		k_final = keynew_r;
		s_final = keynew_r_size;
		p_final = offsetnew_r;
		v_final = valsetnew_r;
	} else {
		k_final = zstrdup(kv[INDEX_SIZE - 1].key);
		s_final = kv[INDEX_SIZE - 1].key_size;
		v_final = kv[INDEX_SIZE - 1].valset;
		p_final = p[INDEX_SIZE];
		for (j = INDEX_SIZE - 1; j > i; j--) {
			kv[j] = kv[j - 1];
			p[j + 1] = p[j];
		}
		strlcpy(kv[i].key, keynew_r, keynew_r_size + 1);
		kv[i].key_size = keynew_r_size;
		kv[i].valset = valsetnew_r;
		p[i + 1] = offsetnew_r;
	}
	*keynew = zstrdup(kv[INDEX_MSIZE].key);
	*key_sizenew = kv[INDEX_MSIZE].key_size;
	*valsetnew = kv[INDEX_MSIZE].valset;
	*count = INDEX_MSIZE;
	*offsetnew = alloc_node();
	newnode.cnt = INDEX_MSIZE;
	for (j = 0; j < INDEX_MSIZE - 1; j++) {
		newnode.items[j] = kv[j + INDEX_MSIZE + 1];
		newnode.ptr[j] = p[j + INDEX_MSIZE + 1];
	}
	newnode.ptr[INDEX_MSIZE - 1] = p[INDEX_SIZE];
	strlcpy(newnode.items[INDEX_MSIZE - 1].key, k_final, s_final + 1);
	newnode.items[INDEX_MSIZE - 1].key_size = s_final;
	newnode.items[INDEX_MSIZE - 1].valset = v_final;
	newnode.ptr[INDEX_MSIZE] = p_final;
	zfree(k_final);
	flush_node(offset, &node);
	flush_node(*offsetnew, &newnode);
	return INSERTNOTCOMPLETE;
}

status_t index_insert(char *key, size_t key_size, long long int valset) {
	long int offsetnew, offset;
	char *keynew = NULL;
	size_t key_sizenew;
	long long int valsetnew;
	status_t code = insert(key, key_size, valset, root, &keynew, &key_sizenew, &valsetnew, &offsetnew);

	if (code == INSERTNOTCOMPLETE) {
		offset = alloc_node();
		rootnode.cnt = 1;
		strlcpy(rootnode.items[0].key, keynew, key_sizenew + 1);
		rootnode.items[0].key_size = key_sizenew;
		rootnode.items[0].valset = valsetnew;
		rootnode.ptr[0] = root;
		rootnode.ptr[1] = offsetnew;
		root = offset;
		zfree(keynew);
		flush_node(offset, &rootnode);
		code = SUCCESS;
	}
	return code;
}

static status_t delete(char *key, long int t) {
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
	get_node(t, &nod);
	n = &nod.cnt;
	kv = nod.items;
	p = nod.ptr;
	i = get(key, kv, *n);

	/* *t is a leaf */
	if (p[0] == -1) {
		if (i == *n || strcmp(key, kv[i].key) < 0)
			return NOTFOUND;
		/* key is now equal to k[i], located in a leaf:  */
		for (j = i + 1; j < *n; j++) {
			kv[j - 1] = kv[j];
			p[j] = p[j + 1];
		}
		--*n;
		flush_node(t, &nod);
		return *n >= (t == root ? 1 : INDEX_MSIZE) ? SUCCESS : UNDERFLOW;
	}

	/*  t is an interior node (not a leaf): */
	item = kv + i;
	left = p[i];
	get_node(left, &nod1);
	nleft = & nod1.cnt;
	/* key found in interior node.  Go to left child *p[i] and then follow a

	  path all the way to a leaf, using rightmost branches:  */
	if (i < *n && !strcmp(key, item->key)) {
		size_t key_size = item->key_size;
		q = p[i];
		get_node(q, &nod1);
		nq = nod1.cnt;
		while (q1 = nod1.ptr[nq], q1 != -1) {
			q = q1;
			get_node(q, &nod1);
			nq = nod1.cnt;
		}

		/*  Exchange k[i] with the rightmost item in that leaf:   */
		addr = nod1.items + nq - 1;
		*item = *addr;
		strlcpy(addr->key, key, key_size + 1);
		flush_node(t, &nod);
		flush_node(q, &nod1);
	}

	/*  Delete key in subtree with root p[i]:  */
	code = delete(key, left);
	if (code != UNDERFLOW)
		return code;

	/*  Underflow, borrow, and , if necessary, merge:  */
	if (i < *n)
		get_node(p[i + 1], &nodR);
	if (i == *n || nodR.cnt == INDEX_MSIZE) {
		if (i > 0) {
			get_node(p[i - 1], &nodL);
			if (i == *n || nodL.cnt > INDEX_MSIZE)
				borrowleft = 1;
		}
	}

	/* borrow from left sibling */
	if (borrowleft) {
		item = kv + i - 1;
		left = p[i - 1];
		right = p[i];
		nod1 = nodL;
		get_node(right, &nod2);
		nleft = & nod1.cnt;
	} else {
		right = p[i + 1];      /*  borrow from right sibling   */
		get_node(left, &nod1);
		nod2 = nodR;
	}
	nright = & nod2.cnt;
	lkey = nod1.items;
	rkey = nod2.items;
	lptr = nod1.ptr;
	rptr = nod2.ptr;
	if (borrowleft) {
		rptr[*nright + 1] = rptr[*nright];
		for (j = *nright; j > 0; j--) {
			rkey[j] = rkey[j - 1];
			rptr[j] = rptr[j - 1];
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
		if (*nright > INDEX_MSIZE) {
			lkey[INDEX_MSIZE - 1] = *item;
			lptr[INDEX_MSIZE] = rptr[0];
			*item = rkey[0];
			++*nleft;
			--*nright;
			for (j = 0; j < *nright; j++) {
				rptr[j] = rptr[j + 1];
				rkey[j] = rkey[j + 1];
			}
			rptr[*nright] = rptr[*nright + 1];
			flush_node(t, &nod);
			flush_node(left, &nod1);
			flush_node(right, &nod2);
			return SUCCESS;
		}
	}

	/*  Merge   */
	lkey[INDEX_MSIZE - 1] = *item;
	lptr[INDEX_MSIZE] = rptr[0];
	for (j = 0; j < INDEX_MSIZE; j++) {
		lkey[INDEX_MSIZE + j] = rkey[j];
		lptr[INDEX_MSIZE + j + 1] = rptr[j + 1];
	}
	*nleft = INDEX_SIZE;
	free_node(right);
	for (j = i + 1; j < *n; j++) {
		kv[j - 1] = kv[j];
		p[j] = p[j + 1];
	}
	--*n;
	flush_node(t, &nod);
	flush_node(left, &nod1);

	return *n >= (t == root ? 1 : INDEX_MSIZE) ? SUCCESS : UNDERFLOW;
}

status_t index_delete(char *key) {
	long int newroot;

	status_t code = delete(key, root);
	if (code == UNDERFLOW) {
		newroot = rootnode.ptr[0];
		free_node(root);
		if (newroot != -1)
			get_node(newroot, &rootnode);
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
		get_node(offset, &nod);
		kv = nod.items;
		n = nod.cnt;
		printf("%*s", position, "");
		for (i = 0; i < n; i++)
			printf(" %.*s[%d][%llu]", kv[i].key_size, kv[i].key, kv[i].key_size, kv[i].valset);
		puts("");
		for (i = 0; i <= n; i++)
			index_print(nod.ptr[i]);
		position -= 6;
	}
}

void index_print_root() {
	index_print(root);
}
#endif
