"""Workshop -> Discord. Standard library only; no credentials belong in state/logs."""
import argparse
import base64
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import re
import socket
import sys
import time
import urllib.error
import urllib.parse as up
import urllib.request as ur
from email.utils import parsedate_to_datetime

API = "https://api.reforgermods.net/v2"
BRANCH = "workshop-relay-state"
STATE_PATH = "workshop-relay-state.json"
ROOT = Path(__file__).resolve().parent


class RelayError(Exception):
    pass


class StateError(RelayError):
    """A durable state failure must stop ALL further sends."""


class HttpError(RelayError):
    def __init__(self, status, headers=None, body=b""):
        super().__init__(f"HTTP {status}")
        self.status, self.headers, self.body = status, headers or {}, body


class NoRedirect(ur.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


def delay(headers, body, attempt):
    value = headers.get("Retry-After")
    try:
        seconds = float(value)
    except (TypeError, ValueError):
        try:
            seconds = (parsedate_to_datetime(value) - dt.datetime.now(dt.timezone.utc)).total_seconds()
        except (TypeError, ValueError):
            try:
                seconds = float(json.loads(body).get("retry_after", 2 ** attempt))
            except (ValueError, TypeError, AttributeError):
                seconds = 2 ** attempt
    if not 0 <= seconds <= 60:
        raise RelayError("Retry delay exceeds this run's budget; retry next scheduled run")
    return max(seconds, 0.25)


class HTTP:
    def __init__(self):
        self.opener = ur.build_opener(NoRedirect())

    def request(self, method, url, headers=None, payload=None):
        headers = {"User-Agent": "ArmaWorkshopDiscordRelay/1.0 (+https://github.com/propst-design/ArmaCommanderReforger)",
                   "Accept": "application/json", **(headers or {})}
        data = None if payload is None else json.dumps(payload, ensure_ascii=False).encode("utf-8")
        if data is not None:
            headers["Content-Type"] = "application/json"
        for attempt in range(4):
            try:
                with self.opener.open(ur.Request(url, data=data, headers=headers, method=method), timeout=30) as response:
                    status, response_headers, body = response.status, response.headers, response.read()
            except urllib.error.HTTPError as exc:
                status, response_headers, body = exc.code, exc.headers, exc.read()
            except (urllib.error.URLError, TimeoutError, socket.timeout, OSError):
                if method == "GET" and attempt < 3:
                    time.sleep(2 ** attempt)
                    continue
                # Never include exception text: urllib exceptions can contain webhook tokens.
                raise RelayError("Network failure; delivery may be uncertain") from None
            retry = status == 429 or (method == "GET" and (status == 202 or status >= 500))
            if retry and attempt < 3:
                try:
                    seconds = delay(response_headers, body, attempt)
                except RelayError:
                    # Preserve the definite rejection status for the delivery ledger.
                    raise HttpError(status, response_headers, body) from None
                time.sleep(seconds)
                continue
            if not 200 <= status < 300 or status == 202:
                raise HttpError(status, response_headers, body)
            try:
                return json.loads(body), response_headers
            except (ValueError, UnicodeDecodeError):
                raise RelayError("Invalid JSON response; delivery may be uncertain") from None
        raise RelayError("Request retry budget exhausted")


def object_field(value, key, kind):
    if not isinstance(value, dict) or not isinstance(value.get(key), kind):
        raise RelayError(f"API/state schema mismatch: {key}")
    return value[key]


def version_name(row):
    value = object_field(row, "version", str)
    if not value or len(value) > 100:
        raise RelayError("Invalid version name")
    return value


class Workshop:
    def __init__(self, http):
        self.http = http

    def get(self, url):
        headers = {}
        if os.getenv("REFORGERMODS_API_KEY"):
            headers["X-API-Key"] = os.environ["REFORGERMODS_API_KEY"]
        result, response_headers = self.http.request("GET", url, headers)
        if response_headers.get("X-Cache", "").upper() == "STALE":
            raise RelayError("Workshop cache is stale; postponing")
        if result.get("status") != "success":
            raise RelayError("Workshop response is not successful")
        return result

    def snapshot(self, mod_id):
        mod = object_field(self.get(f"{API}/mods/{mod_id}"), "mod", dict)
        if mod.get("id") != mod_id or not isinstance(mod.get("name"), str) or not mod["name"]:
            raise RelayError("Mod identity/name mismatch")
        initial = f"{API}/mods/{mod_id}/versions"
        url, visited, rows, count = initial, set(), [], None
        while url:
            parsed = up.urlsplit(url)
            if (parsed.scheme, parsed.netloc, parsed.path) != ("https", "api.reforgermods.net", up.urlsplit(initial).path):
                raise RelayError("Unsafe version pagination link")
            if url in visited or len(visited) >= 100:
                raise RelayError("Repeated/excessive version pagination")
            visited.add(url)
            result = self.get(url)
            data = object_field(result, "data", dict)
            if data.get("modId") != mod_id:
                raise RelayError("Version history mod identity mismatch")
            page_count = object_field(data, "count", int)
            if count is not None and count != page_count:
                raise RelayError("Version history changed during pagination")
            count = page_count
            rows.extend(object_field(data, "versions", list))
            url = result.get("links", {}).get("next")
            meta = result.get("meta", {})
            if not url and meta.get("currentPage", 1) < meta.get("totalPages", 1):
                url = initial + "?page=" + str(meta["currentPage"] + 1)
            if url:
                url = up.urljoin(initial, url)
        names = [version_name(row) for row in rows]
        if count != len(rows) or len(set(names)) != len(names) or not rows:
            raise RelayError("Incomplete/duplicate/empty version history; leaving state unchanged")
        for row in rows:
            object_field(row, "published", bool)
            object_field(row, "approved", bool)
        if mod.get("versionCount") != count or mod.get("version") not in names:
            raise RelayError("Mod detail and history caches disagree; postponing")
        return mod, rows

    def detail(self, mod_id, version):
        result = self.get(f"{API}/mods/{mod_id}/versions/{up.quote(version, safe='')}")
        data = object_field(result, "data", dict)
        if data.get("modId") != mod_id or data.get("version") != version:
            raise RelayError("Version detail identity mismatch")
        if data.get("published") is not True or data.get("approved") is not True:
            raise RelayError("Version detail not published/approved yet")
        notes = data.get("changelog")
        if not isinstance(notes, str) or not notes.strip():
            raise RelayError("Workshop changelog absent/empty; retaining version for a later poll")
        return notes


class GitHubState:
    def __init__(self, http, repo, token):
        if not re.fullmatch(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+", repo):
            raise StateError("Invalid repository")
        self.http, self.sha = http, None
        self.base = f"https://api.github.com/repos/{repo}"
        self.headers = {"Authorization": f"Bearer {token}", "X-GitHub-Api-Version": "2022-11-28"}

    def request(self, method, path, payload=None):
        try:
            return self.http.request(method, self.base + path, self.headers, payload)[0]
        except RelayError as exc:
            raise StateError(f"GitHub state operation failed ({exc}); no further sends") from None

    def load(self, initialize=False):
        # Explicit initialization is required so a deleted state branch cannot silently reset history.
        try:
            self.http.request("GET", self.base + f"/git/ref/heads/{BRANCH}", self.headers)
        except HttpError as exc:
            if exc.status != 404 or not initialize:
                raise StateError("State branch unavailable; use initialize only on first installation") from None
            sha = os.environ.get("GITHUB_SHA")
            if not sha:
                raise StateError("GITHUB_SHA required for first initialization")
            self.request("POST", "/git/refs", {"ref": f"refs/heads/{BRANCH}", "sha": sha})
        try:
            result, _ = self.http.request("GET", self.base + f"/contents/{STATE_PATH}?ref={BRANCH}", self.headers)
        except HttpError as exc:
            if exc.status == 404 and initialize:
                return {"schema": 1, "mods": {}}
            raise StateError("State file unavailable; refusing automatic reinitialization") from None
        self.sha = result["sha"]
        try:
            state = json.loads(base64.b64decode(result["content"]))
            if state.get("schema") != 1 or not isinstance(state.get("mods"), dict):
                raise ValueError()
            validate_state(state)
        except (KeyError, ValueError, TypeError):
            raise StateError("State is corrupt/unsupported; restore it before polling") from None
        return state

    def save(self, state):
        raw = (json.dumps(state, ensure_ascii=False, indent=2) + "\n").encode()
        if len(raw) > 900_000:
            raise StateError("Ledger approaching GitHub Contents API read limit; archive/compact before sending")
        payload = {"message": "Update Workshop relay delivery ledger", "branch": BRANCH,
                   "content": base64.b64encode(raw).decode()}
        if self.sha:
            payload["sha"] = self.sha
        # The blob SHA is compare-and-swap. A conflict or ambiguous write stops this run.
        result = self.request("PUT", f"/contents/{STATE_PATH}", payload)
        try:
            self.sha = result["content"]["sha"]
        except (KeyError, TypeError):
            raise StateError("State write acknowledgement invalid") from None


def validate_state(state):
    for mod_id, entry in state["mods"].items():
        if not re.fullmatch(r"[0-9A-F]{16}", mod_id) or not isinstance(entry, dict):
            raise StateError("Invalid mod ledger")
        seen = entry.get("seen")
        if not isinstance(seen, list) or any(not isinstance(v, str) or not v for v in seen) or len(set(seen)) != len(seen):
            raise StateError("Invalid seen-version ledger")
        if "pending" not in entry:
            raise StateError("Missing pending-delivery ledger")
        job = entry["pending"]
        if job is None:
            continue
        if not isinstance(job, dict) or not isinstance(job.get("version"), str) or job["version"] in seen:
            raise StateError("Invalid pending version")
        payloads, index, inflight = job.get("messages"), job.get("next"), job.get("inflight")
        if not isinstance(payloads, list) or not payloads or type(index) is not int or not 0 <= index <= len(payloads):
            raise StateError("Invalid pending parts")
        if inflight is not None and (type(inflight) is not int or inflight != index or index >= len(payloads)):
            raise StateError("Invalid pending delivery intent")
        for payload in payloads:
            if not isinstance(payload, dict) or not isinstance(payload.get("content"), str) or units(payload["content"]) > 2000:
                raise StateError("Invalid pending payload")
            if payload.get("allowed_mentions") != {"parse": [], "users": [], "roles": [], "replied_user": False}:
                raise StateError("Pending payload must suppress mentions")


def units(text):
    return len(text.encode("utf-16-le")) // 2


def split_text(text, limit=1600):
    """Lossless split, preferring newlines. Count UTF-16 conservatively for Discord."""
    chunks = []
    while text:
        used, end = 0, 0
        for char in text:
            size = 2 if ord(char) > 0xFFFF else 1
            if used + size > limit:
                break
            used += size
            end += 1
        if end < len(text):
            newline = text.rfind("\n", 0, end)
            if newline >= end // 2:
                end = newline + 1
            elif end and text[end - 1] == "\r":
                end -= 1
        if not end:
            raise RelayError("Unable to split changelog")
        chunks.append(text[:end])
        text = text[end:]
    return chunks


def messages(mod_id, name, version, notes):
    base_header = f"{name[:100]} — {version}\nhttps://reforger.armaplatform.com/workshop/{mod_id}\n"
    chunks = split_text(notes, min(1600, 2000 - units(base_header) - 100))
    digest = hashlib.sha256((mod_id + "\0" + version + "\0" + notes).encode()).hexdigest()[:16]
    payloads = []
    for index, chunk in enumerate(chunks, 1):
        marker = f"{digest}:{index}"
        header = base_header + f"Part {index}/{len(chunks)} · relay {marker}\n\n"
        content = header + chunk
        if units(content) > 2000:
            raise RelayError("Discord content length exceeded")
        payloads.append({"content": content, "allowed_mentions": {"parse": [], "users": [], "roles": [], "replied_user": False}, "flags": 4})
    return payloads


def webhook_url(value):
    parsed = up.urlsplit(value)
    if parsed.scheme != "https" or parsed.netloc != "discord.com" or not re.fullmatch(r"/api(?:/v10)?/webhooks/\d+/[A-Za-z0-9_.-]+", parsed.path):
        raise RelayError("DISCORD_WEBHOOK_URL must be a discord.com incoming webhook URL")
    query = dict(up.parse_qsl(parsed.query))
    if set(query) - {"thread_id", "wait"}:
        raise RelayError("Unsupported webhook query parameters")
    query["wait"] = "true"
    return up.urlunsplit((parsed.scheme, parsed.netloc, parsed.path, up.urlencode(query), ""))


def verify_webhook(http, webhook):
    parsed = up.urlsplit(webhook)
    url = up.urlunsplit((parsed.scheme, parsed.netloc, parsed.path, "", ""))
    result, _ = http.request("GET", url)
    if not isinstance(result, dict) or result.get("id") != parsed.path.split("/")[-2] or result.get("type") != 1:
        raise RelayError("Discord webhook identity/type mismatch; incoming webhook required")
    # Do not print the object: Discord may return the webhook token in it.
    print("Discord incoming webhook verified without posting")


def deliver(entry, state, store, http, webhook):
    job = entry["pending"]
    if job.get("inflight") is not None:
        raise RelayError(f"Uncertain Discord delivery for {job['version']} part {job['next'] + 1}; review channel and use recover")
    while job["next"] < len(job["messages"]):
        index = job["next"]
        job["inflight"] = index
        store.save(state)  # Durable intent BEFORE the external side effect.
        try:
            result, _ = http.request("POST", webhook, payload=job["messages"][index])
        except HttpError as exc:
            # Definite rejection: safe to retry after fixing credentials/payload or rate limit.
            # 5xx/connection errors remain uncertain and require operator reconciliation.
            if exc.status in {400, 401, 403, 404, 405, 413, 415, 422, 429}:
                job["inflight"] = None
                store.save(state)
            raise
        message_id = result.get("id") if isinstance(result, dict) else None
        if not isinstance(message_id, str) or not message_id.isdigit():
            raise RelayError("Discord did not acknowledge a message ID; reconcile before retrying")
        job.setdefault("receipts", []).append(message_id)
        job["next"] += 1
        job["inflight"] = None
        store.save(state)
        time.sleep(0.5)
    entry["seen"].append(job["version"])
    entry.setdefault("delivered", {})[job["version"]] = job.get("receipts", [])
    entry["pending"] = None
    store.save(state)


def poll(mod_ids, state, store, workshop, http, webhook, initialize=False):
    failures = []
    for mod_id in mod_ids:
        try:
            entry = state["mods"].get(mod_id)
            if entry and entry.get("pending") and not initialize:
                deliver(entry, state, store, http, webhook)
            mod, rows = workshop.snapshot(mod_id)
            published = [row for row in rows if row["published"] and row["approved"]]
            if not entry:
                if not published or mod["version"] not in [r["version"] for r in published]:
                    raise RelayError("No coherent published baseline yet")
                state["mods"][mod_id] = {"seen": [r["version"] for r in published], "pending": None}
                store.save(state)
                print(f"{mod_id}: initialized {len(published)} existing versions without posting")
                continue
            if initialize:
                print(f"{mod_id}: existing ledger preserved")
                continue
            # No semver comparison: lower version labels can be legitimate new publications.
            for row in sorted(published, key=lambda r: (r.get("createdAt") or "", r["version"])):
                version = row["version"]
                if version in entry["seen"]:
                    continue
                notes = workshop.detail(mod_id, version)
                entry["pending"] = {"version": version, "messages": messages(mod_id, mod["name"], version, notes), "next": 0, "inflight": None}
                store.save(state)
                deliver(entry, state, store, http, webhook)
                print(f"{mod_id}: delivered {version}")
        except StateError:
            raise
        except RelayError as exc:
            failures.append(f"{mod_id}: {exc}")
    for failure in failures:
        print(f"ERROR: {failure}", file=sys.stderr)
    return 1 if failures else 0


def recover(state, store, mod_id, version, part, decision):
    entry = state["mods"].get(mod_id, {})
    job = entry.get("pending")
    if not job or job["version"] != version or job.get("inflight") is None or job["inflight"] != part - 1 or job["next"] != part - 1:
        raise RelayError("Recovery does not match the currently uncertain part")
    if decision == "delivered":
        job["next"] += 1
        job.setdefault("receipts", []).append("operator-confirmed")
    job["inflight"] = None
    store.save(state)
    print(f"{mod_id}: part {part} marked {decision}; next poll resumes")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=["poll", "initialize", "recover", "probe"], default="poll")
    parser.add_argument("--config", type=Path, default=ROOT / "mods.json")
    parser.add_argument("--mod", default="")
    parser.add_argument("--version", default="")
    parser.add_argument("--part", type=int, default=0)
    parser.add_argument("--decision", choices=["delivered", "retry"], default="delivered")
    args = parser.parse_args()
    config = json.loads(args.config.read_text(encoding="utf-8"))
    mod_ids = [row["id"] for row in config["mods"]]
    if not mod_ids or len(set(mod_ids)) != len(mod_ids) or any(not re.fullmatch(r"[0-9A-F]{16}", item) for item in mod_ids):
        raise RelayError("Configure unique 16-character uppercase Workshop IDs")
    http = HTTP()
    workshop = Workshop(http)
    if args.mode == "probe":
        for mod_id in mod_ids:
            mod, rows = workshop.snapshot(mod_id)
            notes = workshop.detail(mod_id, mod["version"])
            print(f"{mod_id}: {mod['name']} {mod['version']}; {len(rows)} versions; {len(notes)} changelog characters; {len(messages(mod_id, mod['name'], mod['version'], notes))} Discord parts")
        return 0
    webhook = webhook_url(os.environ.get("DISCORD_WEBHOOK_URL", "")) if args.mode == "poll" else None
    if webhook:
        verify_webhook(http, webhook)
    token = os.environ.get("GH_TOKEN")
    if not token:
        raise StateError("GH_TOKEN required for durable state")
    store = GitHubState(http, os.environ.get("GITHUB_REPOSITORY", ""), token)
    state = store.load(initialize=args.mode == "initialize")
    if args.mode == "recover":
        recover(state, store, args.mod, args.version, args.part, args.decision)
        return 0
    return poll(mod_ids, state, store, workshop, http, webhook, initialize=args.mode == "initialize")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except RelayError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        sys.exit(1)
    except Exception:
        # Unknown exceptions must not expose request URLs/credentials in a traceback.
        print("ERROR: unexpected schema/runtime failure; no further sends; inspect durable ledger", file=sys.stderr)
        sys.exit(1)
