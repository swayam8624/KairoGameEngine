// The WASM module owns all game state. JavaScript translates browser input
// into unit axes/fixed ticks and paints authoritative state into Canvas 2D.
const canvas = document.querySelector('#game');
const ctx = canvas.getContext('2d', {willReadFrequently: true});
const status = document.querySelector('#status');
const pressed = new Set();
const scale = 3;
let api, previous, accumulator = 0;
const controls = ['ArrowLeft', 'ArrowRight', 'KeyA', 'KeyD', 'Space'];

function key(event, down) {
    if (!controls.includes(event.code)) return;
    event.preventDefault();
    if (down) pressed.add(event.code); else pressed.delete(event.code);
}
window.addEventListener('keydown', e => key(e, true));
window.addEventListener('keyup', e => key(e, false));
function clearInput() { pressed.clear(); previous = undefined; accumulator = 0; }
window.addEventListener('blur', clearInput);
document.addEventListener('visibilitychange', clearInput);
for (const button of document.querySelectorAll('[data-key]')) {
    const code = button.dataset.key;
    button.addEventListener('pointerdown', event => {
        button.setPointerCapture(event.pointerId); pressed.add(code);
    });
    for (const name of ['pointerup', 'pointercancel', 'lostpointercapture'])
        button.addEventListener(name, () => pressed.delete(code));
}
document.querySelector('#reset').addEventListener('click', () => {
    if (api) { clearInput(); api.game_reset(); draw(); }
});
function tick() {
    const left = pressed.has('ArrowLeft') || pressed.has('KeyA');
    const right = pressed.has('ArrowRight') || pressed.has('KeyD');
    if (!api.game_step(Number(right) - Number(left), Number(pressed.has('Space'))))
        throw new Error('Simulation rejected browser input');
}
function draw() {
    ctx.fillStyle = '#0d1726'; ctx.fillRect(0, 0, canvas.width, canvas.height);
    const colors = {'#':'#52657b', 'C':'#ffc94a', 'H':'#e75b58', 'G':'#42b879'};
    for (let y=0; y<6; ++y) for (let x=0; x<16; ++x) {
        const tile = String.fromCharCode(api.game_tile(x,y));
        if (!colors[tile]) continue;
        ctx.fillStyle = colors[tile];
        if (tile === 'C') ctx.fillRect((x*16+5)*scale,(y*16+5)*scale,6*scale,6*scale);
        else ctx.fillRect(x*16*scale,y*16*scale,16*scale,16*scale);
    }
    ctx.fillStyle = '#50c9fc'; ctx.fillRect(api.game_x()*scale,api.game_y()*scale,10*scale,14*scale);
    const text = api.game_won() ? 'Level complete!' : `Crystals ${api.game_coins()}/2 · Deaths ${api.game_deaths()}`;
    if (status.textContent !== text) status.textContent = text;
}
function frame(now) {
    if (previous !== undefined && !document.hidden) accumulator += Math.min(.1, (now-previous)/1000);
    previous = now;
    while (accumulator >= 1/60) { tick(); accumulator -= 1/60; }
    draw(); requestAnimationFrame(frame);
}
// Headless acceptance uses the real browser event handlers and same tick/
// paint functions. It checks hazard reset, the complete objective and pixels.
function smoke() {
    const send = (code, down) => window.dispatchEvent(new KeyboardEvent(down?'keydown':'keyup',{code,cancelable:true}));
    const ticks = n => { for(let i=0;i<n;++i) tick(); };
    draw(); const before = ctx.getImageData(0,0,canvas.width,canvas.height).data;
    send('ArrowRight',true); ticks(50); send('ArrowRight',false);
    if (api.game_deaths() !== 1) throw new Error('Hazard/respawn failed');
    api.game_reset(); clearInput();
    send('ArrowRight',true); ticks(31); send('Space',true); ticks(17);
    send('Space',false); ticks(80); send('ArrowRight',false);
    draw(); const after = ctx.getImageData(0,0,canvas.width,canvas.height).data;
    if (!api.game_won() || api.game_coins()!==2 || api.game_deaths()!==0 || !before.some((v,i)=>v!==after[i]))
        throw new Error('Win or rendered pixel change failed');
    document.body.dataset.smoke = 'passed';
}
try {
    const response = await fetch('platformer.wasm');
    if (!response.ok) throw new Error(`WASM fetch ${response.status}`);
    const module = await WebAssembly.instantiate(await response.arrayBuffer(), {});
    api = module.instance.exports; api.game_reset();
    if (new URLSearchParams(location.search).has('selftest')) smoke();
    else requestAnimationFrame(frame);
} catch (error) {
    status.textContent = `Game failed: ${error.message}`;
    document.body.dataset.smoke = 'failed';
}
