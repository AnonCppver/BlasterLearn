# Priority Units 11 and 15 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the homepage statistics strip, mark units 11 and 15 as priority systems, and expand both units with evidence-backed algorithm and network-flow detail.

**Architecture:** Keep the 16-card overview and static `main.html` composition. Add a small reusable priority marker to cards 11 and 15, and a reusable full-width deep-dive layout inside those two unit fragments. Rebuild `main.html` from the fragments after every content change.

**Tech Stack:** HTML5, CSS, vanilla JavaScript, Node.js built-in test runner.

---

### Task 1: Add regression contracts

**Files:**
- Modify: `docs/html/tests/site.test.mjs`

- [ ] Assert `home.html` and `main.html` contain no `stats-strip`.
- [ ] Assert exactly two `priority-mark` icons exist in `home.html`, inside the unit 11 and unit 15 cards.
- [ ] Assert units 11 and 15 each contain a `priority-deep-dive` with at least three `deep-dive-block` sections.
- [ ] Assert unit 11 includes `O(1)`, binary lookup, interpolation fraction, filtered `HitTime`, HitScan, Projectile, and server `ApplyDamage()`.
- [ ] Assert unit 15 includes the index-coordinate formulas, `TentativelyClaimed`, the three decision branches, `Remainder`, FastArray callbacks, and Grid widget placement.
- [ ] Run `node --test docs/html/tests/site.test.mjs`; expect failure because the priority layout is not yet present.

### Task 2: Simplify homepage and add priority markers

**Files:**
- Modify: `docs/html/home.html`
- Modify: `docs/html/assets/css/site.css`

- [ ] Delete the entire `stats-strip` section and its unused responsive rules.
- [ ] Change unit 11 and 15 card titles to append `<span class="priority-mark" role="img" aria-label="重点单元">★</span>`.
- [ ] Style the marker as a compact red square aligned to the title baseline, using the existing red, ink, mono, and border variables.

### Task 3: Expand server-side rewind

**Files:**
- Modify: `docs/html/units/11-hit-detection-and-server-side-rewind.html`

- [ ] Add a full-width timeline explaining `GetServerTime() - SingleTripTime` and why RPC arrival time is not the validation time.
- [ ] Add the server history algorithm: 16 fixed HitBoxes, `ECC_HitBox`, server-only Tick, preallocated ring buffer, `O(1)` writes, fixed arrays, externalized BoxExtent, binary lookup, and adjacent-frame interpolation.
- [ ] Add the confirmation algorithm: cache current pose, move historical boxes, disable Mesh collision, head-first pass, body pass, restore pose, then server `ApplyDamage()`.
- [ ] Add a HitScan/Projectile comparison describing submitted parameters and `LineTraceSingleByChannel` versus `PredictProjectilePath`.

### Task 4: Expand inventory pickup and placement

**Files:**
- Modify: `docs/html/units/15-inventory-space-query-and-grid-placement.html`

- [ ] Add the complete `TryAddItem → HasRoomForItem → branch → Server RPC → FastArray/UI` flow.
- [ ] Explain one-dimensional GridSlots versus two-dimensional rectangle checks, bounds formulas, and `ForEach2D` coordinate conversion.
- [ ] Explain first-fit search, `CheckedIndices`, transactional `TentativelyClaimed`, `FirstGridIndex`, and stack fill calculation.
- [ ] Explain no-room, existing-stack, and new-item branches, including `Remainder` handling for full and partial scene pickup.
- [ ] Explain listen-server/local delegate versus remote-client `PostReplicatedAdd`, converging on `AddItemAtIndex` and `UpdateGridSlots`.

### Task 5: Rebuild and verify

**Files:**
- Modify (generated): `docs/html/main.html`

- [ ] Run `node docs/html/tools/build-main.mjs`; expect 17 modules assembled.
- [ ] Run `node --test docs/html/tests/site.test.mjs`; expect zero failures.
- [ ] Run `node --check docs/html/assets/js/site.js`; expect exit code 0.
- [ ] Verify desktop and mobile layouts have no horizontal overflow, exactly two priority icons, no statistics strip, and expanded units 11 and 15.
