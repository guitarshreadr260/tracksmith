import assert from 'node:assert/strict';
import {validElasticMap,addElasticAnchor,quantizeElasticMap,snapWarpTime} from '../src/elasticEditing.ts';
const initial=[{source:0,target:0},{source:2,target:2}],added=addElasticAnchor(initial);
assert.deepEqual(added,[{source:0,target:0},{source:1,target:1},{source:2,target:2}]);
assert.ok(validElasticMap([{source:0,target:0},{source:1,target:1.2},{source:2,target:2}],2));
for(const map of [[{source:0,target:0},{source:1,target:1.9},{source:2,target:2}],[{source:0,target:0},{source:1,target:NaN},{source:2,target:2}],[{source:0,target:.1},{source:2,target:2}],[{source:0,target:0},{source:1,target:1},{source:.5,target:1.5},{source:2,target:2}]])assert.equal(validElasticMap(map,2),false);
assert.equal(validElasticMap(initial,31),false);assert.deepEqual(initial,[{source:0,target:0},{source:2,target:2}]);
console.log('Elastic UI map validation: fixed edges, ratios, monotonicity, invalid numbers, length limits and immutable anchor insertion PASS');

assert.equal(snapWarpTime(.12,.13,120,.25),.12); // project phase, not clip-relative zero
const loose=[{source:0,target:0},{source:.43,target:.43},{source:.97,target:.97},{source:1.51,target:1.51},{source:2,target:2}];
const half=quantizeElasticMap(loose,2,0,120,.25,.5),full=quantizeElasticMap(loose,2,0,120,.25,1);
assert.ok(validElasticMap(half.anchors,2)&&validElasticMap(full.anchors,2));assert.equal(half.anchors[1].target,(.43+.375)/2);assert.equal(full.anchors[1].target,.375);assert.equal(loose[1].target,.43);
const capped=quantizeElasticMap([{source:0,target:0},{source:.1,target:.1},{source:2,target:2}],2,0,120,4,1);assert.ok(capped.appliedStrength<1&&validElasticMap(capped.anchors,2));assert.equal(capped.anchors.at(-1).target,2);
console.log('Project-phase grid snap, noncumulative quantization, strength bounds and pinned edges PASS');
