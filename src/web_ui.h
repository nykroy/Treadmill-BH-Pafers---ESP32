#pragma once
#include <Arduino.h>

const char WEB_UI[] PROGMEM = R"HTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>BH | Puente de control</title><style>
:root{color-scheme:dark;font:16px system-ui;background:#0b1020;color:#e8eefb}*{box-sizing:border-box}
body{max-width:980px;margin:auto;padding:20px}h1{font-size:1.6rem}h2{font-size:1.1rem;margin-top:0}
.card{background:#151e33;border:1px solid #293754;border-radius:18px;padding:20px;margin:16px 0}
.row{display:flex;gap:10px;flex-wrap:wrap;align-items:center}.grow{flex:1;min-width:170px}
.speed-steps,.incline-steps{flex-wrap:nowrap;margin-bottom:10px}.speed-steps button,.incline-steps button{flex:1;min-width:0;padding:12px 4px}
.speed-presets,.incline-presets{display:grid;gap:8px;width:100%;margin-bottom:10px}
.speed-presets{grid-template-columns:repeat(8,minmax(0,1fr))}
.incline-presets{grid-template-columns:repeat(15,minmax(0,1fr))}
.speed-presets button,.incline-presets button{width:100%;min-width:0;padding:12px 4px}
.run-controls{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px;width:100%}
.run-controls button{width:100%;min-height:68px;font-size:1.35rem;font-weight:700}
#stop{grid-column:1/-1}
button,input,select{font:inherit;border:1px solid #46567b;border-radius:10px;padding:12px;background:#223252;color:inherit}
button{cursor:pointer;touch-action:manipulation}button:hover{background:#30476c}button:disabled{opacity:.35;cursor:not-allowed}
.primary{background:#116b65}.stop{background:#9e303c}.muted{color:#a7b6d2;font-size:.85rem}
#bleState{display:inline-block;padding:8px 14px;border-radius:24px;background:#293754}
.good{color:#63e7bc}code{overflow-wrap:anywhere;font-size:.8rem}#error{color:#ffb1b8;white-space:pre-wrap}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:12px}
.metric{background:#0d1528;padding:14px;border-radius:12px}.metric b{display:block;font-size:1.65rem}
fieldset{border:0;padding:0;margin:0}#console{background:#070c17;height:260px;overflow:auto;white-space:pre-wrap;font:12px ui-monospace,monospace;padding:12px;border-radius:12px}
.tx{color:#7ee9bc}.rx{color:#85baff}.log{color:#c6cddd}.warning{color:#ffd58b}
@media(max-width:700px){.speed-presets{grid-template-columns:repeat(4,minmax(0,1fr))}.incline-presets{grid-template-columns:repeat(5,minmax(0,1fr))}}
@media(max-width:500px){body{padding:10px}.card{padding:14px}button{flex-grow:1}.row input{width:100%}}
</style></head><body>
<h1>BH Fitness web</h1>
<section class="card"><h2 id="connectionTitle">Panel de Conexi&oacute;n Bluetooth</h2>
<div id="blePanel">
<p id="bleState" role="status">Desconectado</p>
<div class="row"><button id="scan">Escanear BLE</button><button id="connect" class="primary">Conectar</button><button id="disconnect">Desconectar</button></div>
<p><select id="devices" style="width:100%" aria-label="Dispositivo BLE"><option value="">PAFERS_C7BE99 (escanear primero)</option></select></p>
<div class="muted" id="scanInfo"></div><p class="muted">Servicio: <code id="service">--</code><br>Write: <code id="write">--</code><br>Notify: <code id="notify">--</code></p></div>
<div id="uartPanel" hidden><p id="uartState" role="status">UART no disponible</p><p id="ftmsState">FTMS: sin clientes</p>
<p class="muted">UART inicializada no confirma conexi&oacute;n f&iacute;sica. La compatibilidad con la consola debe verificarse.</p></div>
<div id="error" role="alert"></div><p id="notice" role="status" class="muted"></p></section>
<section class="card"><h2>Telemetr&iacute;a</h2><div class="grid">
<div class="metric"><span>Velocidad actual</span><b id="speed">--</b><small>km/h</small></div>
<div class="metric"><span>Inclinaci&oacute;n</span><b id="incline">--</b><small>nivel</small></div>
<div class="metric"><span>Distancia estimada</span><b id="distance">0.000</b><small id="distanceUnit">km / sesi&oacute;n BLE</small></div>
<div class="metric"><span>Tiempo estimado</span><b id="elapsed">00:00</b><small>en movimiento</small></div></div>
<p id="motion" class="muted">Estado desconocido</p></section>
<section class="card"><h2>Control de la cinta</h2>
<fieldset id="controls" disabled><div class="run-controls"><button id="start" class="primary">START</button><button id="startEco" class="primary">START (ECO)</button><button id="stop" class="stop">STOP</button></div>
<p>Velocidad objetivo enviada: <b id="target">1.0</b> km/h <span class="muted">(no es confirmaci&oacute;n)</span></p>
<div class="row speed-steps"><button data-step="-5">-0.5</button><button data-step="-1">-0.1</button><button data-step="1">+0.1</button><button data-step="5">+0.5</button></div>
<div class="speed-presets"><button data-speed="20">2.0</button><button data-speed="40">4.0</button><button data-speed="60">6.0</button><button data-speed="80">8.0</button><button data-speed="100">10.0</button><button data-speed="120">12.0</button><button data-speed="140">14.0</button><button data-speed="160">16.0</button></div>
<p>Inclinaci&oacute;n objetivo enviada: <b id="targetIncline">0</b></p>
<div class="row incline-steps"><button data-incline="-1">Inclinaci&oacute;n -</button><button data-incline="1">Inclinaci&oacute;n +</button></div>
<div class="incline-presets">
<button data-incline-value="1" aria-label="Inclinaci&oacute;n 1">1</button><button data-incline-value="2" aria-label="Inclinaci&oacute;n 2">2</button><button data-incline-value="3" aria-label="Inclinaci&oacute;n 3">3</button><button data-incline-value="4" aria-label="Inclinaci&oacute;n 4">4</button><button data-incline-value="5" aria-label="Inclinaci&oacute;n 5">5</button>
<button data-incline-value="6" aria-label="Inclinaci&oacute;n 6">6</button><button data-incline-value="7" aria-label="Inclinaci&oacute;n 7">7</button><button data-incline-value="8" aria-label="Inclinaci&oacute;n 8">8</button><button data-incline-value="9" aria-label="Inclinaci&oacute;n 9">9</button><button data-incline-value="10" aria-label="Inclinaci&oacute;n 10">10</button>
<button data-incline-value="11" aria-label="Inclinaci&oacute;n 11">11</button><button data-incline-value="12" aria-label="Inclinaci&oacute;n 12">12</button><button data-incline-value="13" aria-label="Inclinaci&oacute;n 13">13</button><button data-incline-value="14" aria-label="Inclinaci&oacute;n 14">14</button><button data-incline-value="15" aria-label="Inclinaci&oacute;n 15">15</button></div></fieldset>
<p class="muted">L&iacute;mites: 0-16 km/h, inclinaci&oacute;n 1-15. Los decimales TX son experimentales. 1 km/h recibido es ambiguo en algunas consolas BH. No se inventan checksums ni confirmaciones.</p></section>
<section class="card"><h2>Actualizaci&oacute;n OTA</h2>
<p class="warning">Sin contrase&ntilde;a: cualquiera en la red puede instalar firmware. Usa solo una red de confianza. Det&eacute;n la cinta f&iacute;sicamente antes de actualizar; se perder&aacute; el control y el ESP32 se reiniciar&aacute;.</p>
<p class="muted">En modo sat&eacute;lite desconecta primero el BLE. Selecciona el firmware.bin compilado para este ESP32 y no cortes la alimentaci&oacute;n.</p>
<form id="otaForm"><div class="row"><input id="otaFile" class="grow" type="file" accept=".bin" aria-label="Firmware binario" required><button id="otaButton">Actualizar firmware</button></div></form>
<p id="otaStatus" role="status" class="muted"></p></section>
<section class="card"><h2>Consola HEX en vivo</h2><div id="console" role="log" aria-label="Paquetes BLE"></div>
<form id="rawForm"><p class="row"><input id="hex" class="grow" placeholder="55 0A 01 02" maxlength="60" aria-label="Trama hexadecimal" autocomplete="off"><button id="raw" disabled>Enviar HEX</button></p></form>
<p class="muted">1-20 bytes. HEX se transmite literalmente, sin checksum a&ntilde;adido. Solo red local de confianza; sin autenticaci&oacute;n. <span id="usbWarning"></span></p></section>
<script>
'use strict';
const $=id=>document.getElementById(id);
let state=null, generation=-1, cursor=0, pending=false, alive=false, updating=false, startSequenceActive=false, stopRequested=false, stopPending=false, initialBleStart=false, restartAfterStop=false, connectionEpoch=0;
async function api(path,body){
 const response=await fetch(path,{method:body===undefined?'GET':'POST',headers:body===undefined?{}:{'Content-Type':'application/json'},body:body===undefined?undefined:JSON.stringify(body),signal:AbortSignal.timeout(6000)});
 const data=await response.json();if(!response.ok)throw Error(data.message||`HTTP ${response.status}`);return data;
}
function enable(){
 const connected=alive&&!updating&&!state?.otaActive&&state?.state==='connected';
 const initialAmbiguous=initialBleStart&&state?.telemetryFresh&&state.speed===1;
 const running=state?.telemetryFresh&&state.speed>0&&!initialAmbiguous;
 const stopped=initialAmbiguous||(state?.telemetryFresh&&state.speed===0);
 $('controls').disabled=!connected||pending;$('raw').disabled=!connected||pending;
 $('scan').disabled=!alive||updating||pending||state?.state!=='disconnected';
 $('connect').disabled=!alive||updating||pending||state?.state!=='disconnected'||!$('devices').value;
 $('disconnect').disabled=!alive||updating||state?.state==='disconnected'||state?.state==='scanning';
 $('otaButton').disabled=!alive||updating||pending||!!state?.otaActive;
 $('otaFile').disabled=updating;
 $('stop').disabled=!connected||stopPending||(stopped&&!startSequenceActive);
 if(connected&&pending)$('controls').disabled=false;
 const startDisabled=!connected||pending||(running&&!restartAfterStop);
 $('start').disabled=startDisabled;$('startEco').disabled=startDisabled;
 document.querySelectorAll('#controls button:not(#stop):not(#start):not(#startEco)').forEach(b=>b.disabled=!connected||pending);
}
async function action(path,body={}){
 pending=true;enable();$('notice').textContent='';
 try{const result=await api(path,body);$('notice').textContent=result.message;return true;}
 catch(error){$('notice').textContent=error.message;return false;}finally{pending=startSequenceActive;enable();}
}
function addLine(text,kind='log'){
 const line=document.createElement('div');line.className=kind;line.textContent=text;
 const box=$('console'),bottom=box.scrollTop+box.clientHeight>=box.scrollHeight-35;
 box.append(line);while(box.children.length>400)box.firstChild.remove();if(bottom)box.scrollTop=box.scrollHeight;
}
function render(s){
 if(s.state!==state?.state||s.mode!==state?.mode)connectionEpoch++;
 if(s.state!=='connected')restartAfterStop=false;
 if(s.mode==='satelite'&&s.state==='connected'&&state?.state!=='connected')initialBleStart=true;
 if(s.state!=='connected'||s.mode!=='satelite'||s.motion==='START en consola'||
    (s.telemetryFresh&&s.speed>0&&s.speed!==1))initialBleStart=false;
 state=s;alive=true;
 const integrated=s.mode==='integrado';
 $('blePanel').hidden=integrated;$('uartPanel').hidden=!integrated;
 $('connectionTitle').textContent=integrated?'Estado UART / FTMS':'Panel de Conexión Bluetooth';
 $('distanceUnit').textContent=integrated?'km / sesión UART':'km / sesión BLE';
 if(integrated){
  $('uartState').textContent=s.state!=='connected'?'UART / FTMS no disponible':s.telemetryFresh?'UART activa: datos recientes':'UART inicializada: sin datos recientes';
  $('ftmsState').textContent=`FTMS: ${s.ftmsClients?'cliente conectado':'sin clientes'}`;
 }
 const labels={disconnected:'Desconectado',scanning:'Escaneando...',connecting:'Conectando...',connected:'Conectado'};
 $('bleState').textContent=labels[s.state]+(s.state==='connected'?` - RSSI: ${s.rssi??'--'} dBm`:'');
 $('bleState').className=s.state==='connected'?'good':'';
 $('error').textContent=s.error;
 $('usbWarning').textContent=s.usbLogsDropped?`${s.usbLogsDropped} mensajes USB omitidos; consulte la consola web.`:'';
 $('service').textContent=s.serviceUuid||'--';$('write').textContent=s.writeUuid||'--';$('notify').textContent=s.notifyUuid||'--';
 if(generation!==s.scanGeneration){
  generation=s.scanGeneration;const selected=$('devices').value;$('devices').replaceChildren();
  for(const d of [...s.devices].sort((a,b)=>Number(b.target)-Number(a.target)||b.rssi-a.rssi)){
   const option=document.createElement('option');option.value=d.address;option.dataset.type=d.type;
   option.textContent=`${d.target?'★ ':''}${d.name||'(sin nombre)'} | ${d.address} | ${d.rssi} dBm`;
   if(d.target)option.className='good';$('devices').append(option);
  }
  if([...$('devices').options].some(o=>o.value===selected))$('devices').value=selected;
  if(!s.devices.length){const option=new Option('Sin dispositivos: escanear BLE','');$('devices').add(option);}
  $('scanInfo').textContent=`${s.devices.length} dispositivos${s.scanTruncated?' (limite 24 alcanzado; repita mas cerca)':''}`;
 }
 $('speed').textContent=s.telemetryFresh?s.speed.toFixed(2):'--';$('incline').textContent=s.telemetryFresh?s.incline:'--';
 $('target').textContent=(s.targetSpeedTenths/10).toFixed(1);$('targetIncline').textContent=s.targetIncline;
 $('distance').textContent=s.distanceKm.toFixed(3);
 const seconds=Math.floor(s.elapsedSeconds);$('elapsed').textContent=`${Math.floor(seconds/60).toString().padStart(2,'0')}:${(seconds%60).toString().padStart(2,'0')}`;
 $('motion').textContent=s.telemetryFresh?s.motion:'Sin telemetria reciente / estado desconocido';enable();
}
$('scan').onclick=()=>action('/api/ble/scan');
$('connect').onclick=()=>{const o=$('devices').selectedOptions[0];action('/api/ble/connect',{address:o.value,type:Number(o.dataset.type)});};
$('disconnect').onclick=()=>action('/api/ble/disconnect');
$('devices').onchange=enable;
async function startMachine(eco){
 const message=eco
  ?'La banda comenzará a 1 km/h y con inclinación 4. Mantén la zona despejada y la llave de seguridad preparada. STOP web no sustituye la parada de emergencia física.'
  :'La banda puede comenzar a moverse. Mantén la zona despejada y la llave de seguridad preparada. STOP web no sustituye la parada de emergencia física.';
 if(!confirm(message+(restartAfterStop?' Comprueba físicamente que la cinta se ha detenido antes de volver a arrancar.':'')))return;
 startSequenceActive=true;stopRequested=false;pending=true;enable();$('notice').textContent='';
 let startAttempted=false;
 try{
  if(eco){
   let result=await api('/api/control/speed',{value:10});$('notice').textContent=result.message;
   if(stopRequested)return;
  }
  if(stopRequested)return;
  startAttempted=true;
  initialBleStart=false;restartAfterStop=false;enable();
  let result=await api('/api/control/start',{});$('notice').textContent=result.message;
  if(stopRequested)return;
  if(eco){result=await api('/api/control/incline',{value:4});$('notice').textContent=result.message;}
 }catch(error){
  $('notice').textContent=error.message;
  if(eco&&startAttempted&&!stopRequested){
   try{
    const result=await api('/api/control/stop',{});
    $('notice').textContent+=`; STOP solicitado: ${result.message}`;
   }catch(stopError){$('notice').textContent+=`; Error al solicitar STOP: ${stopError.message}`;}
  }
 }
 finally{startSequenceActive=false;pending=stopPending;enable();}
}
$('start').onclick=()=>startMachine(false);
$('startEco').onclick=()=>startMachine(true);
$('stop').onclick=async()=>{
 stopRequested=true;stopPending=true;
 const epoch=connectionEpoch;
 const accepted=await action('/api/control/stop');
 stopPending=false;pending=startSequenceActive;
 if(accepted&&epoch===connectionEpoch&&state?.state==='connected'){
  restartAfterStop=true;
  $('notice').textContent+=' Comprueba que la cinta se ha detenido antes de volver a arrancar.';
 }
 enable();
};
document.querySelectorAll('[data-speed]').forEach(b=>b.onclick=()=>action('/api/control/speed',{value:Number(b.dataset.speed)}));
document.querySelectorAll('[data-step]').forEach(b=>b.onclick=()=>action('/api/control/speed',{delta:Number(b.dataset.step)}));
document.querySelectorAll('[data-incline]').forEach(b=>b.onclick=()=>action('/api/control/incline',{delta:Number(b.dataset.incline)}));
document.querySelectorAll('[data-incline-value]').forEach(b=>b.onclick=()=>action('/api/control/incline',{value:Number(b.dataset.inclineValue)}));
$('rawForm').onsubmit=e=>{e.preventDefault();if(!alive||state?.state!=='connected'||pending)return;if(confirm('Enviar trama HEX literal? Puede mover o reconfigurar la cinta.'))action('/api/ble/raw',{hex:$('hex').value});};
$('otaForm').onsubmit=e=>{
 e.preventDefault();
 const file=$('otaFile').files[0];
 if(!alive||updating||pending||!file)return;
 if(state?.mode==='satelite'&&state.state!=='disconnected'){$('otaStatus').textContent='Desconecta BLE antes de actualizar.';return;}
 if(!file.name.endsWith('.bin')||!file.size){$('otaStatus').textContent='Selecciona un archivo .bin no vacio.';return;}
 if(!confirm('Confirma que la cinta esta parada y vacia. Durante la actualizacion no habra control y el ESP32 se reiniciara. Continuar?'))return;
 updating=true;enable();$('otaStatus').textContent='Preparando actualizacion...';
 const form=new FormData();form.append('firmware',file);
 const xhr=new XMLHttpRequest();xhr.open('POST','/api/ota');xhr.responseType='json';xhr.timeout=120000;
 xhr.upload.onprogress=event=>{if(event.lengthComputable)$('otaStatus').textContent=`Enviando: ${Math.floor(event.loaded/event.total*100)} %; despues se verificara el firmware.`;};
 xhr.onload=()=>{
  if(xhr.status>=200&&xhr.status<300){$('otaStatus').textContent='Firmware aceptado. Reiniciando; espera y vuelve a abrir la pagina.';setTimeout(()=>location.reload(),8000);}
  else{updating=false;$('otaStatus').textContent=xhr.response?.message||`Error HTTP ${xhr.status}`;enable();}
 };
 xhr.onerror=xhr.ontimeout=()=>{updating=false;$('otaStatus').textContent='Conexion interrumpida: resultado incierto. Espera y comprueba el ESP32 antes de reintentar.';enable();};
 xhr.send(form);
};
async function poll(){
 if(updating){setTimeout(poll,700);return;}
 try{
  render(await api('/api/status'));
  const logs=await api(`/api/logs?since=${cursor}`);
  if(logs.latest<cursor){cursor=0;addLine('ESP32 reiniciado; recuperando registro');}
  else{if(logs.lost)addLine('Se perdieron entradas antiguas del buffer');for(const e of logs.entries)addLine(`[${e.ms}] ${e.text}`,e.text.startsWith('TX')?'tx':e.text.startsWith('RX')?'rx':'log');cursor=logs.latest;}
 }catch(error){alive=false;generation=-1;$('bleState').textContent='Sin comunicacion HTTP';$('error').textContent=error.message;enable();}
 setTimeout(poll,700);
}
enable();poll();
</script></body></html>)HTML";
