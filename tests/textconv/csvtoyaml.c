//
//  csvtoyaml.c
//  libgit2_clar
//
//  Created by Local Administrator on 12/28/17.
//
#include "clar_libgit2.h"
#include "common.h"
#include "csvtoyaml.h"

enum stream_state {
	INIT,
	ESCAPED,
	QUOTED,
	ESCAPED_QUOTED,
	NORMAL
};

struct yaml_stream {
	git_writestream parent;
	git_writestream *next;
	char linebuf[1024];
	size_t pos;
	enum stream_state state;
};


static int yaml_stream_write(git_writestream *s, const char *buffer, size_t len)
{
	size_t i;
	int err = 0;
	struct yaml_stream *stream = (struct yaml_stream *)s;

	for (i = 0; err == 0 && i < len; i++) {
		char c = buffer[i];
		switch (stream->state) {
			case INIT:
				stream->next->write(stream->next, "-", 1);
				stream->linebuf[stream->pos++] = c;
				stream->state = NORMAL;
				break;
			case ESCAPED:
				stream->linebuf[stream->pos++] = c;
				stream->state = NORMAL;
				break;
			case ESCAPED_QUOTED:
				stream->linebuf[stream->pos++] = c;
				stream->state = QUOTED;
				break;
			case QUOTED:
				switch(c) {
				case '"':
					stream->state = NORMAL;
					break;
				case '\\':
					stream->state = ESCAPED_QUOTED;
					break;
				default:
					stream->linebuf[stream->pos++] = c;
					break;
				};
				break;
			case NORMAL:
				switch(c) {
					case '"':
						stream->state = QUOTED;
						break;
					case '\\':
						stream->state = ESCAPED;
						break;
					case ',':
						err = stream->next->write(stream->next, "\n  - ", 5);
						err = err || stream->next->write(stream->next, stream->linebuf, stream->pos);
						stream->pos = 0;
						break;
					case '\n':
						err = stream->next->write(stream->next, "\n  - ", 5);
						err = err || stream->next->write(stream->next, stream->linebuf, stream->pos);
						err = err || stream->next->write(stream->next, "\n-", 2);
						stream->pos = 0;
						break;
					case '\r':
						break;
					default:
						stream->linebuf[stream->pos++] = c;
						break;
				};
				break;
		}
	}

	return err;
}


static int yaml_stream_close(git_writestream *s)
{
	struct yaml_stream *stream = (struct yaml_stream *)s;
	cl_assert_equal_i(0, stream->pos);
	stream->next->close(stream->next);
	return 0;
}

static void yaml_stream_free(git_writestream *stream)
{
	git__free(stream);
}

static int yaml_stream_init(
	git_writestream **out,
	git_textconv *self,
	git_writestream *next)
{
	struct yaml_stream *stream = git__calloc(1, sizeof(struct yaml_stream));
	cl_assert(stream);

	GIT_UNUSED(self);

	stream->parent.write = yaml_stream_write;
	stream->parent.close = yaml_stream_close;
	stream->parent.free = yaml_stream_free;
	stream->next = next;
	stream->state = INIT;
	stream->pos = 0;

	*out = (git_writestream *)stream;
	return 0;
}

git_textconv *create_csv_to_yaml_textconv(void)
{
	git_textconv *textconv = git__calloc(1, sizeof(git_textconv));
	cl_assert(textconv);

	textconv->version = GIT_TEXTCONV_VERSION;
	textconv->stream = yaml_stream_init;

	return textconv;
}
