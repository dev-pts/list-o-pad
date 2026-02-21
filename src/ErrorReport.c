static void report_error(const char *filename, const char *string, size_t len, struct LOP_Location loc, const char *err_string)
{
	assert(loc.line_offset < len);

	fprintf(stderr, "%s in file '%s' at %i:%i\n", err_string, filename, loc.lineno, loc.charno + 1);

	for (size_t i = loc.line_offset; i < len; i++) {
		const char *p = &string[i];

		if (*p == 0 || *p == '\n') {
			break;
		}

		fprintf(stderr, "%c", *p);
	}

	fprintf(stderr, "\n");

	{
		for (size_t i = loc.line_offset; i < len && (i - loc.line_offset < loc.charno); i++) {
			const char *p = &string[i];

			if (isspace(*p)) {
				fprintf(stderr, "%c", *p);
			} else {
				fprintf(stderr, " ");
			}
		}
	}

	fprintf(stderr, "^\n");
}
