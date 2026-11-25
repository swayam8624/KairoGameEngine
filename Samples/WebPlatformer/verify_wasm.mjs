// Execute the actual compiled module in Node's WebAssembly runtime. The
// native trace executable owns its own vector-backed map. Compare all exposed
// state and map cells after identical inputs, including resets and a full win.
import {readFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import assert from 'node:assert/strict';
const [wasm, native] = process.argv.slice(2);
const {instance} = await WebAssembly.instantiate(readFileSync(wasm), {});
const api = instance.exports;
api.game_reset();
const commands = [];
const append = (axis,jump,n) => {for(let i=0;i<n;++i)commands.push([axis,jump]);};
append(1,0,50); // actual hazard and respawn
append(9,0,1); append(1,0,31); append(1,1,17); append(1,0,80); // win
const fixedCount = commands.length;
let seed = 817;
function random() { seed = (Math.imul(seed,1664525)+1013904223)>>>0; return seed; }
for(let trial=0;trial<100;++trial) {
    append(9,0,1);
    for(let tick=0;tick<200;++tick) append((random()%3)-1,Number(random()%7===0),1);
}
const result = spawnSync(native, [], {input:commands.map(c=>c.join(' ')).join('\n')+'\n',encoding:'utf8',maxBuffer:8*1024*1024,timeout:20000});
assert.equal(result.status,0,result.stderr);
const lines = result.stdout.trimEnd().split('\n');
assert.equal(lines.length,commands.length);
function snapshot() {
    let map=''; for(let y=0;y<6;++y)for(let x=0;x<16;++x)map+=String.fromCharCode(api.game_tile(x,y));
    return [api.game_x(),api.game_y(),api.game_coins(),api.game_deaths(),api.game_won(),map].join(' ');
}
for(let i=0;i<commands.length;++i) {
    const [axis,jump] = commands[i];
    if(axis===9)api.game_reset();else assert.equal(api.game_step(axis,jump),1);
    assert.equal(snapshot(),lines[i],`native/WASM divergence at ${i}`);
    if(i===49)assert.equal(api.game_deaths(),1);
    if(i===fixedCount-1) {assert.equal(api.game_won(),1);assert.equal(api.game_coins(),2);}
}
const before=snapshot();assert.equal(api.game_step(2,0),0);assert.equal(snapshot(),before);
console.log(`${commands.length} native/WASM state and map comparisons passed; death and win verified`);
