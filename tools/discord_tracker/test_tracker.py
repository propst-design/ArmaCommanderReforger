"""Offline checks: an in-memory GitHub and Discord, with Discord's archived-thread rules."""
import itertools
import json
import os
from pathlib import Path
import re
import sys
import unittest
import urllib.parse as up

sys.path.insert(0, str(Path(__file__).resolve().parent))
import tracker  # noqa: E402

REPO = "propst-design/ArmaCommanderReforger"
BOT = "900"


class Fake:
    def __init__(self):
        self.ids = itertools.count(1000)
        self.issues, self.comments = {}, {}
        self.channels = {
            "10": {"id": "10", "type": 15, "guild_id": "1", "available_tags": [
                {"id": "t-aca", "name": "ACA"}, {"id": "t-bug", "name": "Bug"},
                {"id": "t-idea", "name": "Idea"}, {"id": "t-done", "name": "Done"}]},
            "20": {"id": "20", "type": 15, "guild_id": "1", "available_tags": []},
            "30": {"id": "30", "type": 0, "guild_id": "1"},
        }
        self.messages = {}
        self.writes = []

    # GitHub side
    def add_issue(self, number, title, body="", state="open", labels=(), reason=None):
        self.issues[number] = {"number": number, "title": title, "body": body, "state": state,
                               "state_reason": reason, "labels": [{"name": n} for n in labels],
                               "html_url": f"https://github.com/{REPO}/issues/{number}",
                               "user": {"login": "jacob", "html_url": "https://github.com/jacob"},
                               "created_at": "2026-10-01T00:00:00Z", "updated_at": "2026-10-28T00:00:00Z"}
        self.comments.setdefault(number, [])
        return self.issues[number]

    def add_comment(self, number, body, login="player", kind="User"):
        cid = next(self.ids)
        self.comments[number].append({"id": cid, "body": body, "user": {"login": login, "type": kind},
                                      "html_url": f"https://github.com/{REPO}/issues/{number}#issuecomment-{cid}",
                                      "created_at": "2026-10-28T00:00:00Z"})
        return self.comments[number][-1]

    # Discord side
    def threads(self, parent):
        return [c for c in self.channels.values() if c.get("parent_id") == parent]

    def thread_messages(self, tid):
        return self.messages[tid]

    def request(self, method, url, headers, payload=None):
        parsed = up.urlparse(url)
        path, query = parsed.path, up.parse_qs(parsed.query)
        if method != "GET":
            self.writes.append((method, path, payload))
        if url.startswith(tracker.GITHUB):
            return self.github(path.removeprefix(f"/repos/{REPO}"), query)
        return self.discord(method, path.removeprefix("/api/v10"), payload)

    def github(self, path, query):
        if query.get("page", ["1"]) != ["1"]:
            return []
        if path == "/issues":
            rows = self.issues.values()
            if query.get("state") == ["open"]:
                rows = [i for i in rows if i["state"] == "open"]
            return [dict(i) for i in rows]
        match = re.fullmatch(r"/issues/(\d+)(/comments)?", path)
        number = int(match[1])
        if number not in self.issues:
            raise tracker.HttpError(404)
        return [dict(c) for c in self.comments[number]] if match[2] else dict(self.issues[number])

    def discord(self, method, path, payload):
        if path == "/" + "users/@me":
            return {"id": BOT}
        if path == "/guilds/1/threads/active":
            return {"threads": [c for c in self.channels.values() if "parent_id" in c and not c["thread_metadata"]["archived"]]}
        match = re.fullmatch(r"/channels/(\d+)/threads/archived/public", path)
        if match:
            return {"threads": [c for c in self.threads(match[1]) if c["thread_metadata"]["archived"]], "has_more": False}
        match = re.fullmatch(r"/channels/(\d+)/threads", path)
        if match:
            tid = str(next(self.ids))
            thread = {"id": tid, "type": 11, "parent_id": match[1], "owner_id": BOT, "name": payload["name"],
                      "applied_tags": payload.get("applied_tags", []),
                      "thread_metadata": {"archived": False, "locked": False, "archive_timestamp": "x"}}
            self.channels[tid] = thread
            self.messages[tid] = [self.stored(tid, payload["message"])]
            return dict(thread)
        match = re.fullmatch(r"/channels/(\d+)/messages(?:/(\d+))?", path)
        if match:
            tid, mid = match[1], match[2]
            meta = self.channels[tid]["thread_metadata"]
            if method == "GET":
                return list(reversed(self.messages[tid]))
            if meta["archived"] and (method != "POST" or meta["locked"]):
                raise tracker.HttpError(400, b'{"code": 50083, "message": "Thread is archived"}')
            meta["archived"] = False
            if method == "POST":
                self.messages[tid].append(self.stored(str(next(self.ids)), payload))
            elif method == "PATCH":
                message = next(m for m in self.messages[tid] if m["id"] == mid)
                message.update(content=payload.get("content", ""), embeds=payload.get("embeds", []))
            else:
                self.messages[tid] = [m for m in self.messages[tid] if m["id"] != mid]
            return None
        match = re.fullmatch(r"/channels/(\d+)", path)
        channel = self.channels.get(match[1])
        if method == "GET":
            if channel is None:
                raise tracker.HttpError(404)
            return dict(channel)
        meta = channel["thread_metadata"]
        if meta["archived"] and payload.get("archived") is not False and set(payload) - {"archived", "locked"}:
            raise tracker.HttpError(400, b'{"code": 50083, "message": "Thread is archived"}')
        for key in ("archived", "locked"):
            if key in payload:
                meta[key] = payload[key]
        channel.update({k: v for k, v in payload.items() if k in ("name", "applied_tags")})
        return dict(channel)

    def stored(self, mid, payload):
        return {"id": mid, "author": {"id": BOT}, "content": payload.get("content", ""),
                "embeds": json.loads(json.dumps(payload.get("embeds", [])))}

    def post(self, number):
        return next(c for c in self.threads("10") if c["name"].startswith(f"ACA #{number} "))


CFG = {"mod": "ACA", "title": "Arma Commander: Actual", "tracker": "10", "roadmap": "20"}


def run(fake, event=None, **cfg):
    gh, dc = tracker.GitHub(fake, REPO, "gh-token"), tracker.Discord(fake, "bot-token")
    fake.writes = []
    result = tracker.run(gh, dc, {**CFG, **cfg}, event)
    return result, fake.writes


class TrackerTest(unittest.TestCase):
    def setUp(self):
        self.fake = Fake()
        self.fake.add_issue(1, "Roadmap: what's being worked on", "### In progress\n- Spawning rework (#39)")
        self.fake.add_issue(39, "Spawning rework", "Choose your side", labels=["enhancement", "in progress"])
        self.fake.add_issue(73, "Garrisons spawn inside walls", "See #39", labels=["bug"])
        self.fake.add_issue(12, "Old fixed thing", state="closed")
        self.fake.add_comment(39, "Tested on Everon")
        self.fake.add_comment(39, "Bot noise", login="github-actions[bot]", kind="Bot")

    def test_first_run_posts_open_issues_and_the_roadmap(self):
        self.assertEqual(run(self.fake)[0], 0)
        names = sorted(c["name"] for c in self.fake.threads("10"))
        self.assertEqual(names, ["ACA #39 Spawning rework", "ACA #73 Garrisons spawn inside walls"])
        self.assertEqual(self.fake.post(39)["applied_tags"], ["t-aca", "t-idea"])
        self.assertEqual(self.fake.post(73)["applied_tags"], ["t-aca", "t-bug"])
        messages = self.fake.messages[self.fake.post(39)["id"]]
        self.assertEqual(len(messages), 2)  # starter and the human comment, not the bot's
        self.assertIn("Tested on Everon", messages[1]["embeds"][0]["description"])
        starter = self.fake.messages[self.fake.post(73)["id"]][0]["embeds"][0]
        self.assertIn(f"[#39](https://github.com/{REPO}/issues/39)", starter["description"])
        roadmap = self.fake.threads("20")
        self.assertEqual([t["name"] for t in roadmap], ["Arma Commander: Actual roadmap"])
        self.assertIn("Spawning rework", self.fake.messages[roadmap[0]["id"]][0]["content"])

    def test_a_rerun_with_nothing_new_sends_nothing(self):
        run(self.fake)
        self.assertEqual(run(self.fake), (0, []))

    def test_close_comment_and_reopen(self):
        run(self.fake)
        self.fake.add_comment(73, "Fixed in 0.5.8")
        issue = self.fake.issues[73]
        issue["state"], issue["state_reason"] = "closed", "completed"
        self.assertEqual(run(self.fake)[0], 0)
        post = self.fake.post(73)
        self.assertEqual(post["thread_metadata"], {"archived": True, "locked": True, "archive_timestamp": "x"})
        self.assertEqual(post["applied_tags"], ["t-aca", "t-done", "t-bug"])
        contents = [m["content"] for m in self.fake.messages[post["id"]]]
        self.assertEqual(contents[-1], "Closed as completed on GitHub.")
        self.assertEqual(run(self.fake), (0, []))
        # A comment on the closed issue reaches the locked post without a second closing note.
        self.fake.add_comment(73, "Still happens on Arland")
        run(self.fake)
        self.assertEqual(len(self.fake.messages[post["id"]]), 4)
        self.assertTrue(post["thread_metadata"]["locked"] and post["thread_metadata"]["archived"])
        issue["state"] = "open"
        run(self.fake)
        self.assertEqual(self.fake.messages[post["id"]][-1]["content"], "Reopened on GitHub.")
        self.assertFalse(post["thread_metadata"]["locked"])
        self.assertEqual(run(self.fake), (0, []))

    def test_edits_and_deleted_comments_follow_github(self):
        run(self.fake)
        post = self.fake.post(39)
        post["thread_metadata"]["archived"] = True  # auto-archived after a quiet week
        self.fake.issues[39]["title"] = "Spawning rework: TvT"
        self.fake.comments[39][0]["body"] = "Tested on Everon and Arland"
        self.fake.add_comment(39, "Second")
        run(self.fake)
        self.assertEqual(post["name"], "ACA #39 Spawning rework: TvT")
        messages = self.fake.messages[post["id"]]
        self.assertIn("and Arland", messages[1]["embeds"][0]["description"])
        self.assertEqual(len(messages), 3)
        del self.fake.comments[39][0]
        run(self.fake)
        self.assertEqual(len(self.fake.messages[post["id"]]), 2)
        self.assertEqual(run(self.fake), (0, []))

    def test_roadmap_grows_and_shrinks_in_place(self):
        run(self.fake)
        roadmap = self.fake.issues[1]
        roadmap["body"] = "\n".join(f"### Part {n}\n" + "- item\n" * 300 for n in range(3))
        run(self.fake)
        post = self.fake.threads("20")[0]
        messages = self.fake.messages[post["id"]]
        self.assertGreater(len(messages), 2)
        self.assertTrue(all(0 < len(m["content"]) <= 2000 for m in messages))
        roadmap["body"] = "Short again"
        run(self.fake)
        self.assertEqual(len(self.fake.messages[post["id"]]), 1)
        self.assertEqual(len(self.fake.threads("20")), 1)
        self.assertEqual(run(self.fake), (0, []))

    def test_deleted_issue_locks_its_post(self):
        run(self.fake)
        post = self.fake.post(73)
        del self.fake.issues[73]
        run(self.fake, {"action": "deleted", "issue": {"number": 73}})
        self.assertEqual(self.fake.messages[post["id"]][-1]["content"], "Deleted on GitHub.")
        self.assertTrue(post["thread_metadata"]["locked"])

    def test_other_posts_in_the_forum_are_left_alone(self):
        self.fake.channels["50"] = {"id": "50", "parent_id": "10", "owner_id": "123", "name": "ACA #39 my own post",
                                    "thread_metadata": {"archived": False, "locked": False}}
        self.fake.messages["50"] = []
        run(self.fake)
        self.assertEqual(len(self.fake.threads("10")), 3)
        self.assertNotIn("50", [w[1].split("/")[3] for w in self.fake.writes if w[1].startswith("/api/v10/channels/")])

    def test_a_text_channel_is_refused(self):
        with self.assertRaises(tracker.TrackerError):
            run(self.fake, tracker="30")

    def test_missing_configuration_skips(self):
        saved = dict(os.environ)
        try:
            os.environ.pop("DISCORD_BOT_TOKEN", None)
            self.assertEqual(tracker.main(), 0)
        finally:
            os.environ.clear()
            os.environ.update(saved)


class RenderTest(unittest.TestCase):
    def test_markdown(self):
        text = tracker.to_discord("<!-- hidden -->#### Deep\n- [ ] todo\n- [x] done\n@everyone see #4 and "
                                  "propst-design/BetterBattleBuddies#11, not `#5` or [#6](x)", REPO)
        self.assertEqual(text, "**Deep**\n- ☐ todo\n- ☑ done\n@​everyone see "
                               f"[#4](https://github.com/{REPO}/issues/4) and "
                               "[propst-design/BetterBattleBuddies#11](https://github.com/propst-design/BetterBattleBuddies/issues/11)"
                               ", not `#5` or [#6](x)")

    def test_split_keeps_lines_whole(self):
        chunks = tracker.split_text("\n".join("x" * 50 for _ in range(100)), 500)
        self.assertTrue(all(len(c) <= 500 for c in chunks))
        self.assertEqual("".join(chunks).count("x"), 5000)


if __name__ == "__main__":
    unittest.main()
