import {readFileSync} from 'node:fs';
import ts from 'typescript';
import assert from 'node:assert/strict';
const source=readFileSync(new URL('../src/InlineMidiNotes.tsx',import.meta.url),'utf8'),helpers=source.slice(source.indexOf('export function pitchTop'),source.indexOf('export function InlineMidiKeys'));
const code=ts.transpileModule(helpers,{compilerOptions:{target:ts.ScriptTarget.ES2022,module:ts.ModuleKind.CommonJS}}).outputText,exports={};new Function('exports',code)(exports);
assert.equal(exports.barBeat(0),'1 | 1 | 000');assert.equal(exports.barBeat(8),'3 | 1 | 000');assert.equal(exports.barBeat(4-.00001),'2 | 1 | 000');assert.equal(exports.barBeat(.25),'1 | 1 | 240');
assert.equal(exports.pitchTop([]),59);assert.equal(exports.pitchTop([{pitch:36},{pitch:38},{pitch:42}]),49);assert.equal(exports.pitchTop([{pitch:127}],24),127);assert.equal(exports.pitchTop([{pitch:0}],-24),23);
console.log('In-track pitch register, keyboard bounds and bar/beat/tick cursor formatting PASS');
