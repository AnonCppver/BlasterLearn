import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const siteRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const files = {
  entry: 'index.html',
  main: 'main.html',
  home: 'home.html',
  unit01: 'units/01-crosshair-team-identification.html',
  unit02: 'units/02-aim-offset-turn-in-place.html',
  unit03: 'units/03-weapon-attachment-fabrik.html',
  unit04: 'units/04-main-menu-steam-sessions.html',
  unit05: 'units/05-weapon-classification-and-data.html',
  unit06: 'units/06-match-state-time-sync-and-latency.html',
  unit07: 'units/07-health-shield-and-buff.html',
  unit08: 'units/08-elimination-scoring-respawn-default-weapon.html',
  unit09: 'units/09-reloading-strategy-and-combat-state.html',
  unit10: 'units/10-weapon-firing-and-input-feel.html',
  unit11: 'units/11-hit-detection-and-server-side-rewind.html',
  unit12: 'units/12-pickup-server-spawn-and-consumption.html',
  unit13: 'units/13-inventory-item-manifest-and-fragments.html',
  unit14: 'units/14-inventory-fast-array-replication.html',
  unit15: 'units/15-inventory-space-query-and-grid-placement.html',
  unit16: 'units/16-inventory-item-interactions.html',
  css: 'assets/css/site.css',
  js: 'assets/js/site.js',
  logo: 'assets/media/ue5-mark.svg',
};

const unitFiles = Array.from({ length: 16 }, (_, index) => files[`unit${String(index + 1).padStart(2, '0')}`]);
const unitIds = Array.from({ length: 16 }, (_, index) => `unit-${String(index + 1).padStart(2, '0')}`);

const read = (relativePath) => {
  const absolutePath = resolve(siteRoot, relativePath);
  assert.ok(existsSync(absolutePath), `${relativePath} should exist`);
  return readFileSync(absolutePath, 'utf8');
};

test('homepage, units, and final assembly are independent HTML files', () => {
  for (const file of Object.values(files)) read(file);
  for (const [index, fragment] of unitFiles.entries()) {
    const html = read(fragment).trim();
    assert.match(html, /^<section\b/);
    assert.match(html, new RegExp(`id=["']${unitIds[index]}["']`));
  }
});

test('main.html statically assembles all fragments in the required order', () => {
  const html = read(files.main);
  const sectionIds = [...html.matchAll(/<section\b[^>]*\bid=["']([^"']+)["']/g)]
    .map((match) => match[1]);
  assert.deepEqual(sectionIds, ['top', 'overview', ...unitIds]);
  assert.doesNotMatch(html, /data-include=/);
  assert.match(html, /<script[^>]+src=["']assets\/js\/site\.js["']/);
  assert.doesNotMatch(read(files.js), /fetch\s*\(/);
  assert.equal((html.match(/data-track-link/g) ?? []).length, 16);
});

test('UE5 logo has no red line decoration around the ring', () => {
  assert.doesNotMatch(read(files.logo), /stroke=["']#f0442e["']/i);
});

test('index.html remains a compatibility entry for main.html', () => {
  const html = read(files.entry);
  assert.match(html, /url=main\.html/i);
  assert.match(html, /href=["']main\.html["']/);
});

test('home provides reserved GitHub and download buttons', () => {
  const html = read(files.home);
  assert.match(html, /data-placeholder-link[^>]*>[\s\S]*?GitHub/i);
  assert.match(html, /data-placeholder-link[^>]*>[\s\S]*?下载项目/);
  assert.equal((html.match(/data-placeholder-link/g) ?? []).length, 2);
  assert.equal((html.match(/class=["']unit-card(?: unit-card--priority)?["']/g) ?? []).length, 16);
});

test('homepage removes the statistics strip and marks units 06, 10, 11, and 15 as priorities', () => {
  const home = read(files.home);
  const main = read(files.main);
  assert.doesNotMatch(home, /stats-strip/);
  assert.doesNotMatch(main, /stats-strip/);
  assert.equal((home.match(/class=["']priority-mark["']/g) ?? []).length, 4);
  assert.match(home, /class=["']unit-card unit-card--priority["'] href=["']#unit-06["'][\s\S]*?priority-mark[\s\S]*?<h3>比赛状态与时间同步<\/h3>/);
  assert.match(home, /class=["']unit-card unit-card--priority["'] href=["']#unit-10["'][\s\S]*?priority-mark[\s\S]*?<h3>射击链路与输入手感<\/h3>/);
  assert.match(home, /class=["']unit-card unit-card--priority["'] href=["']#unit-11["'][\s\S]*?priority-mark[\s\S]*?<h3>命中确认与服务器回溯<\/h3>/);
  assert.match(home, /class=["']unit-card unit-card--priority["'] href=["']#unit-15["'][\s\S]*?priority-mark[\s\S]*?<h3>空间查询与物品落位<\/h3>/);
  assert.doesNotMatch(home, /<h3>[^<]*<span class=["']priority-mark["']/);
  assert.match(read(files.css), /\.priority-mark\s*\{/);
  assert.match(read(files.css), /\.unit-card--priority\s*\{/);
});

test('all reporting and browsing-instruction copy is removed', () => {
  const content = [files.main, files.home, ...unitFiles]
    .map(read)
    .join('\n');
  const bannedCopy = [
    '点击单元直接滚动到对应内容',
    '继续下滑依次浏览',
    '每个单元只保留功能结果',
    '已整理功能单元',
    '作品集从玩家能够感知的效果出发',
    '个人技术作品',
    '浏览功能单元',
    '选择功能单元',
    '技术作品集',
  ];
  for (const phrase of bannedCopy) {
    assert.equal(content.includes(phrase), false, `reporting copy should be removed: ${phrase}`);
  }
});

test('home copy is focused on implementation technologies', () => {
  const html = read(files.home);
  for (const term of ['Unreal Engine 5.4', 'C++', 'Server RPC', 'Replication', 'Server-Side Rewind']) {
    assert.ok(html.includes(term), `home should mention ${term}`);
  }
});

test('each unit contains its technical contract and five highlights', () => {
  const requirements = {
    [files.unit01]: ['ECC_Visibility', 'IInteractWithCrosshairsInterface', 'PlayerState', 'CrosshairSpread'],
    [files.unit02]: ['AO_Yaw', 'NormalizedDeltaRotator', 'Rotate Root Bone', 'FInterpTo'],
    [files.unit03]: ['COND_OwnerOnly', 'RightHandSocket', 'LeftHandSocket', 'FABRIK'],
    [files.unit04]: ['UMultiplayerSessionsSubsystem', 'MatchType', 'IOnlineSession', 'ClientTravel'],
    [files.unit05]: ['AWeapon', 'AHitScanWeapon', 'AProjectileWeapon', 'EWeaponType'],
    [files.unit06]: ['WaitingToStart', 'GetServerTime', 'ClientServerDelta', 'SingleTripTime'],
    [files.unit07]: ['FReplicatedVitalState', 'OnRep', 'UBuffComponent', 'SetComponentTickEnabled'],
    [files.unit08]: ['ABlasterGameMode', 'PlayerState', 'GameState', 'RequestRespawn'],
    [files.unit09]: ['ServerReload', 'CombatState', 'AnimNotify', 'AmountToReload'],
    [files.unit10]: ['HitTarget', 'ServerFire', 'MulticastFire', 'FireDelay'],
    [files.unit11]: ['ECC_HitBox', 'Server-Side Rewind', 'ServerScoreRequest', 'ApplyDamage'],
    [files.unit12]: ['APickupSpawnPoint', 'Inactive', 'Consumed', 'bConsumed'],
    [files.unit13]: ['GameplayTag', 'FInvItemManifest', 'TInstancedStruct', 'FInvFragment'],
    [files.unit14]: ['FFastArraySerializer', 'MarkItemDirty', 'PostReplicatedAdd', 'ReplicateSubobjects'],
    [files.unit15]: ['TryAddItem', 'HasRoomForItem', 'TentativelyClaimed', 'Server_AddNewItem'],
    [files.unit16]: ['FirstGridIndex', 'HoverItem', 'SwapWithHoverItem', 'Server_DropItem'],
  };

  for (const [file, terms] of Object.entries(requirements)) {
    const html = read(file);
    assert.equal((html.match(/<li>/g) ?? []).length, 5, `${file} should have five highlights`);
    assert.match(html, /class=["']feature-summary["']/);
    assert.match(html, /class=["']compact-flow["']/);
    for (const term of terms) assert.ok(html.includes(term), `${file} should mention ${term}`);
  }
});

test('animated captures use an uncropped GIF media treatment', () => {
  const gifUnits = [files.unit01, files.unit02, files.unit07, files.unit08, files.unit12, files.unit16];
  for (const file of gifUnits) {
    const html = read(file);
    assert.match(html, /class=["'][^"']*feature-media[^"']*gif-media[^"']*["'][\s\S]*?<img[^>]+\.gif["']/i);
  }

  const css = read(files.css);
  assert.match(css, /\.feature-media\.gif-media\s+img\s*\{[^}]*object-fit\s*:\s*contain/i);
  assert.match(css, /\.feature-media\.gif-media\s+img\s*\{[^}]*height\s*:\s*auto/i);
  assert.match(css, /\.feature-media\.gif-media\s+\.media-label\s*\{[^}]*top\s*:\s*auto[^}]*bottom\s*:\s*18px/i);
});

test('priority units provide extended algorithm and network-flow detail', () => {
  const unit10 = read(files.unit10);
  const unit11 = read(files.unit11);
  const unit15 = read(files.unit15);

  for (const [file, html] of [[files.unit10, unit10], [files.unit11, unit11], [files.unit15, unit15]]) {
    assert.match(html, /class=["'][^"']*priority-deep-dive[^"']*["']/);
    assert.ok((html.match(/class=["'][^"']*deep-dive-block[^"']*["']/g) ?? []).length >= 3,
      `${file} should contain at least three deep-dive blocks`);
  }

  for (const term of ['DeprojectScreenToWorld', 'DistanceToCharacter + 100.f', 'LocalFire', 'ServerFire', 'MulticastFire', 'bCanFire', 'FireTimerFinished', 'OnRep_Ammo']) {
    assert.ok(unit10.includes(term), `unit 10 should explain ${term}`);
  }

  for (const term of ['GetServerTime() - SingleTripTime', 'O(1)', '二分查找', 'Fraction', 'LineTraceSingleByChannel', 'PredictProjectilePath', 'ApplyDamage()']) {
    assert.ok(unit11.includes(term), `unit 11 should explain ${term}`);
  }
  for (const term of ['Index = Y * Col + X', 'TentativelyClaimed', 'CheckedIndices', 'Remainder', 'Server_AddStacksToItem', 'PostReplicatedAdd', 'UpdateGridSlots']) {
    assert.ok(unit15.includes(term), `unit 15 should explain ${term}`);
  }
});

test('fragment media and shell assets resolve from the composed page root', () => {
  const content = [files.main, files.home, ...unitFiles]
    .map(read)
    .join('\n');
  const assetPattern = /(?:href|src)=["']((?:assets\/)[^"']+)["']/g;
  for (const match of content.matchAll(assetPattern)) {
    assert.ok(existsSync(resolve(siteRoot, match[1])), `broken composed asset: ${match[1]}`);
  }
});
