import fs from 'fs'; import vm from 'vm';
import { createRequire } from 'module';
const require = createRequire(import.meta.url);   // ← 脚本在 report/ 下，能找到 node_modules
const { JSDOM } = require('jsdom');
const dom = new JSDOM('<!DOCTYPE html><body></body>', { pretendToBeVisual: true });
const { window } = dom;
const bbox = (el) => { const t=el.textContent||''; let u=0;
  for (const ch of t) u += /[\u2e80-\u9fff\uff00-\uffef]/.test(ch)?1:0.55;
  return { x:0,y:0,width:Math.max(8,u*16),height:16 }; };
for (const p of [window.Element.prototype, window.SVGElement.prototype]) {
  p.getBBox = function(){ return bbox(this); };
  p.getComputedTextLength = function(){ return bbox(this).width; };
}
globalThis.window=window; globalThis.document=window.document;
for (const k of ['CSSStyleSheet','CSSRule','CSSStyleRule','CSSMediaRule','CSSKeyframesRule','HTMLElement','SVGElement','Element','Node','Event','CustomEvent','MutationObserver','getComputedStyle','requestAnimationFrame','cancelAnimationFrame','DOMParser','XMLSerializer','Image','HTMLCanvasElement','ResizeObserver'])
  if (window[k]!==undefined && globalThis[k]===undefined) try{Object.defineProperty(globalThis,k,{value:window[k],configurable:true,writable:true});}catch(e){}
try{Object.defineProperty(globalThis,'navigator',{value:window.navigator,configurable:true,writable:true});}catch(e){}
vm.runInThisContext(fs.readFileSync('mermaid.min.js','utf8'),{filename:'m'});
const mermaid = globalThis.mermaid;
mermaid.initialize({startOnLoad:false, securityLevel:'loose', theme:'neutral'});
const md = fs.readFileSync('report.md','utf8'); const lines = md.split('\n');
const blocks=[];
lines.forEach((l,i)=>{ if(l.trim().startsWith('```mermaid')){ let j=i+1,b=[]; while(j<lines.length && !lines[j].trim().startsWith('```')) b.push(lines[j++]); blocks.push({line:i+1,code:b.join('\n'),first:b.filter(x=>x.trim()&&!x.trim().startsWith('%%'))[0]?.trim()||''}); }});
console.log(`共 ${blocks.length} 个 mermaid 块\n`);
let ok=0;
for (const [i,b] of blocks.entries()){
  try{ const {svg}=await mermaid.render('c'+i,b.code);
    const cols=[...new Set((svg.match(/#[0-9a-fA-F]{6}/g)||[]))];
    console.log(`  ${i+1}. 行${String(b.line).padStart(4)} ✅ ${b.first.slice(0,34).padEnd(36)} ${String(svg.length).padStart(6)}B  色数 ${cols.length}`); ok++;
  }catch(e){ console.log(`  ${i+1}. 行${String(b.line).padStart(4)} ❌ ${b.first.slice(0,34)} → ${String(e.message||e).split('\n')[0].slice(0,60)}`); }
}
console.log(`\n结果 ${ok}/${blocks.length} 可渲染`);
