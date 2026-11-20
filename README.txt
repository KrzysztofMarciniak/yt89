yt89 tiny C89 YouTube search/play frontend

BUILD
  make
  make install            (PREFIX=/usr/local by default)

USAGE
  yt89 --search "cat videos"
  yt89 --play <url|video id>

SEARCH OUTPUT
  title  - url
  views
  ----

ENVIRONMENT
  HTTP_CLIENT     curl or wget          (default: curl)
  VIDEO_PLAYER    player reading stdin  (default: mpv)
  YT_DOWNLOADER   yt-dlp compatible     (default: yt-dlp)

NOTES
  --play runs: YT_DOWNLOADER -q -f b -o - URL | VIDEO_PLAYER -
  Search scrapes YouTube's embedded JSON; it breaks if the page format changes.
  Needs a POSIX shell, popen() and system().
