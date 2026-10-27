/* ============================================================================
 *  page_html.h — страница веб-стенда ЛР N2 (во flash-памяти ESP32).
 *  Работает без доступа в Интернет. Изменять не требуется (кроме задания 10).
 * ========================================================================== */
#ifndef PAGE_HTML_H
#define PAGE_HTML_H

const char PAGE_HTML[] PROGMEM = R"=====(
<!doctype html><html lang="ru"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ЛР2 — коды, исправляющие ошибки</title>
<style>
 body{font-family:system-ui,Arial,sans-serif;margin:0;background:#eef2f5;color:#1b2433}
 header{background:#16335c;color:#fff;padding:14px 18px;font-size:19px;font-weight:700}
 .wrap{max-width:860px;margin:0 auto;padding:16px}
 .card{background:#fff;border-radius:10px;padding:16px;margin:12px 0;box-shadow:0 1px 4px #0002}
 h2{font-size:16px;margin:0 0 10px;color:#16335c}
 button{background:#0f7c86;color:#fff;border:0;border-radius:6px;padding:8px 14px;font-size:14px;cursor:pointer;margin:3px}
 button.sec{background:#3f6185}
 select,input{padding:6px;border:1px solid #c9d5df;border-radius:6px;font-size:14px}
 .bits{font-family:"Courier New",monospace;font-size:24px;letter-spacing:2px;margin:8px 0}
 .bit{display:inline-block;width:26px;text-align:center;border-radius:4px;cursor:pointer;user-select:none}
 .bit.f{background:#e9a23b;color:#16335c}
 .b0{color:#6b7b8f}.b1{color:#16335c;font-weight:700}
 .res{font-weight:700;padding:6px 0}
 .leds{font-size:28px;letter-spacing:6px}
 table{border-collapse:collapse;width:100%;font-size:13px;margin-top:8px}
 th,td{border:1px solid #c9d5df;padding:4px 6px;text-align:center}
 th{background:#16335c;color:#fff}
 .muted{color:#6b7b8f;font-size:13px}
</style></head><body>
<header>Лабораторная работа №2 · Коды, исправляющие ошибки</header>
<div class="wrap">

<div class="card"><h2>1. Выбор кода</h2>
 <select id="code" onchange="refresh()">
  <option value="h">Хэмминг (7,4)</option>
  <option value="x">Расширенный Хэмминг (8,4), SEC-DED</option>
  <option value="r">Повторение (мажоритарный)</option>
  <option value="t">Двумерный итерационный</option>
  <option value="H">Хэмминг (15,11) — задание 10</option>
 </select>
 <span id="rp">q:<select id="q"><option>3</option><option>5</option></select></span>
 <span id="tp">решётка:<select id="ab"><option value="2,2">2×2</option><option value="2,3">2×3</option><option value="3,3">3×3</option></select></span>
 <div class="muted" id="params"></div>
</div>

<div class="card"><h2>2. Передача одной комбинации</h2>
 данные: <input id="data" value="1011" size="12">
 <button onclick="encode()">Закодировать и передать</button>
 <div>передано: <span id="sent" class="bits"></span></div>
 <div>принято (щёлкайте по разрядам, чтобы внести ошибку):</div>
 <div id="recv" class="bits"></div>
 <div class="res" id="dec"></div>
 <div class="muted" id="expl"></div>
 <div class="leds" id="leds">⚪⚪⚪</div>
</div>

<div class="card"><h2>3. Статистический эксперимент (ДСК)</h2>
 p0:<input id="p0" value="0.05" size="6"> N:<input id="N" value="100000" size="8">
 <button onclick="exp()">Текущий код</button>
 <button class="sec" onclick="expAll()">Все 4 кода</button>
 <button class="sec" onclick="dl()">Скачать CSV</button>
 <table id="tbl"><tr><th>код</th><th>p0</th><th>N</th><th>исправл.</th><th>обнар.</th><th>ош.дек.</th><th>P_дек</th><th>P_обн</th><th>мс</th></tr></table>
</div>

</div>
<script>
let rows=[];
function cfg(){let c=document.getElementById('code').value;
 let s='code='+c; if(c=='r')s+='&q='+document.getElementById('q').value;
 if(c=='t'){let ab=document.getElementById('ab').value.split(',');s+='&a='+ab[0]+'&b='+ab[1];}
 return s;}
function refresh(){let c=document.getElementById('code').value;
 document.getElementById('rp').style.display=c=='r'?'':'none';
 document.getElementById('tp').style.display=c=='t'?'':'none';
 fetch('/api/info?'+cfg()).then(r=>r.json()).then(d=>{
   document.getElementById('params').textContent=
    `n=${d.n}, k=${d.k}, R=${d.R}, L=${d.L}, d=${d.d}, исправляет t=${d.t} ошибк(у/и)`;});}
let sent='',word='';
function render(){let h='';for(let i=0;i<word.length;i++){
   let f=(word[i]!=sent[i]);h+=`<span class="bit ${f?'f':(word[i]=='1'?'b1':'b0')}" onclick="flip(${i})">${word[i]}</span>`;}
 document.getElementById('recv').innerHTML=h; check();}
function encode(){fetch('/api/encode?'+cfg()+'&data='+document.getElementById('data').value)
 .then(r=>r.json()).then(d=>{sent=d.word;word=d.word;
   document.getElementById('sent').textContent=sent;render();});}
function flip(i){word=word.substring(0,i)+(word[i]=='1'?'0':'1')+word.substring(i+1);render();}
function leds(o){const m=['🟢⚪⚪','🟢⚪⚪','⚪🟡⚪','⚪⚪🔴'];document.getElementById('leds').textContent=m[o];}
function check(){if(!word)return;
 let dataBits=sent; // здесь sent — кодовое слово; извлечём k инф. разрядов на МК
 fetch('/api/check?'+cfg()+'&sent='+document.getElementById('data').value+'&word='+word)
 .then(r=>r.json()).then(d=>{
   let names=['принято верно','ошибка исправлена','ошибка обнаружена (брак)','ОШИБКА ДЕКОДИРОВАНИЯ'];
   let col=['#2e8b62','#2e8b62','#e9a23b','#c2413a'][d.outcome];
   let e=document.getElementById('dec');
   e.textContent='декодер: '+names[d.outcome]+'  →  сообщение '+d.info;e.style.color=col;
   let ex='статус: '+d.status+(d.pos>0?(', позиция '+d.pos):'');
   document.getElementById('expl').textContent=ex;leds(d.outcome);});}
function exp(){fetch('/api/exp?'+cfg()+'&p='+document.getElementById('p0').value+'&N='+document.getElementById('N').value)
 .then(r=>r.json()).then(d=>addRow(document.getElementById('code').selectedOptions[0].text,d));}
function expAll(){let codes=['h','x','r','t'],i=0;
 (function nx(){if(i>=codes.length)return;document.getElementById('code').value=codes[i];refresh();
   setTimeout(()=>{exp();i++;setTimeout(nx,50);},50);})();}
function addRow(name,d){let p0=document.getElementById('p0').value,N=document.getElementById('N').value;
 rows.push([name,p0,N,d.corr,d.det,d.mis,d.Pdek,d.Pdet,d.ms]);
 let r=document.getElementById('tbl').insertRow(-1);
 [name,p0,N,d.corr,d.det,d.mis,d.Pdek,d.Pdet,d.ms].forEach(v=>{let c=r.insertCell(-1);c.textContent=v;});}
function dl(){let s='код;p0;N;исправл;обнар;ош_дек;P_дек;P_обн;мс\n'+rows.map(r=>r.join(';')).join('\n');
 let a=document.createElement('a');a.href='data:text/csv;charset=utf-8,'+encodeURIComponent(s);
 a.download='lab2_results.csv';a.click();}
refresh();encode();
</script></body></html>
)=====";

#endif
