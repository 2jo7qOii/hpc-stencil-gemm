// 完整报告 → 打印优化 HTML（浏览器端渲染 Mermaid）
import fs from 'fs';
const md = fs.readFileSync('report.md','utf8');
const esc = s => s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
const inl = s => esc(s)
  .replace(/`([^`]+)`/g,'<code>$1</code>')
  .replace(/\*\*([^*]+)\*\*/g,'<strong>$1</strong>')
  .replace(/\[([^\]]+)\]\(([^)]+)\)/g,'<a href="$2">$1</a>');

const lines = md.split('\n');
const out = []; let i = 0; let para = [];
const flush = () => { if (para.length) { out.push('<p>'+inl(para.join(' '))+'</p>'); para=[]; } };

while (i < lines.length) {
  const raw = lines[i], line = raw.replace(/\s+$/,''), t = line.trim();
  if (t.startsWith('```')) {                                     // 代码 / mermaid
    flush(); const lang = t.slice(3).trim().toLowerCase();
    const body=[]; i++;
    while (i < lines.length && !lines[i].trim().startsWith('```')) body.push(lines[i++]);
    i++;
    if (lang === 'mermaid') out.push('<div class="figure"><pre class="mermaid">'+esc(body.join('\n'))+'</pre></div>');
    else out.push('<pre><code>'+esc(body.join('\n'))+'</code></pre>');
    continue;
  }
  let m;
  if ((m = t.match(/^(#{1,6})\s+(.*)$/))) { flush(); const l=m[1].length; out.push(`<h${l}>${inl(m[2])}</h${l}>`); i++; continue; }
  if (/^(-{3,}|\*{3,}|_{3,})$/.test(t)) { flush(); out.push('<hr>'); i++; continue; }
  if (t.includes('|') && i+1 < lines.length && /^\s*\|?[\s:|-]+\|?\s*$/.test(lines[i+1]) && lines[i+1].includes('-')) {
    flush();
    const cells = l => l.trim().replace(/^\|/,'').replace(/\|$/,'').split('|').map(c=>c.trim());
    const head = cells(line); i += 2; const rows=[];
    while (i < lines.length && lines[i].includes('|') && lines[i].trim()) rows.push(cells(lines[i++]));
    out.push('<table><thead><tr>'+head.map(c=>`<th>${inl(c)}</th>`).join('')+'</tr></thead><tbody>'
      + rows.map(r=>'<tr>'+r.map(c=>`<td>${inl(c)}</td>`).join('')+'</tr>').join('') + '</tbody></table>');
    continue;
  }
  if (t.startsWith('>')) { flush(); const b=[]; while (i<lines.length && lines[i].trim().startsWith('>')) b.push(lines[i++].trim().slice(1).trim()); out.push('<blockquote>'+inl(b.filter(Boolean).join(' '))+'</blockquote>'); continue; }
  if (/^\s*([-*+]|\d+\.)\s+/.test(line)) {
    flush(); const ord=/^\s*\d+\.\s+/.test(line), tag=ord?'ol':'ul', it=[];
    while (i<lines.length && /^\s*([-*+]|\d+\.)\s+/.test(lines[i])) it.push(lines[i++].replace(/^\s*([-*+]|\d+\.)\s+/,''));
    out.push(`<${tag}>`+it.map(x=>`<li>${inl(x)}</li>`).join('')+`</${tag}>`); continue;
  }
  if (!t || t.startsWith('<!--')) { flush(); i++; continue; }
  para.push(t); i++;
}
flush();

const CSS = fs.readFileSync('打印优化.css','utf8');
const html = `<!DOCTYPE html><html lang="zh-CN"><head><meta charset="utf-8">
<title>HPC 第二关实验报告</title><style>
:root{--ink:#212529;--muted:#6c757d;--line:#ced4da;--accent:#1c7ed6;--panel:#f8f9fa}
*{box-sizing:border-box}
body{margin:0;background:#eef1f4;color:var(--ink);font-family:"Noto Sans CJK SC","Source Han Sans SC","WenQuanYi Micro Hei",-apple-system,"Segoe UI",Roboto,sans-serif;line-height:1.65;font-size:15px}
.page{max-width:1000px;margin:0 auto;background:#fff;padding:48px 56px 80px;box-shadow:0 1px 3px rgba(0,0,0,.08)}
h1,h2,h3,h4{line-height:1.3;font-weight:700}
h1{font-size:26px;margin:0 0 6px;padding-bottom:12px;border-bottom:3px solid var(--accent)}
h2{font-size:20px;margin:30px 0 12px;padding-left:10px;border-left:5px solid var(--accent)}
h3{font-size:16.5px;margin:22px 0 9px} h4{font-size:15px;margin:16px 0 7px;color:var(--muted)}
p{margin:9px 0} a{color:var(--accent)} hr{border:0;border-top:1px solid var(--line);margin:24px 0}
blockquote{margin:11px 0;padding:9px 14px;background:#fff9db;border-left:4px solid #f59f00;border-radius:0 5px 5px 0;color:#5c3d00}
blockquote p{margin:4px 0}
code{background:#f1f3f5;padding:1px 5px;border-radius:3px;font-family:"DejaVu Sans Mono",Consolas,monospace;font-size:13px;color:#c92a2a}
pre{background:#21252b;color:#e6e6e6;padding:12px 15px;border-radius:6px;overflow-x:auto;margin:11px 0}
pre code{background:none;color:inherit;padding:0;font-size:12.5px;line-height:1.5}
table{border-collapse:collapse;width:100%;margin:11px 0;font-size:13px}
th,td{border:1px solid var(--line);padding:4px 8px;text-align:left;vertical-align:top;line-height:1.4}
th{background:var(--panel);font-weight:700} tr:nth-child(even) td{background:#fcfcfd}
ul,ol{margin:8px 0;padding-left:24px} li{margin:3px 0}
.figure{margin:14px 0;padding:10px;background:var(--panel);border:1px solid var(--line);border-radius:8px;text-align:center;overflow-x:auto}
.tip{background:#e7f5ff;border:1px solid #a5d8ff;border-radius:6px;padding:10px 14px;margin-bottom:18px;font-size:14px}
${CSS}
</style></head><body><div class="page">
<div class="tip no-print"><b>打印优化完整版。</b>按 <b>Ctrl+P</b> → 目标选「另存为 PDF」→ 勾选「打印背景色和图像」。
下方提示不会被打印。共 ${out.length} 个内容块。</div>
${out.join('\n')}
</div>
<script src="mermaid.min.js"></script>
<script>mermaid.initialize({startOnLoad:true,theme:'neutral',securityLevel:'loose',
  fontFamily:'"Noto Sans CJK SC","WenQuanYi Micro Hei",sans-serif'});</script>
</body></html>`;
fs.writeFileSync('full.html', html);
const nf = (html.match(/class="mermaid"/g)||[]).length;
const nt = (html.match(/<table>/g)||[]).length;
console.log(`已生成 full.html：${(html.length/1024).toFixed(1)} KB，图 ${nf} 张，表 ${nt} 个`);
