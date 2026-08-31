# Blaster All Sixteen Units Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Create one compact technical HTML module for each of the 16 documented systems and statically assemble all modules into `docs/html/main.html` so the page works when opened directly.

**Architecture:** Keep `home.html` and `units/*.html` as focused source modules. `main.html` contains a static copy of those modules because `file://` pages cannot reliably fetch adjacent HTML. Shared CSS and JavaScript provide the visual system, navigation, reveal effects, and active-section tracking without loading content at runtime.

**Tech Stack:** HTML5, CSS, vanilla JavaScript, Node.js built-in test runner.

---

### Task 1: Lock the sixteen-unit contract

**Files:**
- Modify: `docs/html/tests/site.test.mjs`

- [ ] Extend the unit manifest to list `01` through `16` in numeric order.
- [ ] Assert every module begins with a `<section>` whose ID is `unit-NN`.
- [ ] Assert `home.html` contains 16 unit cards and `main.html` contains `top`, `overview`, then `unit-01` through `unit-16`.
- [ ] Assert the header contains 16 tracked unit links.
- [ ] Assert every module has a summary, a compact flow, five technical highlights, and its required implementation terms.
- [ ] Run `node --test docs/html/tests/site.test.mjs` and confirm the test fails because units 04–16 do not exist.

### Task 2: Add media assets

**Files:**
- Create: `docs/html/assets/media/menu-settings.png`
- Create: `docs/html/assets/media/menu-modes.png`
- Create: `docs/html/assets/media/weapon-class-tree.png`
- Create: `docs/html/assets/media/health-shield-buff.gif`
- Create: `docs/html/assets/media/elimination-respawn.gif`
- Create: `docs/html/assets/media/elimination-scoring.gif`
- Create: `docs/html/assets/media/combat-state-reachability.png`
- Create: `docs/html/assets/media/reloading.gif`
- Create: `docs/html/assets/media/pickups.gif`
- Create: `docs/html/assets/media/inventory-interactions.gif`

- [ ] Copy the prepared source images without recompression or image editing.
- [ ] Keep units without screenshots as HTML/CSS flow diagrams.
- [ ] Verify every referenced asset resolves from the `docs/html` root.

### Task 3: Create units 04–08

**Files:**
- Create: `docs/html/units/04-main-menu-steam-sessions.html`
- Create: `docs/html/units/05-weapon-classification-and-data.html`
- Create: `docs/html/units/06-match-state-time-sync-and-latency.html`
- Create: `docs/html/units/07-health-shield-and-buff.html`
- Create: `docs/html/units/08-elimination-scoring-respawn-default-weapon.html`

- [ ] Unit 04: show `UMenu → UMultiplayerSessionsSubsystem → IOnlineSession → Travel`, Steam Session delegates, MatchType filtering, and seamless travel.
- [ ] Unit 05: show `AWeapon` mechanism inheritance, `EWeaponType`, `EFireType`, Projectile association, and ammunition ownership.
- [ ] Unit 06: show authoritative match phases, join-in-progress synchronization, filtered time samples, server-time reconstruction, and local HUD countdown.
- [ ] Unit 07: show authoritative composite health/shield replication, shield-first damage allocation, OnRep presentation, validated recovery Buffs, and conditional Tick.
- [ ] Unit 08: show GameMode settlement, PlayerState/GameState scoring, reliable multicast presentation, delayed respawn, and server-spawned default weapons.

### Task 4: Create units 09–12

**Files:**
- Create: `docs/html/units/09-reloading-strategy-and-combat-state.html`
- Create: `docs/html/units/10-weapon-firing-and-input-feel.html`
- Create: `docs/html/units/11-hit-detection-and-server-side-rewind.html`
- Create: `docs/html/units/12-pickup-server-spawn-and-consumption.html`

- [ ] Unit 09: show Server RPC reload request, CombatState reachability, reload/animation Strategy, AnimNotify commit timing, ammunition/HUD chain, and shell-by-shell loading.
- [ ] Unit 10: show crosshair target computation, muzzle rotation, local predicted fire, multicast deduplication, FireDelay gating, continuous fire, and reload-to-fire continuity.
- [ ] Unit 11: show skeletal HitBoxes, `ECC_HitBox`, server-only ring history, filtered server time, hitscan/projectile confirmation requests, rewind reconstruction, and authoritative `ApplyDamage()`.
- [ ] Unit 12: show authority-only overlap, `Inactive → Active → Consumed`, one-shot consumption guard, derived pickup effects, destruction replication, and spawn-point refresh.

### Task 5: Create units 13–16

**Files:**
- Create: `docs/html/units/13-inventory-item-manifest-and-fragments.html`
- Create: `docs/html/units/14-inventory-fast-array-replication.html`
- Create: `docs/html/units/15-inventory-space-query-and-grid-placement.html`
- Create: `docs/html/units/16-inventory-item-interactions.html`

- [ ] Unit 13: show GameplayTag identity, Manifest aggregation, `TInstancedStruct<FInvFragment>`, serialization, runtime `UInvItem` creation, UI data, and dropped-Actor reconstruction.
- [ ] Unit 14: compare ordinary replicated TArray with `FFastArraySerializer`, explain stable IDs, dirty marking, incremental callbacks, replicated subobjects, and authoritative collection ownership.
- [ ] Unit 15: show `TryAddItem`, room query, 2D bounds/collision checks, tentative claims, stack/new-item Server RPC branches, FastArray notification, and Grid widget placement.
- [ ] Unit 16: show item hit-testing, `FirstGridIndex`, HoverItem region calculation, placement/merge/swap, context menu capabilities, and Server RPC drop/consume paths.

### Task 6: Expand the home index and global navigation

**Files:**
- Modify: `docs/html/home.html`
- Modify: `docs/html/main.html`
- Modify: `docs/html/assets/css/site.css`

- [ ] Add cards for units 04–16 to the existing unit selector.
- [ ] Add tracked links for units 04–16 to the sticky header.
- [ ] Keep desktop navigation compact and make the mobile menu scrollable.
- [ ] Keep all copy technical and remove report/browsing instructions.

### Task 7: Statically assemble and verify

**Files:**
- Modify: `docs/html/main.html`
- Test: `docs/html/tests/site.test.mjs`

- [ ] Insert the exact 16 unit sections in numeric order after the overview.
- [ ] Keep `main.html` free of `data-include` and `fetch()` dependencies.
- [ ] Run `node --test docs/html/tests/site.test.mjs`; expect all tests to pass.
- [ ] Run `node --check docs/html/assets/js/site.js`; expect exit code 0.
- [ ] Open the HTTP preview at desktop and 390×844 widths; verify 16 sections, no missing assets, no horizontal overflow, working anchors, and no console warnings/errors.
