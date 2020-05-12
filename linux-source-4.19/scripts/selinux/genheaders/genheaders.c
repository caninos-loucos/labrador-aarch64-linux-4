// SPDX-License-Identifier: GPL-2.0

/* NOTE: we really do want to use the kernel headers here */
#define __EXPORTED_HEADERS__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

struct security_class_mapping {
	const char *name;
	const char *perms[sizeof(unsigned) * 8 + 1];
};

#include "classmap.h"
#include "initial_sid_to_string.h"

#define max(x, y) (((int)(x) > (int)(y)) ? x : y)

const char *progname;

static void usage(void)
{
	printf("usage: %s flask.h av_permissions.h\n", progname);
	exit(1);
}

static char *stoupperx(const char *s)
{
	char *s2 = strdup(s);
	char *p;

	if (!s2) {
		fprintf(stderr, "%s:  out of memory\n", progname);
		exit(3);
	}

	for (p = s2; *p; p++)
		*p = toupper(*p);
	return s2;
}

int main(int argc, char *argv[])
{
	int i, j, k;
	int isids_len;
	FILE *fout;
	const char *needle = "SOCKET";
	char *substr;

	progname = argv[0];

	if (argc < 3)
		usage();

	fout = fopen(argv[1], "w");
	if (!fout) {
		fprintf(stderr, "Could not open %s for writing:  %s\n",
			argv[1], strerror(errno));
		exit(2);
	}

	for (i = 0; secclass_map[i].name; i++) {
		struct security_class_mapping *map = &secclass_map[i];
		map->name = stoupperx(map->name);
		for (j = 0; map->perms[j]; j++)
			map->perms[j] = stoupperx(map->perms[j]);
	}

	isids_len = sizeof(initial_sid_to_string) / sizeof (char *);
	for (i = 1; i < isids_len; i++)
		initial_sid_to_string[i] = stoupperx(initial_sid_to_string[i]);

	fprintf(fout, "/* This file is automatically generated.  Do not edit. */\n");
	fprintf(fout, "#ifndef _SELINUX_FLASK_H_\n#define _SELINUX_FLASK_H_\n\n");

	for (i = 0; secclass_map[i].name; i++) {
		struct security_class_mapping *map = &secclass_map[i];
		fprintf(fout, "#define SECCLASS_%s", map->name);
		for (j = 0; j < max(1, 40 - strlen(map->name)); j++)
			fprintf(fout, " ");
		fprintf(fout, "%2d\n", i+1);
	}

	fprintf(fout, "\n");

	for (i = 1; i < isids_len; i++) {
		const char *s = initial_sid_to_string[i];
		fprintf(fout, "#define SECINITSID_%s", s);
		for (j = 0; j < max(1, 40 - strlen(s)); j++)
			fprintf(fout, " ");
		fprintf(fout, "%2d\n", i);
	}
	fprintf(fout, "\n#define SECINITSID_NUM %d\n", i-1);
	fprintf(fout, "\nstatic inline bool security_is_socket_class(u16 kern_tclass)\n");
	fprintf(fout, "{\n");
	fprintf(fout, "\tbool sock = false;\n\n");
	fprintf(fout, "\tswitch (kern_tclass) {\n");
	for (i = 0; secclass_map[i].name; i++) {
		struct security_class_mapping *map = &secclass_map[i];
		substr = strstr(map->name, needle);
		if (substr && strcmp(substr, needle) == 0)
			fprintf(fout, "\tcase SECCLASS_%s:\n", map->name);
	}
	fprintf(fout, "\t\tsock = true;\n");
	fprintf(fout, "\t\tbreak;\n");
	fprintf(fout, "\tdefault:\n");
	fprintf(fout, "\t\tbreak;\n");
	fprintf(fout, "\t}\n\n");
	fprintf(fout, "\treturn sock;\n");
	fprintf(fout, "}\n");

	fprintf(fout, "\n#endif\n");
	fclose(fout);

	fout = fopen(argv[2], "w");
	if (!fout) {
		fprintf(stderr, "Could not open %s for writing:  %s\n",
			argv[2], strerror(errno));
		exit(4);
	}

	fprintf(fout, "/* This file is automatically generated.  Do not edit. */\n");
	fprintf(fout, "#ifndef _SELINUX_AV_PERMISSIONS_H_\n#define _SELINUX_AV_PERMISSIONS_H_\n\n");

	for (i = 0; secclass_map[i].name; i++) {
		struct security_class_mapping *map = &secclass_map[i];
		for (j = 0; map->perms[j]; j++) {
			if (j >= 32) {
				fprintf(stderr, "Too many permissions to fit into an access vector at (%s, %s).\n",
					map->name, map->perms[j]);
				exit(5);
			}
			fprintf(fout, "#define %s__%s", map->name,
				map->perms[j]);
			for (k = 0; k < max(1, 40 - strlen(map->name) - strlen(map->perms[j])); k++)
				fprintf(fout, " ");
			fprintf(fout, "0x%08xU\n", (1<<j));
		}
	}

	fprintf(fout, "\n#endif\n");
	fclose(fout);
	exit(0);
}
