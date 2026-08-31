# Blaster Composed Main Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split the portfolio homepage and each feature unit into independent HTML files, then assemble them into one technical long-form page through `main.html`.

**Architecture:** `main.html` owns the global header, footer, navigation, and include slots. `home.html` and the three files under `units/` contain isolated `<section>` fragments; `site.js` loads those fragments in document order before initializing navigation, reveal effects, and reading progress. `index.html` remains a compatibility entry that redirects to `main.html`.

**Tech Stack:** HTML5 fragments, CSS3, vanilla JavaScript Fetch API, Node.js built-in test runner.

---

### Task 1: Define the composition contract

**Files:**
- Modify: `docs/html/tests/site.test.mjs`

- [ ] Write tests requiring `main.html`, `home.html`, three unit fragments, ordered include slots, placeholder GitHub/download buttons, technical-only copy, and resolvable local assets.
- [ ] Run `node --test docs/html/tests/site.test.mjs` and verify failure because the split files and include contract do not exist.

### Task 2: Split homepage and unit content

**Files:**
- Create: `docs/html/home.html`
- Create: `docs/html/units/01-crosshair-team-identification.html`
- Create: `docs/html/units/02-aim-offset-turn-in-place.html`
- Create: `docs/html/units/03-weapon-attachment-fabrik.html`

- [ ] Move the hero, technical metrics, and system index into `home.html`; add disabled placeholder buttons for GitHub and project download.
- [ ] Move each existing feature section into its corresponding unit file without changing the verified technical facts.
- [ ] Remove portfolio-reporting phrases, browsing instructions, documented-unit counts, and other nontechnical commentary.

### Task 3: Assemble the final page

**Files:**
- Create: `docs/html/main.html`
- Modify: `docs/html/index.html`
- Modify: `docs/html/assets/js/site.js`
- Modify: `docs/html/assets/css/site.css`

- [ ] Add four ordered `data-include` slots to `main.html` and preserve the fixed navigation/footer.
- [ ] Load fragment files with Fetch, insert them before initializing scroll behavior, and show a concise Chinese load error if a fragment cannot be retrieved.
- [ ] Prevent placeholder GitHub/download buttons from navigating until real URLs are supplied.
- [ ] Redirect `index.html` to `main.html` while retaining a visible fallback link.

### Task 4: Verify behavior and layout

**Files:**
- Verify: `docs/html/main.html`
- Verify: `docs/html/home.html`
- Verify: `docs/html/units/*.html`

- [ ] Run `node --test docs/html/tests/site.test.mjs` and require all checks to pass.
- [ ] Serve `docs/html`, open `main.html`, and verify all four fragments load in order with no console errors.
- [ ] Check desktop and mobile widths, navigation anchors, the UE5 hero mark, and placeholder button state.

## Execution note

The user explicitly requested direct writes to `docs/html`. No branch, commit, or game-source modification is included; unrelated Unreal assets and source changes remain untouched.

