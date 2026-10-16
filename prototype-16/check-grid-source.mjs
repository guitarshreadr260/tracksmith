import assert from 'node:assert/strict';
import {musicalGrid,snapBeat} from './Tracksmith/ui/src/musicalGrid.ts';
assert.equal(musicalGrid(64,1024,.25).labels[1].bar,3);
assert.equal(musicalGrid(64,4096,.25).subPixels,16);
assert.equal(snapBeat(1.13,.25),1.25);
assert.ok(Math.abs(snapBeat(1.15,1/3)-1)<1e-8);
assert.ok(Number.isFinite(musicalGrid(64,0,.25).barPixels));
assert.equal(musicalGrid(64,4096,.25).labels[4].bar,2);
console.log('Actual grid module: bar labels, adaptive subdivisions, sixteenth/triplet snap and hidden-ruler handling passed.');
