import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import ts from 'typescript';
import {createCommandRegistry} from '../src/commands.ts';
const source=readFileSync(new URL('../src/Studio.tsx',import.meta.url),'utf8');
const start=source.indexOf('function key(e:KeyboardEvent)'),end=source.indexOf(' }window.addEventListener',start)+2;
assert.ok(start>=0&&end>start);
const handler=ts.transpileModule(source.slice(start,end)+';return key;', {compilerOptions:{target:ts.ScriptTarget.ES2022}}).outputText;
function press(code,options={}){
 const actions=[],context={audio:{},ioOpen:false,launcher:false,details:false,instrumentPicker:false,shuttle:d=>actions.push(d),lastShuttle:{current:0},...options};
 context.registry=createCommandRegistry([{id:'zoom.in',menu:'View',label:'In',shortcut:'T',repeatable:true,enabled:true,run:()=>actions.push('in')},{id:'zoom.out',menu:'View',label:'Out',shortcut:'R',repeatable:true,enabled:true,run:()=>actions.push('out')}]);
 let prevented=false;
 const key=new Function(...Object.keys(context),handler)(...Object.values(context));
 key({code,ctrlKey:false,altKey:false,metaKey:false,shiftKey:false,repeat:false,isComposing:false,defaultPrevented:false,...options,target:{closest:selector=>selector.includes('input')?options.editable:options.menu},preventDefault(){prevented=true}});
 return {actions,prevented};
}
assert.deepEqual(press('KeyR').actions,['out']);assert.deepEqual(press('KeyT').actions,['in']);
assert.deepEqual(press('KeyT',{repeat:true}).actions,['in']);
for(const option of ['editable','menu','ioOpen','launcher','details','instrumentPicker','isComposing','defaultPrevented','ctrlKey','altKey','metaKey','shiftKey'])for(const code of ['KeyR','KeyT'])assert.deepEqual(press(code,{[option]:true}).actions,[],`${code} blocked by ${option}`);
assert.equal(press('KeyT').prevented,true);assert.equal(press('KeyQ').prevented,false);
assert.deepEqual(press('Numpad1').actions,[-1]);assert.deepEqual(press('Numpad2').actions,[1]);
console.log('Current Studio shortcut handler: R/T, repeat, typing/modifier/modal/menu/IME/handled-event guards and shuttle PASS');

assert.deepEqual(press('KeyT',{audio:{stemPlan:{stage:'ready'}}}).actions,[]);
