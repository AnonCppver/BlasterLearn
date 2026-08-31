import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const htmlRoot = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const mainPath = resolve(htmlRoot, 'main.html');
const sources = [
  'home.html',
  'units/01-crosshair-team-identification.html',
  'units/02-aim-offset-turn-in-place.html',
  'units/03-weapon-attachment-fabrik.html',
  'units/04-main-menu-steam-sessions.html',
  'units/05-weapon-classification-and-data.html',
  'units/06-match-state-time-sync-and-latency.html',
  'units/07-health-shield-and-buff.html',
  'units/08-elimination-scoring-respawn-default-weapon.html',
  'units/09-reloading-strategy-and-combat-state.html',
  'units/10-weapon-firing-and-input-feel.html',
  'units/11-hit-detection-and-server-side-rewind.html',
  'units/12-pickup-server-spawn-and-consumption.html',
  'units/13-inventory-item-manifest-and-fragments.html',
  'units/14-inventory-fast-array-replication.html',
  'units/15-inventory-space-query-and-grid-placement.html',
  'units/16-inventory-item-interactions.html',
];

const indent = (html) => html.trim().split(/\r?\n/).map((line) => `    ${line}`).join('\n');
const modules = sources
  .map((source) => `    <!-- Source: ${source} -->\n${indent(readFileSync(resolve(htmlRoot, source), 'utf8'))}`)
  .join('\n\n');

const shell = readFileSync(mainPath, 'utf8');
const mainPattern = /  <main id="main">[\s\S]*?  <\/main>/;
if (!mainPattern.test(shell)) throw new Error('main.html is missing its main shell');

const output = shell.replace(mainPattern, `  <main id="main">\n${modules}\n  </main>`);
writeFileSync(mainPath, output, 'utf8');
console.log(`Assembled ${sources.length} HTML modules into main.html`);
