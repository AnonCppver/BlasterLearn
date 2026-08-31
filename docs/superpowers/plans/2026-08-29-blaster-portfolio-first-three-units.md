# Blaster Portfolio First Three Units Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a local static portfolio homepage and three detailed feature pages in the approved competitive-HUD style.

**Architecture:** Use framework-free HTML, one shared CSS file, and one small progressive-enhancement JavaScript file. Reuse the existing GIF assets by relative path and validate page structure and local links with a Node built-in test script.

**Tech Stack:** HTML5, CSS3, vanilla JavaScript, Node.js built-in test runner.

---

### Task 1: Static-site contract tests

**Files:**
- Create: `docs/html/tests/site.test.mjs`

- [ ] **Step 1: Write a failing test**

Create tests that require `index.html`, three detail pages, shared CSS/JS, homepage anchors, page-to-page navigation, required technical terms, and resolvable local media references.

- [ ] **Step 2: Run test to verify it fails**

Run: `node --test docs/html/tests/site.test.mjs`

Expected: FAIL because `docs/html/index.html` and the three detail pages do not exist.

### Task 2: Shared visual system and homepage

**Files:**
- Create: `docs/html/assets/css/site.css`
- Create: `docs/html/assets/js/site.js`
- Create: `docs/html/index.html`

- [ ] **Step 1: Implement the shared competitive-HUD visual system**

Define local typography fallbacks, red/black design tokens, responsive grids, navigation, feature panels, diagrams, code blocks, media frames, reading progress, focus states, and reduced-motion behavior.

- [ ] **Step 2: Implement the homepage**

Add project hero, unit navigator, three sequential preview sections, technical metrics, working detail-page links, and an overview of the UE5.4 multiplayer project.

- [ ] **Step 3: Run the contract tests**

Expected: tests still fail only for detail pages or their required content.

### Task 3: Three detail pages

**Files:**
- Create: `docs/html/units/01-crosshair-team-identification.html`
- Create: `docs/html/units/02-aim-offset-turn-in-place.html`
- Create: `docs/html/units/03-weapon-attachment-fabrik.html`

- [ ] **Step 1: Implement unit 01**

Present the local trace pipeline, `ECC_Visibility` collision matrix, interface-driven team query, replicated `PlayerState` data, HUD color, and movement-driven spread.

- [ ] **Step 2: Implement unit 02**

Present `AO_Yaw`, `NormalizedDeltaRotator`, `Rotate Root Bone`, `±90°` entry, `15°` exit, `FInterpTo`, ADS behavior, and remote pitch mapping.

- [ ] **Step 3: Implement unit 03**

Present AreaSphere overlap, OwnerOnly pickup state, server-authoritative equip, primary/secondary sockets, left-hand bone-space conversion, FABRIK, and candidate-set improvement.

- [ ] **Step 4: Run the contract tests**

Run: `node --test docs/html/tests/site.test.mjs`

Expected: all tests pass with zero broken local links.

### Task 4: Visual verification

**Files:**
- Verify: `docs/html/index.html`
- Verify: `docs/html/units/*.html`

- [ ] **Step 1: Serve the docs directory locally**

Use a local static server rooted at `docs/html`.

- [ ] **Step 2: Inspect desktop and mobile layouts**

Check hero hierarchy, sticky navigation, GIF sizing, diagram legibility, horizontal overflow, focus states, and all cross-page navigation.

- [ ] **Step 3: Re-run tests after visual fixes**

Run: `node --test docs/html/tests/site.test.mjs`

Expected: all tests remain green.

## Execution note

The user explicitly requested files be written directly to `docs/html`; execution therefore stays in the current working tree. No commits are created because the repository already contains unrelated user changes and the task does not authorize integrating or committing them.

