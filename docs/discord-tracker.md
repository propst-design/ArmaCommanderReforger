# Issue tracker and roadmap in Discord

GitHub issues from all three mods are mirrored into one Discord forum channel, and each mod's
roadmap issue into a second forum channel. A change on GitHub shows in Discord within about a
minute. GitHub stays the source: Discord replies stay in Discord, and edits made in Discord to
the bot's posts are put back on the next run.

| Repository | Post prefix | Roadmap post |
| --- | --- | --- |
| [ArmaCommanderReforger](https://github.com/propst-design/ArmaCommanderReforger) | `ACA #12 …` | Arma Commander: Actual roadmap |
| [BetterBattleBuddies](https://github.com/propst-design/BetterBattleBuddies) | `BBB #12 …` | Better Battle Buddies roadmap |
| [AIOCommandMenu](https://github.com/propst-design/AIOCommandMenu) | `AiO #12 …` | AiO Command Menu roadmap |

The logic is `tools/discord_tracker/tracker.py` with the reusable workflow
`.github/workflows/discord-tracker-sync.yml`, both in this repository. Each repository's own
`.github/workflows/discord-tracker.yml` calls it on issue and comment events, every six hours
as a fallback, and on demand.

## What it does

- **Every open issue** gets a forum post: the issue as an embed (status, labels, body, author)
  and then one message per comment. Comments by bots are skipped. Issue references such as `#12`
  become links.
- **Edits follow GitHub:** a new title renames the post, an edited comment is edited, a deleted
  comment is removed.
- **Closing** an issue posts "Closed as completed/not planned on GitHub." and locks and archives
  the post. Comments on a closed issue still arrive. **Reopening** unlocks it with a note.
  Deleting an issue locks its post. A post is only ever opened for an open issue, so old closed
  issues don't flood the forum.
- **Forum tags** are applied by name, up to five per post: the mod (`ACA`, `BBB`, `AiO`),
  `Done` or `Closed` once closed, and every issue label that has a tag of the same name.
  The `enhancement` label matches `Idea` or `Suggestion`. Create whichever tags you want; the
  ones that don't exist are skipped.
- **The roadmap** is the open issue whose title starts with "Roadmap" (the pinned
  "Roadmap: what's being worked on"). Its body becomes a post in the roadmap forum, split
  across as many messages as it needs and edited in place when the issue changes. The roadmap
  issue doesn't also get a tracker post.

Discord holds the state. A post is recognised by its prefix and by being the bot's own, and a
comment by the GitHub link on its message. Nothing is written back to GitHub, and the workflow
only needs read access. Every run rechecks every open issue and everything updated in the last
two days, so a run that failed or was dropped is caught up by the next. A run with nothing new
sends nothing.

## First installation

1. **Discord channels.** Create two forum channels, for example `#issue-tracker` and
   `#roadmap`. Add the tags you want to the tracker (see above). To keep the tracker a mirror,
   deny `@everyone` **Create Posts** there and allow **Send Messages in Posts** if players may
   reply. Bug reports from players keep going wherever they go now.
2. **Bot.** In the [Discord Developer Portal](https://discord.com/developers/applications),
   choose **New Application**, open **Bot**, turn **Public Bot** off, and **Reset Token** to
   copy the token. No privileged intents are needed. The bot never connects to the Gateway, so
   it shows as offline. That's expected.
3. **Invite it.** Open this address with the application ID from **General Information**:
   `https://discord.com/oauth2/authorize?client_id=APPLICATION_ID&scope=bot&permissions=292057861120`.
   That grants View Channels, Send Messages, Send Messages in Threads, Embed Links, Read
   Message History and Manage Threads. Manage Threads is needed to tag, rename, lock and
   archive posts. You can limit the bot to the two forum channels through their permissions.
4. **Channel IDs.** In Discord, turn on **Settings → Advanced → Developer Mode**, then
   right-click each forum channel and choose **Copy Channel ID**.
5. **GitHub settings, in each of the three repositories:** go to **Settings → Secrets and
   variables → Actions** and add:
   - the secret `DISCORD_BOT_TOKEN` (the bot token);
   - the variables `DISCORD_TRACKER_CHANNEL_ID` and `DISCORD_ROADMAP_CHANNEL_ID`.

   With the GitHub CLI, for each repository:

   ```
   gh secret set DISCORD_BOT_TOKEN -R propst-design/ArmaCommanderReforger
   gh variable set DISCORD_TRACKER_CHANNEL_ID -R propst-design/ArmaCommanderReforger --body 123456789012345678
   gh variable set DISCORD_ROADMAP_CHANNEL_ID -R propst-design/ArmaCommanderReforger --body 123456789012345678
   ```

   Never commit the token or paste it into issues or logs. If it leaks, reset it in the
   Developer Portal and replace the three secrets.
6. **Merge order.** Merge this repository's change first: BetterBattleBuddies and
   AIOCommandMenu call the reusable workflow from `main` here. Then merge theirs.
7. **First run.** In each repository, open **Actions → Discord tracker → Run workflow**. It
   opens a post for every open issue and the roadmap post. Without the secret or the tracker
   variable, a run logs "skipping" and succeeds, so the workflow can be merged before Discord
   is set up.

Leave the roadmap variable unset to mirror issues only. In that case the roadmap issue gets an
ordinary tracker post.

## Maintenance

- **Recheck everything:** run the workflow with **full** ticked. It also rechecks closed
  issues that have a post.
- **A post deleted in Discord** comes back on the next run while its issue is open.
- **Limits:** Discord allows about two renames per post every ten minutes. A rename it refuses
  is retried on the next run. Long issue bodies and comments are cut at about 3,500 characters;
  the post's title and each comment's author line link to the full text on GitHub. A failed run shows in Actions, and the next event or
  the six-hourly run retries.
- **GitHub schedules** in public repositories pause after 60 days without commits. Issue events
  still trigger runs; re-enable the schedule on the Actions page when needed.
- Run the offline tests with `python -m unittest discover -s tools/discord_tracker -p 'test_*.py'`.
