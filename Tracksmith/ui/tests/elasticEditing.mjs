import assert from 'node:assert/strict';
import {validElasticMap,addElasticAnchor} from '../src/elasticEditing.ts';
const initial=[{source:0,target:0},{source:2,target:2}],added=addElasticAnchor(initial);
assert.deepEqual(added,[{source:0,target:0},{source:1,target:1},{source:2,target:2}]);
assert.ok(validElasticMap([{source:0,target:0},{source:1,target:1.2},{source:2,target:2}],2));
for(const map of [[{source:0,target:0},{source:1,target:1.9},{source:2,target:2}],[{source:0,target:0},{source:1,target:NaN},{source:2,target:2}],[{source:0,target:.1},{source:2,target:2}],[{source:0,target:0},{source:1,target:1},{source:.5,target:1.5},{source:2,target:2}]])assert.equal(validElasticMap(map,2),false);
assert.equal(validElasticMap(initial,31),false);assert.deepEqual(initial,[{source:0,target:0},{source:2,target:2}]);
console.log('Elastic UI map validation: fixed edges, ratios, monotonicity, invalid numbers, length limits and immutable anchor insertion PASS');
