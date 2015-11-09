#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <config.h>
#include <common.h>
#include <log.h>
#include <error.h>
#include "btree.h"
#include "zmalloc.h"

//TODO needs bitflip
struct root_super {
	long int root;
	long int freelist;
};

static void get_node(btree_t *index, long int offset, node_t *pnode) {
	if (offset == index->root) {
		*pnode = index->rootnode;
		return;
	}
	if (fseek(index->fp, offset, SEEK_SET)) {
		lprint("[erro] Failed to read disk\n");
		return;
	}
	if (fread(pnode, sizeof(node_t), 1, index->fp) == 0)
		lprint("[erro] Failed to read disk\n");
}

static void flush_node(btree_t *index, long int offset, node_t *pnode) {
	if (offset == index->root)
		index->rootnode = *pnode;
	if (fseek(index->fp, offset, SEEK_SET)) {
		lprint("[erro] Failed to write disk\n");
		return;
	}
	if (fwrite(pnode, sizeof(node_t), 1, index->fp) == 0)
		lprint("[erro] Failed to write disk\n");
}

static long int alloc_node(btree_t *index) {
	long int offset;
	node_t node;
	memset(&node, 0, sizeof(node_t));

	/* Check freelist */
	if (index->freelist < 0) {
		if (fseek(index->fp, 0, SEEK_END)) {
			lprint("[erro] Failed to write disk\n");
			return -1;
		}
		offset = ftell(index->fp);
		flush_node(index, offset, &node);
	} else {
		offset = index->freelist;
		get_node(index, offset, &node);
		index->freelist = node.ptr[0];
	}
	return offset;
}

static void free_node(btree_t *index, long int offset) {
	node_t node;
	memset(&node, 0, sizeof(node_t));

	get_node(index, offset, &node);
	node.ptr[0] = index->freelist;
	index->freelist = offset;
	flush_node(index, offset, &node);
}

static void storage_read(btree_t *index) {
	struct root_super super;
	memset(&super, 0, sizeof(struct root_super));

	if (fseek(index->fp, 0, SEEK_SET)) {
		lprint("[erro] Failed to read disk\n");
		return;
	}
	if (fread(&super, sizeof(struct root_super), 1, index->fp) == sizeof(struct root_super))
		lprint("[erro] Failed to read disk\n");
	get_node(index, super.root, &index->rootnode);
	index->root = super.root;
	index->freelist = super.freelist;
}

static void storage_write(btree_t *index) {
	struct root_super super;
	memset(&super, 0, sizeof(struct root_super));

	super.root = index->root;
	super.freelist = index->freelist;
	if (fseek(index->fp, 0, SEEK_SET)) {
		lprint("[erro] Failed to write disk\n");
		return;
	}
	if (fwrite(&super, sizeof(struct root_super), 1, index->fp) == 0)
		lprint("[erro] Failed to write disk\n");
	if (index->root != -1)
		flush_node(index, super.root, &index->rootnode);
}

void btree_init(btree_t *index, char *name) {
	memset(index, 0, sizeof(btree_t));
	index->root = -1;
	index->freelist = -1;
	index->fp = fopen(name, "r+b");
	if (index->fp == NULL) {
		// TODO set RW flag
		index->fp = fopen(name, "w+b");
		memset(&index->rootnode, 0, sizeof(node_t));
		storage_write(index);
	} else {
		storage_read(index);
	}
}

void btree_close(btree_t *index) {
	storage_write(index);
	if (index->fp)
		fclose(index->fp);
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

status_t index_get(btree_t *index, char *key) {
	int i, n;
	item_t *kv = NULL;
	node_t node;
	long int offset = index->root;

	while (offset != -1) {
		get_node(index, offset, &node);
		kv = node.items;
		n = node.cnt;
		i = get(key, kv, n);
		if (i < n && !strcmp(key, kv[i].key)) {
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
static status_t insert(btree_t *index, char *key, size_t key_size, long long int valset, long int offset, char **keynew, size_t *key_sizenew, long long int *valsetnew, long int *offsetnew) {
	long int offsetnew_r, *p;
	int i, j, *count;
	char *keynew_r = NULL;
	size_t keynew_r_size;
	long long int valsetnew_r;
	item_t *kv = NULL;
	status_t code;
	node_t node;
	memset(&node, 0, sizeof(node_t));

	/* If leaf return to recursive caller */
	if (offset == -1) {
		*offsetnew = -1;
		*keynew = zstrdup(key);
		*key_sizenew = key_size;
		*valsetnew = valset;
		return INSERTNOTCOMPLETE;
	}

	get_node(index, offset, &node);
	count = &node.cnt;
	kv = node.items;
	p = node.ptr;

	/*  Select pointer p[i] and try to insert key in the subtree of whichp[i] is the root:  */
	i = get(key, kv, *count);
	if (i < *count && !strcmp(key, kv[i].key))
		return DUPLICATEKEY;
	code = insert(index, key, key_size, valset, p[i], &keynew_r, &keynew_r_size, &valsetnew_r, &offsetnew_r);
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
		flush_node(index, offset, &node);
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
	memset(&newnode, 0, sizeof(node_t));
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
	*offsetnew = alloc_node(index);
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
	zfree(keynew_r);
	flush_node(index, offset, &node);
	flush_node(index, *offsetnew, &newnode);
	return INSERTNOTCOMPLETE;
}

status_t btree_insert(btree_t *index, char *key, size_t key_size, long long int valset) {
	long int offsetnew, offset;
	char *keynew = NULL;
	size_t key_sizenew;
	long long int valsetnew;
	status_t code = insert(index, key, key_size, valset, index->root, &keynew, &key_sizenew, &valsetnew, &offsetnew);

	if (code == INSERTNOTCOMPLETE) {
		offset = alloc_node(index);
		index->rootnode.cnt = 1;
		strlcpy(index->rootnode.items[0].key, keynew, key_sizenew + 1);
		index->rootnode.items[0].key_size = key_sizenew;
		index->rootnode.items[0].valset = valsetnew;
		index->rootnode.ptr[0] = index->root;
		index->rootnode.ptr[1] = offsetnew;
		index->root = offset;
		zfree(keynew);
		flush_node(index, offset, &index->rootnode);
		code = SUCCESS;
	}
	return code;
}

static status_t delete(btree_t *index, char *key, long int t) {
	int i, j, *n, *nleft, *nright, borrowleft = 0, nq;
	status_t code;
	item_t *kv;
	item_t *item;
	item_t *addr;
	item_t *lkey;
	item_t *rkey;
	long int *p, left, right, *lptr, *rptr, q, q1;
	node_t nod, nod1, nod2, nodL, nodR;
	memset(&nod, 0, sizeof(node_t));
	memset(&nod1, 0, sizeof(node_t));
	memset(&nod2, 0, sizeof(node_t));

	if (t == -1)
		return NOTFOUND;
	get_node(index, t, &nod);
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
		flush_node(index, t, &nod);
		return *n >= (t == index->root ? 1 : INDEX_MSIZE) ? SUCCESS : UNDERFLOW;
	}

	/*  t is an interior node (not a leaf): */
	item = kv + i;
	left = p[i];
	get_node(index, left, &nod1);
	nleft = & nod1.cnt;
	/* key found in interior node.  Go to left child *p[i] and then follow a

	  path all the way to a leaf, using rightmost branches:  */
	if (i < *n && !strcmp(key, item->key)) {
		size_t key_size = item->key_size;
		q = p[i];
		get_node(index, q, &nod1);
		nq = nod1.cnt;
		while (q1 = nod1.ptr[nq], q1 != -1) {
			q = q1;
			get_node(index, q, &nod1);
			nq = nod1.cnt;
		}

		/*  Exchange k[i] with the rightmost item in that leaf:   */
		addr = nod1.items + nq - 1;
		*item = *addr;
		strlcpy(addr->key, key, key_size + 1);
		flush_node(index, t, &nod);
		flush_node(index, q, &nod1);
	}

	/*  Delete key in subtree with root p[i]:  */
	code = delete(index, key, left);
	if (code != UNDERFLOW)
		return code;

	/*  Underflow, borrow, and , if necessary, merge:  */
	if (i < *n)
		get_node(index, p[i + 1], &nodR);
	if (i == *n || nodR.cnt == INDEX_MSIZE) {
		if (i > 0) {
			get_node(index, p[i - 1], &nodL);
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
		get_node(index, right, &nod2);
		nleft = & nod1.cnt;
	} else {
		right = p[i + 1];      /*  borrow from right sibling   */
		get_node(index, left, &nod1);
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
			flush_node(index, t, &nod);
			flush_node(index, left, &nod1);
			flush_node(index, right, &nod2);
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
			flush_node(index, t, &nod);
			flush_node(index, left, &nod1);
			flush_node(index, right, &nod2);
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
	free_node(index, right);
	for (j = i + 1; j < *n; j++) {
		kv[j - 1] = kv[j];
		p[j] = p[j + 1];
	}
	--*n;
	flush_node(index, t, &nod);
	flush_node(index, left, &nod1);

	return *n >= (t == index->root ? 1 : INDEX_MSIZE) ? SUCCESS : UNDERFLOW;
}

status_t btree_delete(btree_t *index, char *key) {
	long int newroot;

	status_t code = delete(index, key, index->root);
	if (code == UNDERFLOW) {
		newroot = index->rootnode.ptr[0];
		free_node(index, index->root);
		if (newroot > 0)
			get_node(index, newroot, &index->rootnode);
		index->root = newroot;
		code = SUCCESS;
	}
	return code;  /* Return value:  SUCCESS  or NOTFOUND   */
}

#if 1
static void print_traversal(btree_t *index, long int offset) {
	static int position = 0;
	int i, n;
	item_t *kv = NULL;
	node_t node;

	if (offset != -1) {
		position += 6;
		get_node(index, offset, &node);
		kv = node.items;
		n = node.cnt;
		printf("%*s", position, "");
		for (i = 0; i < n; i++)
			printf(" %.*s[%d][%llu]", kv[i].key_size, kv[i].key, kv[i].key_size, kv[i].valset);
		putchar('\n');
		for (i = 0; i <= n; i++)
			print_traversal(index, node.ptr[i]);
		position -= 6;
	}
}

void btree_print(btree_t *index) {
	print_traversal(index, index->root);
}
#endif
