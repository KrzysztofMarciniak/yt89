/* yt89 - tiny C89 YouTube search/play frontend
 *
 * build: cc -std=c89 -pedantic -O2 -o yt89 yt89.c
 *
 * env:   HTTP_CLIENT  curl | wget          (default: curl)
 *        VIDEO_PLAYER                      (default: mpv)
 *        YT_DOWNLOADER                     (default: yt-dlp)
 *
 * usage: yt89 --search "cool videos"
 *        yt89 --play <url|id>
 */
#define _POSIX_C_SOURCE 2
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_RESULTS 20
#define UA "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 Chrome/120.0 Safari/537.36"
#define COOKIE "Cookie: CONSENT=PENDING+999; SOCS=CAI"

static const char *env(const char *name, const char *def)
{
	const char *v = getenv(name);
	return (v && *v) ? v : def;
}

static void die(const char *msg)
{
	fprintf(stderr, "yt89: %s\n", msg);
	exit(1);
}

static char *fetch(const char *cmd, size_t *outlen)
{
	FILE *p;
	char *buf;
	size_t len = 0, cap = 1 << 16, n;

	p = popen(cmd, "r");
	if (!p)
		die("popen failed");
	buf = (char *)malloc(cap);
	if (!buf)
		die("out of memory");
	while ((n = fread(buf + len, 1, cap - len - 1, p)) > 0) {
		len += n;
		if (cap - len < 2) {
			cap *= 2;
			buf = (char *)realloc(buf, cap);
			if (!buf)
				die("out of memory");
		}
	}
	pclose(p);
	buf[len] = '\0';
	*outlen = len;
	return buf;
}

static void urlencode(const char *s, char *out)
{
	static const char hex[] = "0123456789ABCDEF";
	for (; *s; s++) {
		unsigned char c = (unsigned char)*s;
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
			*out++ = (char)c;
		else if (c == ' ')
			*out++ = '+';
		else {
			*out++ = '%';
			*out++ = hex[c >> 4];
			*out++ = hex[c & 15];
		}
	}
	*out = '\0';
}

static char *put_utf8(char *o, unsigned long cp)
{
	if (cp < 0x80) {
		*o++ = (char)cp;
	} else if (cp < 0x800) {
		*o++ = (char)(0xC0 | (cp >> 6));
		*o++ = (char)(0x80 | (cp & 0x3F));
	} else if (cp < 0x10000) {
		*o++ = (char)(0xE0 | (cp >> 12));
		*o++ = (char)(0x80 | ((cp >> 6) & 0x3F));
		*o++ = (char)(0x80 | (cp & 0x3F));
	} else {
		*o++ = (char)(0xF0 | (cp >> 18));
		*o++ = (char)(0x80 | ((cp >> 12) & 0x3F));
		*o++ = (char)(0x80 | ((cp >> 6) & 0x3F));
		*o++ = (char)(0x80 | (cp & 0x3F));
	}
	return o;
}

/* in-place JSON string unescape */
static void unescape(char *s)
{
	char *o = s;
	while (*s) {
		if (*s != '\\') {
			*o++ = *s++;
			continue;
		}
		s++;
		switch (*s) {
		case 'n': case 'r': case 't': *o++ = ' '; s++; break;
		case 'u': {
			unsigned long cp = 0, lo;
			char h[5];
			h[4] = '\0';
			memcpy(h, s + 1, 4);
			cp = strtoul(h, NULL, 16);
			s += 5;
			if (cp >= 0xD800 && cp < 0xDC00 && s[0] == '\\' && s[1] == 'u') {
				memcpy(h, s + 2, 4);
				lo = strtoul(h, NULL, 16);
				cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
				s += 6;
			}
			o = put_utf8(o, cp);
			break;
		}
		case '\0': break;
		default: *o++ = *s++; break;
		}
	}
	*o = '\0';
}

/* find key in s, copy JSON string value after it into out */
static int get_str(const char *s, const char *key, char *out, size_t n)
{
	const char *p = strstr(s, key);
	size_t i = 0;

	if (!p)
		return 0;
	p += strlen(key);
	while (*p && *p != '"' && i + 2 < n) {
		if (*p == '\\' && p[1]) {
			out[i++] = *p++;
		}
		out[i++] = *p++;
	}
	out[i] = '\0';
	unescape(out);
	return 1;
}

static void search(const char *query)
{
	static const char marker[] = "\"videoRenderer\":{\"videoId\":\"";
	char enc[1024], url[1300], cmd[2048], id[16], title[512], views[128];
	char seen[MAX_RESULTS][16];
	const char *client = env("HTTP_CLIENT", "curl");
	char *buf, *p, *next, *end, save;
	size_t len, mlen = sizeof(marker) - 1;
	int count = 0, i, dup;

	if (strlen(query) > 300)
		die("query too long");
	urlencode(query, enc);
	sprintf(url, "https://www.youtube.com/results?hl=en&search_query=%s&sp=EgIQAQ%%253D%%253D", enc);

	if (strstr(client, "wget"))
		sprintf(cmd, "%s -qO- -U '%s' --header='%s' '%s'", client, UA, COOKIE, url);
	else
		sprintf(cmd, "%s -sL -A '%s' -H '%s' '%s'", client, UA, COOKIE, url);

	buf = fetch(cmd, &len);
	if (len == 0)
		die("empty response");

	p = buf;
	while (count < MAX_RESULTS && (p = strstr(p, marker)) != NULL) {
		size_t k = 0;

		p += mlen;
		while (p[k] && p[k] != '"' && k < 15)
			k++;
		memcpy(id, p, k);
		id[k] = '\0';

		next = strstr(p, marker);
		end = next ? next : buf + len;
		save = *end;
		*end = '\0';

		if (!get_str(p, "\"title\":{\"runs\":[{\"text\":\"", title, sizeof(title)))
			strcpy(title, "(no title)");
		if (!get_str(p, "\"viewCountText\":{\"simpleText\":\"", views, sizeof(views)))
			strcpy(views, "n/a");
		*end = save;

		dup = 0;
		for (i = 0; i < count; i++)
			if (strcmp(seen[i], id) == 0)
				dup = 1;
		if (!dup) {
			strcpy(seen[count++], id);
			printf("%s  - https://www.youtube.com/watch?v=%s\n%s\n----\n", title, id, views);
		}
		if (!next)
			break;
		p = next;
	}
	if (count == 0)
		fprintf(stderr, "yt89: no results (blocked or page format changed)\n");
	free(buf);
}

static void play(const char *arg)
{
	char url[512], cmd[1536];
	const char *player = env("VIDEO_PLAYER", "mpv");
	const char *dl = env("YT_DOWNLOADER", "yt-dlp");
	size_t i;

	if (strlen(arg) > 400)
		die("url too long");
	for (i = 0; arg[i]; i++)
		if (arg[i] == '\'' || arg[i] == '\\' || (unsigned char)arg[i] < 32)
			die("bad character in url");

	if (strstr(arg, "://"))
		strcpy(url, arg);
	else
		sprintf(url, "https://www.youtube.com/watch?v=%s", arg);

	sprintf(cmd, "%s -q -f b -o - '%s' | %s -", dl, url, player);
	if (system(cmd) != 0)
		die("playback failed");
}

int main(int argc, char **argv)
{
	if (argc == 3 && strcmp(argv[1], "--search") == 0) {
		search(argv[2]);
		return 0;
	}
	if (argc == 3 && strcmp(argv[1], "--play") == 0) {
		play(argv[2]);
		return 0;
	}
	fprintf(stderr, "usage: yt89 --search \"query\" | --play <url|id>\n");
	return 1;
}
