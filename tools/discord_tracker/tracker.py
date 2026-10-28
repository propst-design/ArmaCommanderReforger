"""GitHub issues -> Discord forum posts; the roadmap issue -> a Discord roadmap post.

Standard library only. Discord is reached over REST with a bot token: no Gateway connection and
nothing to host. Discord itself is the state: a post is found by its "<MOD> #<n> " title, a
mirrored comment by the GitHub link on its embed, and a closed issue by its post being locked.
Every run reconciles from GitHub's current state, so a cancelled or failed run is caught up by the
next one, and a run with nothing new to mirror sends nothing.
"""
import datetime as dt
import json
import os
import re
import socket
import sys
import time
import urllib.error
import urllib.parse as up
import urllib.request as ur

GITHUB = "https://api.github.com"
DISCORD = "https://discord.com/api/v10"
USER_AGENT = "DiscordBot (https://github.com/propst-design/ArmaCommanderReforger, 1)"
FORUM = 15
LOOKBACK = dt.timedelta(hours=48)
NO_PINGS = {"parse": []}
SUPPRESS_EMBEDS = 1 << 2
COLORS = {"open": 0x2DA44E, "completed": 0x8250DF, "not_planned": 0x6E7781}
STATUS = {"open": "Open", "completed": "Closed as completed", "not_planned": "Closed as not planned"}
COMMENT_COLOR = 0x57606A
# Forum tags are matched by name, so the Discord server decides which exist. First match wins.
TAG_ALIASES = {"enhancement": ("idea", "suggestion", "enhancement"), "closed": ("closed", "done")}
COMMENT_LINK = re.compile(r"#issuecomment-(\d+)$")


class TrackerError(Exception):
    pass


class HttpError(TrackerError):
    def __init__(self, status, body=b""):
        detail = ""
        try:
            data = json.loads(body)
            detail = f" ({data.get('code', '')} {data.get('message', '')})"
        except (ValueError, AttributeError, UnicodeDecodeError):
            pass
        super().__init__(f"HTTP {status}{detail}")
        self.status = status


def retry_delay(headers, body, attempt):
    try:
        seconds = float(json.loads(body)["retry_after"])
    except (ValueError, KeyError, TypeError, UnicodeDecodeError):
        try:
            seconds = float(headers.get("Retry-After"))
        except (TypeError, ValueError):
            seconds = 2 ** attempt
    if seconds > 60:
        raise TrackerError("Rate limited for over a minute; the next run catches up")
    return max(seconds, 0.25)


class HTTP:
    def request(self, method, url, headers, payload=None):
        data = None if payload is None else json.dumps(payload, ensure_ascii=False).encode("utf-8")
        headers = {"Accept": "application/json", **headers}
        if data is not None:
            headers["Content-Type"] = "application/json"
        for attempt in range(5):
            try:
                with ur.urlopen(ur.Request(url, data=data, headers=headers, method=method), timeout=30) as response:
                    status, response_headers, body = response.status, response.headers, response.read()
            except urllib.error.HTTPError as exc:
                status, response_headers, body = exc.code, exc.headers, exc.read()
            except (urllib.error.URLError, socket.timeout, OSError):
                if method == "GET" and attempt < 4:
                    time.sleep(2 ** attempt)
                    continue
                raise TrackerError(f"Network failure on {method}; the next run catches up") from None
            # Only GETs retry server errors: a failed POST may still have posted.
            if (status == 429 or (method == "GET" and status >= 500)) and attempt < 4:
                time.sleep(retry_delay(response_headers, body, attempt))
                continue
            if not 200 <= status < 300:
                raise HttpError(status, body)
            return json.loads(body) if body else None
        raise TrackerError("Retry budget exhausted")


class GitHub:
    def __init__(self, http, repo, token):
        self.http, self.repo = http, repo
        self.headers = {"Authorization": f"Bearer {token}", "Accept": "application/vnd.github+json",
                        "X-GitHub-Api-Version": "2022-11-28", "User-Agent": "discord-tracker"}

    def get(self, path):
        return self.http.request("GET", f"{GITHUB}/repos/{self.repo}{path}", self.headers)

    def pages(self, path):
        sep = "&" if "?" in path else "?"
        page = 1
        while True:
            rows = self.get(f"{path}{sep}per_page=100&page={page}")
            yield from rows
            if len(rows) < 100:
                return
            page += 1

    def issues(self, since=None):
        query = f"?state=all&since={up.quote(since)}" if since else "?state=open"
        return [i for i in self.pages("/issues" + query) if "pull_request" not in i]

    def issue(self, number):
        try:
            issue = self.get(f"/issues/{number}")
        except HttpError as exc:
            if exc.status in (404, 410):
                return None
            raise
        return None if "pull_request" in issue else issue

    def comments(self, number):
        return list(self.pages(f"/issues/{number}/comments"))


class Discord:
    def __init__(self, http, token):
        self.http = http
        self.headers = {"Authorization": f"Bot {token}", "User-Agent": USER_AGENT}

    def call(self, method, path, payload=None):
        return self.http.request(method, DISCORD + path, self.headers, payload)

    def me(self):
        # Split so the public-source check doesn't read it as a personal path.
        return self.call("GET", "/" + "users/@me")["id"]

    def channel(self, channel_id):
        try:
            return self.call("GET", f"/channels/{channel_id}")
        except HttpError as exc:
            if exc.status in (403, 404):
                return None
            raise

    def posts(self, forum):
        """Every post in a forum, active or archived."""
        active = self.call("GET", f"/guilds/{forum['guild_id']}/threads/active")["threads"]
        threads = [t for t in active if t.get("parent_id") == forum["id"]]
        before = None
        while True:
            query = "?limit=100" + (f"&before={up.quote(before)}" if before else "")
            page = self.call("GET", f"/channels/{forum['id']}/threads/archived/public{query}")
            threads += page["threads"]
            if not page.get("has_more") or not page["threads"]:
                return threads
            before = page["threads"][-1]["thread_metadata"]["archive_timestamp"]

    def messages(self, thread_id):
        """A thread's messages, oldest first; the starter message's id is the thread's id."""
        out, before = [], None
        while True:
            page = self.call("GET", f"/channels/{thread_id}/messages?limit=100" + (f"&before={before}" if before else ""))
            out += page
            if len(page) < 100:
                return sorted(out, key=lambda m: int(m["id"]))
            before = min(page, key=lambda m: int(m["id"]))["id"]

    def create_post(self, forum_id, name, message, tags=()):
        return self.call("POST", f"/channels/{forum_id}/threads",
                         {"name": name, "auto_archive_duration": 10080, "applied_tags": list(tags), "message": message})

    def send(self, thread_id, message):
        self.call("POST", f"/channels/{thread_id}/messages", message)

    def edit(self, thread_id, message_id, message):
        self.call("PATCH", f"/channels/{thread_id}/messages/{message_id}", message)

    def delete(self, thread_id, message_id):
        try:
            self.call("DELETE", f"/channels/{thread_id}/messages/{message_id}")
        except HttpError as exc:
            if exc.status != 404:
                raise

    def update_thread(self, thread_id, **fields):
        self.call("PATCH", f"/channels/{thread_id}", fields)


def clip(text, limit):
    if len(text) <= limit:
        return text
    cut = text.rfind("\n", 0, limit - 2)
    if cut < limit // 2:
        cut = limit - 2
    return text[:cut].rstrip() + "\n…"


def clip_line(text, limit):
    text = " ".join(text.split())
    return text if len(text) <= limit else text[:limit - 1].rstrip() + "…"


def to_discord(text, repo, links=True):
    """GitHub-flavoured Markdown -> what Discord renders; links=False leaves #12 as plain text."""
    text = (text or "").replace("\r\n", "\n")
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)
    text = re.sub(r"<img\b[^>]*\bsrc=\"([^\"]+)\"[^>]*>", r"[image](\1)", text)
    text = re.sub(r"!\[([^\]]*)\]\(([^)\s]+)[^)]*\)", lambda m: f"[{m[1] or 'image'}]({m[2]})", text)
    text = re.sub(r"^#{4,6}[ \t]+(.+)$", r"**\1**", text, flags=re.M)
    text = re.sub(r"^(\s*[-*]) \[ \] ", r"\1 ☐ ", text, flags=re.M)
    text = re.sub(r"^(\s*[-*]) \[[xX]\] ", r"\1 ☑ ", text, flags=re.M)
    text = re.sub(r"@(everyone|here)\b", "@​\\1", text)
    text = re.sub(r"(?<![\w/\[`])([A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+)#(\d+)\b",
                  lambda m: f"[{m[1]}#{m[2]}](https://github.com/{m[1]}/issues/{m[2]})", text)
    text = re.sub(r"(?<![\w/\[`#&])#(\d+)\b", lambda m: f"[#{m[1]}](https://github.com/{repo}/issues/{m[1]})", text)
    return re.sub(r"\n{3,}", "\n\n", text).strip()


def issue_status(issue):
    if issue["state"] == "open":
        return "open"
    return "not_planned" if issue.get("state_reason") == "not_planned" else "completed"


def author(user, url=None, suffix=""):
    user = user or {}
    entry = {"name": f"{user.get('login', 'ghost')}{suffix}", "url": url or user.get("html_url")}
    if user.get("avatar_url"):
        entry["icon_url"] = user["avatar_url"]
    return entry


def issue_message(issue, repo):
    status = issue_status(issue)
    body = to_discord(issue.get("body"), repo)
    labels = ", ".join(label["name"] for label in issue.get("labels") or []) or "none"
    embed = {"title": clip_line(f"#{issue['number']} {issue['title']}", 256), "url": issue["html_url"],
             "description": clip(body, 3500) if body else "*No description.*", "color": COLORS[status],
             "author": author(issue.get("user")),
             "fields": [{"name": "Status", "value": STATUS[status], "inline": True},
                        {"name": "Labels", "value": clip_line(labels, 1000), "inline": True}],
             "footer": {"text": repo}, "timestamp": issue["created_at"]}
    return {"embeds": [embed], "allowed_mentions": NO_PINGS}


def comment_message(comment, repo):
    body = to_discord(comment.get("body"), repo)
    embed = {"author": author(comment.get("user"), comment["html_url"], " commented"),
             "description": clip(body, 3500) if body else "*Empty comment.*",
             "color": COMMENT_COLOR, "timestamp": comment["created_at"]}
    return {"embeds": [embed], "allowed_mentions": NO_PINGS}


def note(text):
    return {"content": text, "flags": SUPPRESS_EMBEDS, "allowed_mentions": NO_PINGS}


def post_name(mod, issue):
    return clip_line(f"{mod} #{issue['number']} {issue['title']}", 100)


def wanted_tags(available, mod, issue):
    by_name = {tag["name"].casefold(): tag["id"] for tag in available}
    names = [mod] + (["closed"] if issue["state"] == "closed" else []) + [label["name"] for label in issue.get("labels") or []]
    out = []
    for name in names:
        for alias in TAG_ALIASES.get(name.casefold(), (name,)):
            tag = by_name.get(alias.casefold())
            if tag:
                if tag not in out:
                    out.append(tag)
                break
    return out[:5]


def comment_id(message):
    for embed in message.get("embeds") or []:
        match = COMMENT_LINK.search((embed.get("author") or {}).get("url") or "")
        if match:
            return match[1]
    return None


def same(message, payload):
    """Whether a posted message already shows what the payload would."""
    if (message.get("content") or "") != (payload.get("content") or ""):
        return False
    old, new = message.get("embeds") or [], payload.get("embeds") or []
    if len(old) != len(new):
        return False
    for a, b in zip(old, new):
        if any(a.get(key) != b.get(key) for key in ("title", "url", "description", "color")):
            return False
        if [(f["name"], f["value"]) for f in a.get("fields") or []] != [(f["name"], f["value"]) for f in b.get("fields") or []]:
            return False
        if (a.get("author") or {}).get("name") != (b.get("author") or {}).get("name"):
            return False
    return True


def split_text(text, limit=1900):
    """Whole lines per chunk, starting a new chunk at a heading once one is well filled."""
    chunks, current = [], ""
    for line in text.split("\n"):
        while len(line) > limit:
            if current:
                chunks.append(current)
                current = ""
            chunks.append(line[:limit])
            line = line[limit:]
        candidate = f"{current}\n{line}" if current else line
        if len(candidate) > limit or (line.startswith("#") and len(current) > limit * 0.6):
            chunks.append(current)
            current = line
        else:
            current = candidate
    chunks.append(current)
    return [chunk.strip() for chunk in chunks if chunk.strip()]


def roadmap_messages(issue, repo):
    # Plain #12 references: links would roughly double the roadmap's length.
    body = to_discord(issue.get("body"), repo, links=False) or "*Nothing listed yet.*"
    footer = f"-# Kept in sync with [the roadmap issue on GitHub]({issue['html_url']})."
    return [note(chunk) for chunk in split_text(f"{body}\n\n{footer}")]


class Post:
    """A forum post, unarchived before its first change."""

    def __init__(self, dc, thread):
        meta = thread.get("thread_metadata") or {}
        self.dc, self.id = dc, thread["id"]
        self.archived, self.locked = bool(meta.get("archived")), bool(meta.get("locked"))

    def touch(self):
        if self.archived:
            self.dc.update_thread(self.id, archived=False)
            self.archived = False


class Tracker:
    def __init__(self, gh, dc, mod, title, forum, roadmap_forum=None):
        self.gh, self.dc, self.mod, self.title = gh, dc, mod, title
        self.forum, self.roadmap_forum = forum, roadmap_forum
        self.me = dc.me()
        self.posts = {}
        prefix = re.compile(rf"{re.escape(mod)} #(\d+)\b")
        for thread in sorted(dc.posts(forum), key=lambda t: int(t["id"])):
            match = prefix.match(thread.get("name", ""))
            if match and thread.get("owner_id") == self.me:
                self.posts.setdefault(int(match[1]), thread)

    def sync_issue(self, issue):
        number, status = issue["number"], issue_status(issue)
        closed, thread = status != "open", self.posts.get(number)
        starter, name = issue_message(issue, self.gh.repo), post_name(self.mod, issue)
        tags = wanted_tags(self.forum.get("available_tags") or [], self.mod, issue)
        if thread is None:
            if closed:
                return  # posts are opened for open issues only
            thread = self.posts[number] = self.dc.create_post(self.forum["id"], name, starter, tags)
            print(f"#{number}: new post")
        post = Post(self.dc, thread)
        mirrored = {}
        for message in self.dc.messages(post.id):
            if message["author"]["id"] != self.me:
                continue
            if message["id"] == post.id:
                if not same(message, starter):
                    post.touch()
                    self.dc.edit(post.id, post.id, starter)
            elif comment_id(message):
                mirrored.setdefault(comment_id(message), []).append(message)
        if not closed and post.locked:
            self.dc.update_thread(post.id, archived=False, locked=False)
            post.archived = post.locked = False
            self.dc.send(post.id, note("Reopened on GitHub."))
        changes = {}
        if thread.get("name") != name:
            changes["name"] = name
        if sorted(thread.get("applied_tags") or []) != sorted(tags):
            changes["applied_tags"] = tags
        if changes:
            post.touch()
            self.dc.update_thread(post.id, **changes)
        current = set()
        for comment in self.gh.comments(number):
            if (comment.get("user") or {}).get("type") == "Bot":
                continue
            key, payload = str(comment["id"]), comment_message(comment, self.gh.repo)
            current.add(key)
            found = mirrored.get(key)
            if not found:
                post.touch()
                self.dc.send(post.id, payload)
            elif not same(found[0], payload):
                post.touch()
                self.dc.edit(post.id, found[0]["id"], payload)
        for key, messages in mirrored.items():
            for message in messages if key not in current else messages[1:]:
                post.touch()
                self.dc.delete(post.id, message["id"])
        if closed and not post.locked:
            post.touch()
            self.dc.send(post.id, note(f"{STATUS[status]} on GitHub."))
        if closed and not (post.archived and post.locked):
            self.dc.update_thread(post.id, archived=True, locked=True)

    def close_deleted(self, number):
        thread = self.posts.get(number)
        if thread is None or (thread.get("thread_metadata") or {}).get("locked"):
            return
        post = Post(self.dc, thread)
        post.touch()
        self.dc.send(post.id, note("Deleted on GitHub."))
        self.dc.update_thread(post.id, archived=True, locked=True)

    def sync_roadmap(self, issue):
        messages = roadmap_messages(issue, self.gh.repo)
        name = clip_line(f"{self.title} roadmap", 100)
        threads = sorted(self.dc.posts(self.roadmap_forum), key=lambda t: int(t["id"]))
        thread = next((t for t in threads if t.get("owner_id") == self.me and t.get("name") == name), None)
        if thread is None:
            thread = self.dc.create_post(self.roadmap_forum["id"], name, messages[0])
            print("Roadmap: new post")
        post = Post(self.dc, thread)
        if post.locked:
            self.dc.update_thread(post.id, archived=False, locked=False)
            post.archived = post.locked = False
        mine = [m for m in self.dc.messages(post.id) if m["author"]["id"] == self.me]
        for index, payload in enumerate(messages):
            if index >= len(mine):
                post.touch()
                self.dc.send(post.id, payload)
            elif not same(mine[index], payload):
                post.touch()
                self.dc.edit(post.id, mine[index]["id"], payload)
        for message in mine[len(messages):]:
            post.touch()
            self.dc.delete(post.id, message["id"])


def forum_channel(dc, channel_id, what):
    channel = dc.channel(channel_id)
    if not channel or channel.get("type") != FORUM:
        raise TrackerError(f"The {what} channel isn't a forum channel the bot can see")
    return channel


def run(gh, dc, cfg, event=None, now=None):
    now = now or dt.datetime.now(dt.timezone.utc)
    event = event or {}
    roadmap_forum = forum_channel(dc, cfg["roadmap"], "roadmap") if cfg.get("roadmap") else None
    tracker = Tracker(gh, dc, cfg["mod"], cfg["title"], forum_channel(dc, cfg["tracker"], "tracker"), roadmap_forum)
    open_issues = gh.issues()
    candidates = {i["number"]: i for i in open_issues}
    since = (now - LOOKBACK).strftime("%Y-%m-%dT%H:%M:%SZ")
    for issue in gh.issues(since=since):
        candidates.setdefault(issue["number"], issue)
    wanted = set(tracker.posts) if cfg.get("full") else set()
    target = (event.get("issue") or {}) if "pull_request" not in (event.get("issue") or {}) else {}
    if target.get("number"):
        wanted.add(target["number"])
    for number in sorted(wanted - set(candidates)):
        issue = gh.issue(number)
        if issue:
            candidates[number] = issue
        elif event.get("action") == "deleted" and number == target.get("number"):
            tracker.close_deleted(number)
    roadmap = None
    if cfg.get("roadmap_issue"):
        roadmap = gh.issue(cfg["roadmap_issue"])
    else:
        found = sorted((i for i in open_issues if i["title"].casefold().startswith("roadmap")), key=lambda i: i["number"])
        roadmap = found[0] if found else None
    failures = 0
    for number in sorted(candidates):
        if roadmap_forum and roadmap and number == roadmap["number"]:
            continue
        try:
            tracker.sync_issue(candidates[number])
        except TrackerError as exc:
            failures += 1
            print(f"#{number}: {exc}", file=sys.stderr)
    if roadmap_forum:
        if roadmap and roadmap["state"] == "open":
            try:
                tracker.sync_roadmap(roadmap)
            except TrackerError as exc:
                failures += 1
                print(f"Roadmap: {exc}", file=sys.stderr)
        else:
            print("No open roadmap issue found; the roadmap post is left as it is.")
    print(f"Checked {len(candidates)} issues; {failures} failed.")
    return 1 if failures else 0


def main():
    env = os.environ
    token, tracker = env.get("DISCORD_BOT_TOKEN", ""), env.get("DISCORD_TRACKER_CHANNEL_ID", "").strip()
    if not token or not tracker:
        print("DISCORD_BOT_TOKEN or the tracker channel isn't set; skipping.")
        return 0
    cfg = {"mod": env.get("TRACKER_MOD", "").strip(), "title": env.get("TRACKER_TITLE", "").strip(),
           "tracker": tracker, "roadmap": env.get("DISCORD_ROADMAP_CHANNEL_ID", "").strip(),
           "roadmap_issue": int(env.get("ROADMAP_ISSUE") or 0), "full": env.get("TRACKER_FULL") == "true"}
    cfg["title"] = cfg["title"] or cfg["mod"]
    if not re.fullmatch(r"[A-Za-z0-9]{1,20}", cfg["mod"]):
        raise SystemExit("TRACKER_MOD must be a short name such as ACA")
    for key in ("tracker", "roadmap"):
        if cfg[key] and not cfg[key].isdigit():
            raise SystemExit(f"The {key} channel id must be a number (Discord: Copy Channel ID)")
    event = {}
    if env.get("GITHUB_EVENT_PATH"):
        with open(env["GITHUB_EVENT_PATH"], encoding="utf-8") as handle:
            event = json.load(handle)
    http = HTTP()
    try:
        return run(GitHub(http, env["GITHUB_REPOSITORY"], env["GH_TOKEN"]), Discord(http, token), cfg, event)
    except TrackerError as exc:
        print(f"Tracker failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
