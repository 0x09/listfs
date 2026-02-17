/*
 * listfs - Barebones FUSE driver for presenting a list of existing filesystem objects
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
	if(!p)
		return -ENOMEM;

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
			break;
		}
		base = base->links + i;
	}

	info->fh = (uint64_t)base;

	free(freeme);
	return ret;
}

static int listfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* info) {
	int ret = 0;
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	struct btree* base = (struct btree*)info->fh;
	for(size_t i = 0; i < base->len; i++) {
		if(filler(buf, base->links[i].name, NULL, 0)) {
			ret = -errno;
			break;
		}
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
#ifdef __APPLE__
	int ret = listxattr(path, attr, size, 0);
#else
	int ret = listxattr(path, attr, size);
#endif
	if(ret < 0)
		return -errno;
	return 0;
}

#ifdef __APPLE__
static int listfs_getxattr(const char* path, const char* attr, char* value, size_t size, uint32_t offset) {
	int ret = getxattr(path, attr, value, size, offset, 0);
	if(ret < 0)
		return -errno;
	return 0;
}
#else
static int listfs_getxattr(const char* path, const char* attr, char* value, size_t size) {
	int ret = getxattr(path, attr, value, size);
	if(ret < 0)
		return -errno;
	return 0;
}
#endif

static struct fuse_operations listfs_ops = {
	.open        = listfs_open,
	.opendir     = listfs_opendir,
	.read        = listfs_read,
	.readdir     = listfs_readdir,
	.release     = listfs_release,
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


static struct fuse_opt listfs_opts[] = {
	FUSE_OPT_KEY("-h",0),
	FUSE_OPT_KEY("--help",0),
	FUSE_OPT_END
};

static int listfs_opt_proc(void* data, const char* arg, int key, struct fuse_args* args) {
	const char** device = data;
	if(key == FUSE_OPT_KEY_NONOPT && !*device) {
		*device = strdup(arg);
		return 0;
	}
	else if(key == 0) {
		fuse_opt_add_arg(args,"-h");
		fuse_main(args->argc,args->argv,NULL,NULL);
		fuse_opt_free_args(args);
		exit(0);
	}
	return 1;
}

int main(int argc, char* argv[]) {
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	const char* device = NULL;

	if(fuse_opt_parse(&args, &device, listfs_opts, listfs_opt_proc) == -1)
		return 1;

	char* fsname = malloc(strlen("fsname=") + strlen(device) + 1);
	if(!fsname)
		goto end;
	strcpy(fsname, "fsname=");
	strcat(fsname, device);

	char* opts = NULL;
	fuse_opt_add_opt(&opts, "ro");
	fuse_opt_add_opt(&opts, "use_ino");
	fuse_opt_add_opt(&opts, "subtype=list");
	fuse_opt_add_opt_escaped(&opts, fsname);
	fuse_opt_add_arg(&args, "-o");
	fuse_opt_add_arg(&args, opts);

	struct btree root = {"/"};
	FILE* f = fopen(device, "r");
	ssize_t len;
	char* entry = NULL;
	int ret;
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
				struct btree* tmp = realloc(base->links, sizeof(struct btree) * ++base->len);
				if(!tmp) {
					ret = 1;
					goto end;
				}
				base->links = tmp;
				base->links[base->len-1].len = 0;
				base->links[base->len-1].links = NULL;
				base->links[base->len-1].name = token; // hold onto these since entry won't be freed until exit
			}
			base = base->links + i;
		}
		entry = NULL;
	}
	fclose(f);

	ret = fuse_main(args.argc,args.argv,&listfs_ops,&root);

end:
	fuse_opt_free_args(&args);

	return ret;
}
