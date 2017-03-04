/*
 * listfs - Barebones FUSE driver for presenting a list of existing filesystem objects
 * Copyright 2017 0x09.net.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <fuse/fuse.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/xattr.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/uio.h>

struct btree {
	char* name;
	size_t len;
	struct btree* links;
};

struct dir_context {
	char* prefix;
	size_t plen;
	struct btree* base;
};

static int listfs_open(const char* path, struct fuse_file_info* info) {
	int fd = open(path, O_RDONLY);
	if(fd < 0)
		return -errno;

	info->fh = fd;
	info->keep_cache = 1;
	return 0;
}

static int listfs_release(const char* path, struct fuse_file_info* info) {
	close(info->fh);
	return 0;
}

static int listfs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* info) {
	int ret = pread(info->fh, buf, size, offset);
	if(ret < 0)
		return -errno;
	return ret;
}

static int listfs_readlink(const char* path, char* buf, size_t size) {
	int ret = readlink(path,buf,size);
	if(ret < 0)
		return -errno;
	return ret;
}

static int listfs_getattr(const char* path, struct stat* st) {
	int ret = stat(path, st);
	if(ret < 0)
		return -errno;
	return ret;
}

static int listfs_fgetattr(const char* path, struct stat* st, struct fuse_file_info* info) {
	int ret = fstat(info->fh, st);
	if(ret < 0)
		return -errno;
	return ret;
}

static int listfs_opendir(const char* path, struct fuse_file_info* info) {
	int ret = 0;
	char* p = strdup(path),* freeme = p;
	if(!p) {
		ret = -ENOMEM;
		goto end;
	}

	char* token;
	struct btree* base = fuse_get_context()->private_data;
	while((token = strsep(&p, "/"))) {
		if(!*token)
			continue;

		size_t i;
		for(i = 0; i < base->len && strcmp(token, base->links[i].name); i++)
			;
		if(i == base->len) {
			ret = -ENOENT;
			goto end;
		}
		base = base->links + i;
	}

	struct dir_context* d = malloc(sizeof(*d));
	d->prefix = strdup(path);
	d->plen = strlen(d->prefix);
	d->base = base;
	info->fh = (uint64_t)d;
end:
	free(freeme);
	return ret;
}

static int listfs_releasedir(const char* path, struct fuse_file_info* info) {
	free((struct dir_context*)info->fh);
	return 0;
}

static int listfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* info) {
	int ret = 0;
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	struct dir_context* ctx = (struct dir_context*)info->fh;
	for(size_t i = 0; i < ctx->base->len; i++) {
		struct stat st;
		size_t namelen = strlen(ctx->base->links[i].name);
		char* p = malloc(ctx->plen + 1 + namelen);
		memcpy(p, ctx->prefix, ctx->plen);
		p[ctx->plen] = '/';
		memcpy(p+ctx->plen+1, ctx->base->links[i].name, namelen+1);
		if(stat(p, &st) == ENOENT) {
			free(p);
			continue;
		}
		if(filler(buf, p, &st, 0)) {
			free(p);
			ret = -errno;
			break;
		}
		free(p);
	}
	return ret;
}

static int listfs_statfs(const char* path, struct statvfs* st) {
	int ret = statvfs(path, st);
	if(ret < 0)
		return -errno;
	return ret;
}

#ifdef __APPLE__
static int listfs_getxtimes(const char* path, struct timespec* bkuptime, struct timespec* crtime) {
	struct stat st;
	int ret = stat(path, &st);
	crtime->tv_sec = st.st_birthtime;
	crtime->tv_nsec = 0;
	if(ret < 0)
		return -errno;
	return 0;
}
#endif

static int listfs_listxattr(const char* path, char* attr, size_t size) {
	int ret = listxattr(path, attr, size, 0);
	if(ret < 0)
		return -errno;
	return 0;
}

static int listfs_getxattr(const char* path, const char* attr, char* value, size_t size, uint32_t offset) {
	int ret = getxattr(path, attr, value, size, offset, 0);
	if(ret < 0)
		return -errno;
	return 0;
}

static struct fuse_operations listfs_ops = {
	.open        = listfs_open,
	.opendir     = listfs_opendir,
	.read        = listfs_read,
	.readdir     = listfs_readdir,
	.release     = listfs_release,
	.releasedir  = listfs_releasedir,
	.statfs      = listfs_statfs,
	.getattr     = listfs_getattr,
	.readlink    = listfs_readlink,
	.fgetattr    = listfs_fgetattr,
	.listxattr   = listfs_listxattr,
	.getxattr    = listfs_getxattr,
#ifdef __APPLE__
	.getxtimes   = listfs_getxtimes,
#else
	.flag_nopath = 1,
	.flag_nullpath_ok = 1
#endif
};

int main(int argc, char* argv[]) {
	// boilerplate from hfsfuse
	// cheat a lot with option parsing to stay within the fuse_main high level API
	if(argc < 3 || !strcmp(argv[1],"-h")) {
		fuse_main(2,((char*[]){"listfs","-h"}),NULL,NULL);
		return 0;
	}

	const char opts[] = "-oro,allow_other,use_ino,subtype=list,fsname=";
	const char* device = argv[argc-2];
	char* mount = argv[argc-1];
	char* argv2[argc+2];
	argv2[0] = "listfs";
	argv2[1] = mount;
	argv2[2] = strcat(strcpy(malloc(strlen(opts)+strlen(device)+1),opts),device);
	argv2[3] = "-s";
	for(int i = 1; i < argc-2; i++)
		argv2[i+3] = argv[i];
	argv2[argc+1] = NULL;

	struct btree root = {"/"};
	FILE* f = fopen(device, "r");
	ssize_t len;
	char* entry = NULL;
	while((len = getline(&entry, &(size_t){0}, f)) > 0) {
		if(entry[len-1] == '\n')
			entry[len-1] = '\0';

		char* token;
		struct btree* base = &root;
		while((token = strsep(&entry, "/"))) {
			if(!*token)
				continue;

			size_t i;
			for(i = 0; i < base->len && strcmp(token, base->links[i].name); i++)
				;
			if(i == base->len) {
				base->links = realloc(base->links, sizeof(struct btree) * ++(base->len));
				memset(base->links + base->len-1, 0, sizeof(struct btree));
				base->links[base->len-1].name = token; // hold onto these since entry won't be freed until exit
			}
			base = base->links + i;
		}
		entry = NULL;
	}
	fclose(f);

	int ret = fuse_main(sizeof(argv2)/sizeof(*argv2)-1,argv2,&listfs_ops,&root);

	return ret;
}
