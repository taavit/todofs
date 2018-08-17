#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <json-c/json_object.h>
#include <json-c/json_tokener.h>
#include <json-c/json_util.h>
#include <fuse.h>
#include <linux/limits.h>

#define PACKAGE_VERSION "0.1"

struct Todo {
	unsigned int userId;
	unsigned int id;
	const char *title;
	bool completed;
};

struct TodoCollection {
	struct Todo *collection;
	size_t length;
};

static const char*
read_title(json_object *entry)
{
	json_object *title;
	json_object_object_get_ex(entry, "title", &title);
	return json_object_get_string(title);
}

static const unsigned long int
read_id(json_object *entry)
{
	json_object *id;
	json_object_object_get_ex(entry, "id", &id);
	return json_object_get_int(id);
}

static const unsigned long int
read_user_id(json_object *entry)
{
	json_object *id;
	json_object_object_get_ex(entry, "userId", &id);
	return json_object_get_int(id);
}

static const bool
read_completed(json_object *entry)
{
	json_object *id;
	json_object_object_get_ex(entry, "completed", &id);
	return json_object_get_boolean(id);
}

struct Todo
read_todo_from_json_object(json_object *object)
{
	struct Todo tmp = {
		.title = read_title(object),
		.completed = read_completed(object),
		.id = read_id(object),
		.userId = read_user_id(object),
	};

	return tmp;
}

void
create_filename(const struct Todo *todo, char *filename)
{
	sprintf(filename, "%u.txt", todo->id);
}

void
print_all_todos(struct TodoCollection collection)
{
	for (size_t i = 0; i<collection.length; i++) {
		char filename[PATH_MAX];
		create_filename(&collection.collection[i], filename),
		printf(
			"%u(%s)\t:%s [%s]\n",
			collection.collection[i].id,
			filename,
			collection.collection[i].title,
			collection.collection[i].completed
			? "DONE"
			: "TODO"
		);
	}
}

static struct TodoCollection collection = {
	.collection = NULL,
	.length = 0,
};

static int
todofs_getdir(const char *path, fuse_dirh_t h, fuse_dirfil_t filler)
{
	if (strcmp(path, "/") != 0) {
		return -ENOENT;
	}
	filler(h, ".", 0);
	filler(h, "..", 0);
	for (size_t i = 0; i<collection.length; i++) {
		char filename[PATH_MAX];
		create_filename(&collection.collection[i], filename);
		filler(h, filename, 0);
	}
	return 0;
}

static int
todofs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	unsigned int id;
	int ret = sscanf(path, "/%u.txt", &id);
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	}
	else if (ret != 0) {
		size_t i;
		for (i=0; i < collection.length; i++) {
			if (collection.collection[i].id == id) {
				break;
			}
		}

		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(collection.collection[i].title);
		res = 0;
	}
	else {
		res = -ENOENT;
	}
	return res;
}

static int
todofs_open(const char *path, int flags)
{
	unsigned int id;
	int ret = sscanf(path, "/%u.txt", &id);
	if (ret != 0) {
		if ((flags & 3) != O_RDONLY) {
			return -EACCES;
		}
		return 0;
	}

	return -ENOENT;
}

static int
todofs_read(const char *path, char *buf, size_t size, off_t offset)
{
	unsigned int id;
	size_t len;
	sscanf(path, "/%u.txt", &id);
	size_t i;
	for (i=0; i < collection.length; i++) {
		if (collection.collection[i].id == id) {
			break;
		}
	}

	len = strlen(collection.collection[i].title);
	if (offset < len) {
		if (offset + size > len) {
			size = len - offset;
		}
		memcpy(buf, collection.collection[i].title, size);
	} else {
		size = 0;
	}
	return size;
}

static struct fuse_operations todofs_oper = {
	.getdir 	= todofs_getdir,
	.getattr	= todofs_getattr,
	.open		= todofs_open,
	.read		= todofs_read,
};

struct todofs_config {
	char *filename;
};

enum {
	KEY_HELP,
	KEY_VERSION,
};

#define TODOFS_OPT(t, p, v) { t, offsetof(struct todofs_config, p), v }

static struct fuse_opt todofs_opts[] = {
	TODOFS_OPT("filename=%s",	filename,	0),

	FUSE_OPT_KEY("-V",		KEY_VERSION),
	FUSE_OPT_KEY("--version",	KEY_VERSION),
	FUSE_OPT_KEY("-h",		KEY_HELP),
	FUSE_OPT_KEY("--help",		KEY_HELP),
	FUSE_OPT_END
};

static int
todofs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
	switch (key) {
	case KEY_HELP:
		fprintf(stderr,
			"usage: %s mountpoint [options]\n"
			"\n"
			"general options:\n"
			"	-o opt,[opt...] mount options\n"
			"	-h --help 	print help\n"
			"	-V --version 	print version\n"
			"\n"
			"TodoFS options\n"
			"	-o filename=FILENAME.json\n"
			, outargs->argv[0]
		);
		fuse_opt_add_arg(outargs, "-ho");
		fuse_main(outargs->argc, outargs->argv, &todofs_oper);
		exit(1);
	case KEY_VERSION:
		fprintf(stderr, "TodoFS version %s\n", PACKAGE_VERSION);
		fuse_opt_add_arg(outargs, "--version");
		fuse_main(outargs->argc, outargs->argv, &todofs_oper);
		exit(0);
	}
	return 1;
}

int
main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	struct todofs_config conf;
	int ret = 0;
	memset(&conf, 0, sizeof(conf));
	fuse_opt_parse(&args, &conf, todofs_opts, todofs_opt_proc);
	struct json_object *jsonTodos = json_object_from_file(conf.filename);
	size_t jsonLength = json_object_array_length(jsonTodos);
	collection.collection = realloc(collection.collection, sizeof(struct Todo) * jsonLength);
	collection.length = jsonLength;
	for (size_t i=0; i<jsonLength; i++) {
		collection.collection[i] = read_todo_from_json_object(json_object_array_get_idx(jsonTodos, i));
	}
	ret = fuse_main(args.argc, args.argv, &todofs_oper);
	free(collection.collection);
	json_object_put(jsonTodos);
	return ret;
}

