import assert from 'node:assert/strict';
import {formatCounter,parseCounter,selectionRoll} from '../src/transportTiming.ts';
for(const bpm of [20,97,120,300])for(const rate of [44100,48000,96000])for(const seconds of [0,.125,1.99999,60,3599.9999,86400])for(const fmt of ['bars','time','samples'])for(const length of [false,true]){
 const str=formatCounter(seconds,bpm,rate,fmt,length),parsed=parseCounter(str,bpm,rate,fmt,length),tolerance=fmt==='bars'?60/bpm/960:fmt==='samples'?1/rate:.001;
 assert.ok(parsed!==null&&Math.abs(parsed-seconds)<=tolerance+1e-8,`${fmt} ${str} at ${bpm}`);
}
assert.equal(formatCounter(4,120,48000,'bars'),'3 | 1 | 000');assert.equal(formatCounter(4,120,48000,'bars',true),'2 | 0 | 000');assert.equal(parseCounter('3|1|000',120,48000,'bars'),4);
for(const [text,fmt] of [['','time'],['NaN','time'],['1e3','time'],['-1','time'],['0:61','time'],['0|1|000','bars'],['1|5|000','bars'],['1|1|960','bars'],['12.2','samples'],['999999999999999','samples']])assert.equal(parseCounter(text,120,48000,fmt),null);
assert.deepEqual(selectionRoll(8,12,120,1,2,30),{start:6,end:16});assert.deepEqual(selectionRoll(1,3,120,8,8,4),{start:0,end:4});assert.equal(selectionRoll(3,2,120,0,0,30),null);
console.log('Counter format/parse round trips, position versus duration origins, invalid input rejection and selection roll clamping PASS');
