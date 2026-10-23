# Public API regression samples

Captured directly from `https://api.reforgermods.net/v2/mods/{id}`,
`/versions` and `/versions/{current-version}` on 2026-10-23.
The three IDs are configured in the parent directory's `mods.json`.
Files preserve the actual successful JSON response schemas and changelog text.
They contain no request headers, tokens or webhook credentials. They are test
inputs, not current release/status assertions. Tests use copies of these responses
to exercise publication transitions and outages without altering these samples.
