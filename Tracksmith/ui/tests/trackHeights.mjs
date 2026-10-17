import assert from 'node:assert/strict';
import {trackHeight,visibleNoteRows} from '../src/trackHeights.ts';
assert.equal(trackHeight(undefined),126);assert.equal(trackHeight(0,true),323);assert.equal(trackHeight(NaN,true),323);assert.equal(trackHeight(100),126);assert.equal(trackHeight(100,true),216);assert.equal(trackHeight(1000),900);assert.equal(trackHeight(246),246);
assert.equal(visibleNoteRows(216),12);assert.equal(visibleNoteRows(323),24);assert.equal(visibleNoteRows(640),59);assert.equal(visibleNoteRows(900),88);
console.log('Audio/MIDI height defaults and bounds; expanded piano-row counts PASS');
