<!--
Thanks for contributing! Keep a PR focused on one thing. See CONTRIBUTING.md and AI_USAGE.md.
-->

## What and why

<!-- What this changes, and the reason for it. Link any related issue (Fixes #123). -->

## Hardware verification (required)

<!--
This is firmware: a change that compiles is not a change that works. A PR that affects behaviour
must be flashed to a real board and confirmed working. Fill this in, or say why it's exempt
(docs-only / comment-only changes are the exception).
-->

- **Board(s) tested:**
- **PHP version(s):**
- **What you saw (serial output / observed behaviour):**

```text
paste the relevant serial log here (redact secrets — WiFi passwords, .env values, tokens)
```

## Checklist

- [ ] Verified on real hardware (above), or this is a docs/comment-only change.
- [ ] The vendored PHP engine is **not** edited in place — any target change is a build-time patch under `components/php/versions/<ver>/patches/`.
- [ ] `python3 scripts/check-manifest.py <version>` is green for every version I touched (if I changed a build flag, board, or extension).
- [ ] No `build/` artifacts or generated files committed.
- [ ] Docs and `CHANGELOG.md` updated if this adds or changes a feature.
- [ ] I understand and stand behind this code (see the [AI usage policy](https://github.com/php-baremetal/php-esp32/blob/master/AI_USAGE.md) if an assistant helped write it).
