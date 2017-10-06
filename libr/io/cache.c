/* radare - LGPL - Copyright 2008-2017 - pancake */

// TODO: implement a more inteligent way to store cached memory
// TODO: define limit of max mem to cache

#include "r_io.h"

static void cache_item_free(RIOCache *cache) {
	if (!cache)
		return;
	free (cache->data);
	free (cache->odata);
	free (cache);
}

R_API void r_io_cache_init(RIO *io) {
	io->cache = r_list_newf ((RListFree)cache_item_free);
	io->cached = 0;
}

R_API void r_io_cache_commit(RIO *io, ut64 from, ut64 to) {
	RListIter *iter;
	RIOCache *c;
	r_list_foreach (io->cache, iter, c) {
		if (from <= c->to - 1 && c->from <= to - 1) {
			int cached = io->cached;
			io->cached = 0;
			if (r_io_write_at (io, c->from, c->data, c->size)) {
				c->written = true;
			} else {
				eprintf ("Error writing change at 0x%08"PFMT64x"\n", c->from);
			}
			io->cached = cached;
			break; // XXX old behavior, revisit this
		}
	}
}

R_API void r_io_cache_reset(RIO *io, int set) {
	io->cached = set;
	r_list_purge (io->cache);
}

R_API int r_io_cache_invalidate(RIO *io, ut64 from, ut64 to) {
	RListIter *iter;
	RIOCache *c;
	int done = false;

	if (from<to) {
		//r_list_foreach_safe (io->cache, iter, iter_tmp, c) {
		r_list_foreach (io->cache, iter, c) {
			if (c->from >= from && c->to <= to) {
				int cached = io->cached;
				io->cached = 0;
				r_io_write_at (io, c->from, c->odata, c->size);
				io->cached = cached;
				if (!c->written)
					r_list_delete (io->cache, iter);
				c->written = false;
				done = true;
				break;
			}
		}
	}
	return done;
}

R_API int r_io_cache_list(RIO *io, int rad) {
	int i, j = 0;
	RListIter *iter;
	RIOCache *c;
	if (rad == 2) {
		io->cb_printf ("[");
	}
	r_list_foreach (io->cache, iter, c) {
		if (rad == 1) {
			io->cb_printf ("wx ");
			for (i = 0; i < c->size; i++) {
				io->cb_printf ("%02x", (ut8)(c->data[i] & 0xff));
			}
			io->cb_printf (" @ 0x%08"PFMT64x, c->from);
			io->cb_printf (" # replaces: ");
		  	for (i = 0; i < c->size; i++) {
				io->cb_printf ("%02x", (ut8)(c->odata[i] & 0xff));
			}
			io->cb_printf ("\n");
		} else if (rad == 2) {
			io->cb_printf ("{\"idx\":%"PFMT64d",\"addr\":%"PFMT64d",\"size\":%d,", j, c->from, c->size);
			io->cb_printf ("\"before\":\"");
		  	for (i = 0; i < c->size; i++) {
				io->cb_printf ("%02x", c->odata[i]);
			}
			io->cb_printf ("\",\"after\":\"");
		  	for (i = 0; i < c->size; i++) {
				io->cb_printf ("%02x", c->data[i]);
			}
			io->cb_printf ("\",\"written\":%s}%s", c->written? "true": "false",iter->n? ",": "");
		} else if (rad == 0) {
			io->cb_printf ("idx=%d addr=0x%08"PFMT64x" size=%d ", j, c->from, c->size);
			for (i = 0; i < c->size; i++) {
				io->cb_printf ("%02x", c->odata[i]);
			}
			io->cb_printf (" -> ");
			for (i = 0; i < c->size; i++) {
				io->cb_printf ("%02x", c->data[i]);
			}
			io->cb_printf (" %s\n", c->written? "(written)": "(not written)");
		}
		j++;
	}
	if (rad == 2)
		io->cb_printf ("]");
	return false;
}

R_API bool r_io_cache_write(RIO *io, ut64 addr, const ut8 *buf, int len) {
	RIOCache *ch;
	ch = R_NEW0 (RIOCache);
	if (!ch) {
		return false;
	}
	ch->from = addr;
	ch->to = addr + len;
	ch->size = len;
	ch->odata = (ut8*)calloc (1, len + 1);
	if (!ch->odata) {
		free (ch);
		return false;
	}
	ch->data = (ut8*)calloc (1, len + 1);
	if (!ch->data) {
		free (ch->odata);
		free (ch);
		return false;
	}
	ch->written = false;
	r_io_read_at (io, addr, ch->odata, len);
	memcpy (ch->data, buf, len);
	r_list_append (io->cache, ch);
	return true;
}

R_API bool r_io_cache_read(RIO *io, ut64 addr, ut8 *buf, int len) {
	int l, covered = 0;
	RListIter *iter;
	RIOCache *c;
	r_list_foreach (io->cache, iter, c) {
		if (addr < c->to && c->from < addr + len) {
			if (addr < c->from) {
				l = R_MIN (addr + len - c->from, c->size);
				memcpy (buf + c->from - addr, c->data, l);
			} else {
				l = R_MIN (c->to - addr, len);
				memcpy (buf, c->data + addr - c->from, l);
			}
			covered += l;
		}
	}
	return (covered == 0) ? false: true;
}
