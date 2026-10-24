# Workshop changelogs in Discord

One scheduled workflow in **propst-design/ArmaCommanderReforger** watches all three
Workshop mods. Do not install another copy in the other repositories: independent
ledgers would post duplicates to the same channel.

| Workshop mod | ID | Source repository |
| --- | --- | --- |
| Arma Commander Actual | `AC230926A0010001` | [ArmaCommanderReforger](https://github.com/propst-design/ArmaCommanderReforger) |
| Better Battle Buddies | `BBB0261015000001` | [BetterBattleBuddies](https://github.com/propst-design/BetterBattleBuddies) |
| AiO Command Menu -Reforger- | `629405AEFC950C12` | [AIOCommandMenu](https://github.com/propst-design/AIOCommandMenu) |

## First installation

1. In Discord, open the destination text channel's **Edit Channel → Integrations →
   Webhooks → New Webhook**. Name it, save, and copy its webhook URL. You need
   permission to manage webhooks. A normal text channel is simplest; forum channels
   require an existing thread and its `thread_id` query parameter.
2. In the GitHub **ArmaCommanderReforger** repository, open **Settings → Secrets and
   variables → Actions → New repository secret**. Name it `DISCORD_WEBHOOK_URL` and
   paste the Discord URL into **Secret**. [Open the secret form](https://github.com/propst-design/ArmaCommanderReforger/settings/secrets/actions/new).
   This is an Actions secret, not the repository's **Webhooks** settings page.
   Do not commit the URL or paste it into logs, issues, pull requests or chat.
3. Merge the relay workflow into the default branch. GitHub scheduling only runs
   workflows on that branch. Actions must be enabled, and repository policies must
   permit this workflow's `contents: write` permission and creation/updates of the
   `workshop-relay-state` branch. No PAT is required: it uses `github.token`.
4. Open **Actions → Workshop changelog relay → Run workflow**, select the default
   branch and `mode: initialize`. Leave the recovery fields alone. This records all
   existing published/approved versions for each mod **without posting anything**.
   Initialization can run before the Discord secret is configured. An API failure
   fails the run; successfully initialized mods remain saved. Rerun initialization
   to finish the others; it preserves existing ledgers and does not consume new
   releases for already initialized mods.
5. Run the same workflow with `mode: poll` once. A green result validates API reads,
   state access and the webhook's identity/authentication with a read-only Discord
   request. With no new version, it does not post, so this is **not** proof of
   Discord channel posting permissions or delivery.
   The first new Workshop release exercises real delivery. Watch that run and the
   channel; no historical release is sent as a test.

No Discord bot account, Gateway connection or extra server is needed. Anyone with
the webhook URL can post through it. Delete/recreate the Discord webhook and replace
the secret if the URL leaks. Secret values cannot be read back from GitHub.

## Schedule and API contract

The schedule is at UTC minutes **7, 22, 37 and 52** of every hour. GitHub can delay or
drop scheduled runs. Public repository schedules can be disabled after 60 days of
inactivity; check the Actions page and re-enable when necessary. Enable GitHub's
workflow failure notifications for this repository.

The relay reads the independent [Reforger Mods API v2](https://reforgermods.net/arma-reforger-mods-api/v2/):

- `GET https://api.reforgermods.net/v2/mods/{id}` returns `status` and `mod`,
  including `id`, `name`, `version` and `versionCount`.
- `GET /v2/mods/{id}/versions` returns `data.modId`, `data.count` and
  `data.versions[]`. Each version supplies `version`, `published`, `approved` and
  usually `createdAt`. The currently documented endpoint returns the full history,
  without pagination. The relay also follows same-resource `links.next` or
  `meta.currentPage`/`totalPages` if supplied, rejects loops/foreign hosts and
  incomplete counts, and caps traversal at 100 pages.
- `GET /v2/mods/{id}/versions/{encoded-version}` returns `data.modId`, `version`,
  `published`, `approved` and the **version's own `changelog`**. Notes are never
  substituted from GitHub releases, descriptions, or another Workshop version.

Live responses for all three mods were verified on **2026-10-23** and captured in
`tools/workshop_relay/fixtures/`. Current versions then were ACA 0.5.6, BBB 0.1.5 and
AiO 0.3.2. These fixture files are public API samples, not credentials.

Only previously unseen versions with both `published: true` and `approved: true`
are eligible. Published versions are identified by **mod ID + exact version label**,
not the mod's modification date or semantic version ordering. Metadata edits,
reordering, rollbacks to an already seen label, and edited changelogs do not repost.
An unpublished/unapproved version in the baseline can become eligible later.
Multiple releases between polls are processed oldest first by `createdAt`, then label.
A version removed from history and later restored under the same label stays seen.
If Workshop ever reuses a label for a different package, this relay intentionally
does not treat it as a new release.

The API provider [documents caching and public rate limits](https://reforgermods.net/arma-reforger-mods-api/).
History can be cached for six hours and mod details for one hour, so **15-minute
polling does not guarantee a 15-minute publication-to-Discord delay**. The relay
rejects explicit `X-Cache: STALE` responses and mismatched mod/history counts instead
of initializing from partial data. It cannot detect stale data that the provider
does not label, or releases the provider never exposes. Baseline means the complete,
coherent history the API returns at initialization; an older version first exposed
by the API later is an unseen version and may be relayed. There is no invented
publication timestamp or assumption that `updatedAt` proves a new release.

## Messages, outages and persistent duplicate protection

Each message includes the API's mod name, exact version, canonical Workshop link,
part number and stable delivery marker. Changelog text is preserved in outgoing
payloads, including whitespace and line breaks. It is split at newlines where
possible, with a Unicode-safe fallback for long lines; headers and text fit within
Discord's 2,000-character limit using conservative UTF-16 counts. Markdown code
blocks can span messages and therefore render differently across part boundaries;
the underlying text is not rewritten. All user/role/everyone mentions are disabled
and link previews are suppressed.

Missing, empty or whitespace-only changelogs leave that release unsent and produce
a failed run for visibility. Later polls retry it, retaining chronological order
for that mod. No placeholder is invented and no successful delivery is recorded.
Other mods can still proceed. If the author never supplies notes, that mod remains
blocked until the author publishes the missing notes or an operator deliberately
changes the ledger after disabling the workflow.

Network/API outages, HTTP 202 refreshes, 429 limits and read-side 5xx errors have
bounded retries. `Retry-After` (seconds or HTTP date) and Discord JSON `retry_after`
are respected up to 60 seconds per wait. Longer waits and exhausted retries leave
work for the next run. POST requests are retried only after an explicit 429
rejection. There is no aggressive refresh/cache-busting loop. Eligible public reads
need no API key; optionally add `REFORGERMODS_API_KEY` as another Actions secret if
shared runner IP limits become a problem. It is only sent to the fixed API origin.

The durable ledger is `workshop-relay-state.json` on the **workshop-relay-state**
branch in this repository. It records baseline versions, frozen pending message
payloads, per-part progress and Discord message IDs. It contains public changelog
text but **no webhook URL or token**. It is not an expiring Actions cache or artifact.
The state branch initially shares a default-branch commit, then evolves separately.
Do not merge it back to main, delete it, reset it or overwrite it from an old clone.
Back it up through Git history. Polling refuses to recreate a missing branch/file;
`initialize` is an explicit first-install operation, not an outage repair command.

The workflow serializes runs without cancelling active work. Before **each** Discord
POST, it commits the send intent; after `wait=true` returns a message ID, it commits
the acknowledgement. GitHub writes use the previous blob SHA as compare-and-swap.
A state-write failure immediately stops all sending. A restart resumes confirmed
progress without resending successful parts; a crash after the final acknowledgement
can finish the version ledger without another POST. Pending payloads are frozen so
an author editing notes mid-delivery cannot shift chunk boundaries.

Discord's webhook execution endpoint has no transactional commit with GitHub and
does not offer a durable exactly-once key for these messages. A timeout, connection
loss, 5xx, invalid acknowledgement or runner interruption after the durable intent
can mean Discord accepted a message. Such a part is held for operator review,
**never automatically resent**. This trades unattended delivery during rare
uncertain failures for duplicate prevention. Do not claim this relay guarantees
both automatic exactly-once delivery and no operator intervention.

### Recover an uncertain part

1. Inspect the failed run and the state branch's `pending` entry for that mod.
   `next` and `inflight` are zero-based; the workflow recovery form uses a **one-based**
   part number. Read the planned payload's `relay <marker>` and search the Discord
   destination channel for that exact marker. Wait for the failed run to finish.
2. Run **Workshop changelog relay** with `mode: recover`, the exact mod ID, exact
   version, and that part number. Choose `delivered` **only if the message exists**;
   choose `retry` **only after confirming it is absent**. Incorrectly choosing retry
   can duplicate a message. Recovery refuses a different version or part number.
3. Recovery updates the ledger without posting. Run `mode: poll` to resume; later
   scheduled runs do the same. A confirmed part is recorded as operator-confirmed.

Do not clear `pending` or delete the ledger to recover. For a definitively rejected
400/401/403/404 response, the intent is cleared and the next run can retry after the
underlying webhook/permission problem is fixed. HTTP 429 is also a definite rejection.
An ordinary API outage requires no ledger edits.

## Adding mods and maintaining the relay

Add another object with its actual uppercase 16-character Workshop `id` to
`tools/workshop_relay/mods.json`; `repository` is a documentation annotation.
The API supplies display names and version data. The first successful poll for a
new ID silently baselines its existing published history. All configured IDs use
the same secret/channel. Remove an ID to stop polling, but preserve its ledger so
re-adding it does not reset history. Separate channel routing is not configured.

Read-only live schema check (requires internet, no GitHub or Discord credentials):

```sh
python3 tools/workshop_relay/relay.py --mode probe
```

Offline delivery, failure and restart tests (no secrets or network):

```sh
python3 -m unittest discover -s tools/workshop_relay -p 'test_*.py' -v
```

The separate **Workshop relay checks** workflow runs these tests on relevant pull
requests. No polling or Discord secret is exposed to pull-request jobs. The relay
itself is restricted to the original repository's default branch. It does not
modify game code, upload Workshop content, or publish a GitHub release.

External contracts: [Discord webhook execution](https://docs.discord.com/developers/resources/webhook#execute-webhook)
and [GitHub scheduled workflows](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#schedule).
