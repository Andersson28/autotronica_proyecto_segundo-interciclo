#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESP32Servo.h>
#include <esp_system.h>

// =====================================================
// ACCESS POINT
// =====================================================
const char* AP_SSID = "TRACTOR_ESP32";
const char* AP_PASSWORD = "12345678";

IPAddress AP_IP(192, 168, 4, 1);
IPAddress AP_GATEWAY(192, 168, 4, 1);
IPAddress AP_SUBNET(255, 255, 255, 0);

WebServer server(80);
DNSServer dnsServer;

// =====================================================
// PINES ESP32-C3 SUPER MINI
// =====================================================
const uint8_t PINES_SERVO[5] = {0, 1, 3, 4, 5};

const uint8_t L298_IN1 = 6;
const uint8_t L298_IN2 = 7;
const uint8_t L298_IN3 = 10;
const uint8_t L298_IN4 = 20;

// =====================================================
// CALIBRACION DE SERVOS
// =====================================================
const int LIMITE_MINIMO_ANGULO[5] = {
  40,   // S1: inclinacion pala delantera
  95,   // S2: conjunto delantero
  125,  // S3: brazo trasero
  0,    // S4: pluma trasera
  0     // S5: cucharon
};

const int LIMITE_MAXIMO_ANGULO[5] = {
  180,  // S1
  180,  // S2
  180,  // S3
  100,  // S4
  150   // S5
};

const int ANGULO_SUBIDO[5] = {
  40,   // S1
  180,  // S2
  180,  // S3
  0,    // S4
  150   // S5
};

const int ANGULO_BAJADO[5] = {
  180,  // S1
  95,   // S2
  125,  // S3
  100,  // S4
  0     // S5
};

// Grados añadidos al objetivo en cada paso mientras se mantiene pulsado.
const uint8_t PASO_BOTON_GRADOS[5] = {
  5, 5, 5, 5, 5
};

// Cada cuantos milisegundos se aplica un nuevo paso de boton mantenido.
const uint16_t PERIODO_PASO_BOTON_MS[5] = {
  170, 170, 180, 170, 170
};

// Movimiento fisico: mayor valor = mas lento.
const uint16_t INTERVALO_SUBIDA_MS[5] = {
  20, 20, 22, 20, 20
};

const uint16_t INTERVALO_BAJADA_MS[5] = {
  32, 32, 35, 32, 32
};

const uint8_t MICROPASO_SUAVE[5] = {
  1, 1, 1, 1, 1
};

int anguloActual[5] = {
  40, 180, 180, 0, 150
};

int anguloObjetivo[5] = {
  40, 180, 180, 0, 150
};

Servo servos[5];
unsigned long ultimoMovimientoServo[5] = {0, 0, 0, 0, 0};

// -1 = bajar, 0 = detenido, +1 = subir.
int8_t direccionServoActiva[5] = {0, 0, 0, 0, 0};
unsigned long ultimoPasoBotonServo[5] = {0, 0, 0, 0, 0};
unsigned long ultimoHeartbeatServo[5] = {0, 0, 0, 0, 0};
const unsigned long TIMEOUT_SERVO_PULSADO_MS = 1600;

// =====================================================
// SEGURIDAD DE TRACCION
// =====================================================
const unsigned long TIMEOUT_MOTOR_MS = 1500;
unsigned long ultimoComandoMotor = 0;
char estadoMotor = 'X';

// =====================================================
// PAGINA WEB
// =====================================================
const char PAGINA_WEB[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>Tractor</title>
  <style>
    :root{--y:#ffd000;--y2:#ffb800;--b:#070707;--p:#141414;--g:#343434;--w:#f5f5f5}
    *{box-sizing:border-box;-webkit-tap-highlight-color:transparent;user-select:none;-webkit-user-select:none}
    html,body{margin:0;min-height:100%;background:var(--b);color:var(--w);font-family:Arial,sans-serif;overscroll-behavior:none}
    body{padding:12px}
    header{display:flex;align-items:center;justify-content:space-between;gap:10px;max-width:1180px;margin:0 auto 12px;padding:11px 14px;border:2px solid var(--y);border-radius:15px;background:#0b0b0b}
    h1{margin:0;color:var(--y);font-size:clamp(20px,4vw,32px)}
    #conexion{min-width:105px;padding:8px 11px;border-radius:999px;text-align:center;font-size:12px;font-weight:900;color:#000;background:var(--y)}
    .contenedor{display:grid;grid-template-columns:minmax(300px,.8fr) minmax(410px,1.35fr);gap:12px;max-width:1180px;margin:auto}
    .panel{border:2px solid var(--y);border-radius:17px;background:var(--p);padding:12px}
    .titulo{margin:0 0 10px;color:var(--y);text-align:center;font-size:18px;text-transform:uppercase}
    button{border:0;outline:0;touch-action:none;cursor:pointer;font-family:inherit}
    .cruz{display:grid;grid-template-columns:repeat(3,minmax(74px,105px));grid-template-rows:repeat(3,minmax(74px,105px));justify-content:center;gap:8px;padding:6px 0}
    .mando,.flecha{border:3px solid #000;border-radius:17px;background:linear-gradient(145deg,var(--y),var(--y2));color:#000;font-weight:900;box-shadow:0 5px 0 #7a6500}
    .mando{font-size:37px}.flecha{min-height:58px;font-size:30px}
    .mando.activo,.flecha.activo,.mando:active,.flecha:active{transform:translateY(4px) scale(.98);box-shadow:0 1px 0 #7a6500}
    .f{grid-column:2;grid-row:1}.l{grid-column:1;grid-row:2}.x{grid-column:2;grid-row:2;font-size:19px}.r{grid-column:3;grid-row:2}.b{grid-column:2;grid-row:3}
    .estado{text-align:center;margin-top:8px;color:var(--y);font-weight:900}
    .rejilla{display:grid;grid-template-columns:repeat(2,minmax(175px,1fr));gap:10px}
    .tarjeta{min-height:145px;border:1px solid var(--g);border-radius:14px;background:#090909;padding:10px}
    .tarjeta h3{min-height:40px;margin:0 0 7px;color:var(--y);font-size:14px;text-align:center;display:flex;align-items:center;justify-content:center}
    .angulo{text-align:center;margin-bottom:8px;font-weight:900}
    .botones{display:grid;grid-template-columns:1fr 1fr;gap:8px}
    .info{display:flex;align-items:center;justify-content:center;text-align:center;color:#ddd;line-height:1.45}
    @media(max-width:820px){.contenedor{grid-template-columns:1fr}}
    @media(max-width:470px){body{padding:7px}.panel{padding:9px}.rejilla{grid-template-columns:repeat(2,minmax(145px,1fr))}.cruz{grid-template-columns:repeat(3,minmax(67px,91px));grid-template-rows:repeat(3,minmax(67px,91px))}}
  </style>
</head>
<body>
<header><h1>TRACTOR CONTROL</h1><div id="conexion">CONECTANDO</div></header>
<main class="contenedor">
  <section class="panel">
    <h2 class="titulo">Traccion</h2>
    <div class="cruz">
      <button class="mando f" data-motor="F">▲</button>
      <button class="mando l" data-motor="L">◀</button>
      <button class="mando x" id="stopMotor">STOP</button>
      <button class="mando r" data-motor="R">▶</button>
      <button class="mando b" data-motor="B">▼</button>
    </div>
    <div class="estado">Motor: <span id="estadoMotor">DETENIDO</span></div>
  </section>

  <section class="panel">
    <h2 class="titulo">Brazos y palas</h2>
    <div class="rejilla">
      <article class="tarjeta"><h3>S1 · Inclinacion pala delantera</h3><div class="angulo" id="angulo1">40°</div><div class="botones"><button class="flecha" data-servo="1" data-dir="up">▲</button><button class="flecha" data-servo="1" data-dir="down">▼</button></div></article>
      <article class="tarjeta"><h3>S2 · Conjunto delantero</h3><div class="angulo" id="angulo2">180°</div><div class="botones"><button class="flecha" data-servo="2" data-dir="up">▲</button><button class="flecha" data-servo="2" data-dir="down">▼</button></div></article>
      <article class="tarjeta"><h3>S3 · Brazo trasero</h3><div class="angulo" id="angulo3">180°</div><div class="botones"><button class="flecha" data-servo="3" data-dir="up">▲</button><button class="flecha" data-servo="3" data-dir="down">▼</button></div></article>
      <article class="tarjeta"><h3>S4 · Pluma trasera</h3><div class="angulo" id="angulo4">0°</div><div class="botones"><button class="flecha" data-servo="4" data-dir="up">▲</button><button class="flecha" data-servo="4" data-dir="down">▼</button></div></article>
      <article class="tarjeta"><h3>S5 · Cucharon</h3><div class="angulo" id="angulo5">150°</div><div class="botones"><button class="flecha" data-servo="5" data-dir="up">▲</button><button class="flecha" data-servo="5" data-dir="down">▼</button></div></article>
      <article class="tarjeta info"><strong>MULTITOUCH<br></strong>&nbsp;Conduce y mueve un brazo al mismo tiempo.</article>
    </div>
  </section>
</main>

<script>
const conexion=document.getElementById('conexion');
const estadoMotorTexto=document.getElementById('estadoMotor');
const motores=new Map();
const servos=new Map();
let peticionEstadoActiva=false;

function nombreMotor(c){return c==='F'?'ADELANTE':c==='B'?'ATRÁS':c==='L'?'IZQUIERDA':c==='R'?'DERECHA':'DETENIDO'}

async function enviar(url){
  const r=await fetch(url,{cache:'no-store'});
  if(!r.ok)throw new Error('HTTP');
  conexion.textContent='CONECTADO';
  return r;
}

function ultimoMotor(){let c='X';for(const d of motores.values())c=d.comando;return c}
function mandarMotor(c){estadoMotorTexto.textContent=nombreMotor(c);enviar('/motor?cmd='+c).catch(()=>conexion.textContent='SIN ENLACE')}

function soltarMotor(e){
  const d=motores.get(e.pointerId);if(!d)return;
  clearInterval(d.heartbeat);d.boton.classList.remove('activo');motores.delete(e.pointerId);
  mandarMotor(ultimoMotor());
}

document.querySelectorAll('[data-motor]').forEach(b=>{
  b.addEventListener('pointerdown',e=>{
    e.preventDefault();b.setPointerCapture(e.pointerId);b.classList.add('activo');
    const comando=b.dataset.motor;
    mandarMotor(comando);
    const heartbeat=setInterval(()=>mandarMotor(comando),650);
    motores.set(e.pointerId,{comando,heartbeat,boton:b});
  });
  b.addEventListener('pointerup',soltarMotor);
  b.addEventListener('pointercancel',soltarMotor);
  b.addEventListener('lostpointercapture',soltarMotor);
});

document.getElementById('stopMotor').addEventListener('pointerdown',e=>{
  e.preventDefault();
  for(const d of motores.values()){clearInterval(d.heartbeat);d.boton.classList.remove('activo')}
  motores.clear();mandarMotor('X');
});

function iniciarServo(b,e){
  e.preventDefault();b.setPointerCapture(e.pointerId);b.classList.add('activo');
  const id=b.dataset.servo,dir=b.dataset.dir;
  enviar('/servo/start?id='+id+'&dir='+dir).catch(()=>conexion.textContent='SIN ENLACE');
  const heartbeat=setInterval(()=>enviar('/servo/keep?id='+id).catch(()=>{}),700);
  servos.set(e.pointerId,{id,heartbeat,boton:b});
}

function soltarServo(e){
  const d=servos.get(e.pointerId);if(!d)return;
  clearInterval(d.heartbeat);d.boton.classList.remove('activo');servos.delete(e.pointerId);
  enviar('/servo/stop?id='+d.id).catch(()=>conexion.textContent='SIN ENLACE');
}

document.querySelectorAll('[data-servo]').forEach(b=>{
  b.addEventListener('pointerdown',e=>iniciarServo(b,e));
  b.addEventListener('pointerup',soltarServo);
  b.addEventListener('pointercancel',soltarServo);
  b.addEventListener('lostpointercapture',soltarServo);
});

async function actualizarEstado(){
  if(peticionEstadoActiva)return;
  peticionEstadoActiva=true;
  try{
    const r=await enviar('/status');
    const d=await r.json();
    for(let i=0;i<5;i++){
      document.getElementById('angulo'+(i+1)).textContent=d.actual[i]===d.objetivo[i]?d.actual[i]+'°':d.actual[i]+'° → '+d.objetivo[i]+'°';
    }
    estadoMotorTexto.textContent=nombreMotor(d.motor);
  }catch(e){conexion.textContent='SIN ENLACE'}
  peticionEstadoActiva=false;
  setTimeout(actualizarEstado,900);
}

function pararTodo(){
  mandarMotor('X');
  for(const d of servos.values())enviar('/servo/stop?id='+d.id).catch(()=>{});
}

document.addEventListener('visibilitychange',()=>{if(document.hidden)pararTodo()});
window.addEventListener('pagehide',pararTodo);
document.addEventListener('contextmenu',e=>e.preventDefault());
actualizarEstado();
</script>
</body>
</html>
)rawliteral";

// =====================================================
// MOTORES
// =====================================================
void detenerMotores() {
  digitalWrite(L298_IN1, LOW);
  digitalWrite(L298_IN2, LOW);
  digitalWrite(L298_IN3, LOW);
  digitalWrite(L298_IN4, LOW);
  estadoMotor = 'X';
}

void aplicarComandoMotor(char comando) {
  switch (comando) {
    case 'F':
      digitalWrite(L298_IN1, HIGH);
      digitalWrite(L298_IN2, LOW);
      digitalWrite(L298_IN3, HIGH);
      digitalWrite(L298_IN4, LOW);
      estadoMotor = 'F';
      break;
    case 'B':
      digitalWrite(L298_IN1, LOW);
      digitalWrite(L298_IN2, HIGH);
      digitalWrite(L298_IN3, LOW);
      digitalWrite(L298_IN4, HIGH);
      estadoMotor = 'B';
      break;
    case 'L':
      digitalWrite(L298_IN1, HIGH);
      digitalWrite(L298_IN2, LOW);
      digitalWrite(L298_IN3, LOW);
      digitalWrite(L298_IN4, HIGH);
      estadoMotor = 'L';
      break;
    case 'R':
      digitalWrite(L298_IN1, LOW);
      digitalWrite(L298_IN2, HIGH);
      digitalWrite(L298_IN3, HIGH);
      digitalWrite(L298_IN4, LOW);
      estadoMotor = 'R';
      break;
    default:
      detenerMotores();
      return;
  }
  ultimoComandoMotor = millis();
}

// =====================================================
// OBJETIVO DE SERVOS MIENTRAS SE MANTIENE PULSADO
// =====================================================
void actualizarBotonesServo() {
  unsigned long ahora = millis();

  for (int i = 0; i < 5; i++) {
    if (direccionServoActiva[i] == 0) continue;

    if (ahora - ultimoHeartbeatServo[i] > TIMEOUT_SERVO_PULSADO_MS) {
      direccionServoActiva[i] = 0;
      continue;
    }

    if (ahora - ultimoPasoBotonServo[i] < PERIODO_PASO_BOTON_MS[i]) continue;
    ultimoPasoBotonServo[i] = ahora;

    int direccionFisicaSubida = (ANGULO_SUBIDO[i] > ANGULO_BAJADO[i]) ? 1 : -1;
    int direccionAngular = direccionServoActiva[i] > 0
      ? direccionFisicaSubida
      : -direccionFisicaSubida;

    int nuevoObjetivo = anguloObjetivo[i] + direccionAngular * PASO_BOTON_GRADOS[i];
    anguloObjetivo[i] = constrain(
      nuevoObjetivo,
      LIMITE_MINIMO_ANGULO[i],
      LIMITE_MAXIMO_ANGULO[i]
    );
  }
}

// =====================================================
// MOVIMIENTO SUAVE NO BLOQUEANTE
// =====================================================
void actualizarServos() {
  unsigned long ahora = millis();

  for (int i = 0; i < 5; i++) {
    int diferencia = anguloObjetivo[i] - anguloActual[i];
    if (diferencia == 0) continue;

    int direccionAngular = diferencia > 0 ? 1 : -1;
    int direccionFisicaSubida = (ANGULO_SUBIDO[i] > ANGULO_BAJADO[i]) ? 1 : -1;
    bool bajando = direccionAngular != direccionFisicaSubida;

    uint16_t intervalo = bajando ? INTERVALO_BAJADA_MS[i] : INTERVALO_SUBIDA_MS[i];
    int distancia = abs(diferencia);

    // Frenado suave cerca del objetivo.
    if (distancia <= 3) intervalo += 30;
    else if (distancia <= 7) intervalo += 12;

    if (ahora - ultimoMovimientoServo[i] < intervalo) continue;
    ultimoMovimientoServo[i] = ahora;

    int nuevoAngulo = anguloActual[i] + direccionAngular * MICROPASO_SUAVE[i];
    if ((direccionAngular > 0 && nuevoAngulo > anguloObjetivo[i]) ||
        (direccionAngular < 0 && nuevoAngulo < anguloObjetivo[i])) {
      nuevoAngulo = anguloObjetivo[i];
    }

    anguloActual[i] = nuevoAngulo;
    servos[i].write(anguloActual[i]);
  }
}

// =====================================================
// RESPUESTAS WEB
// =====================================================
void enviarInicio() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send_P(200, "text/html", PAGINA_WEB);
}

void redirigirInicio() {
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

void manejarMotor() {
  if (!server.hasArg("cmd") || server.arg("cmd").length() == 0) {
    server.send(400, "text/plain", "Falta cmd");
    return;
  }

  char comando = server.arg("cmd").charAt(0);
  if (comando >= 'a' && comando <= 'z') comando -= 32;

  if (comando == 'F' || comando == 'B' || comando == 'L' || comando == 'R') {
    aplicarComandoMotor(comando);
  } else {
    detenerMotores();
  }

  server.send(200, "text/plain", "OK");
}

bool obtenerIndiceServo(int& indice) {
  if (!server.hasArg("id")) return false;
  int numero = server.arg("id").toInt();
  if (numero < 1 || numero > 5) return false;
  indice = numero - 1;
  return true;
}

void manejarServoStart() {
  int indice;
  if (!obtenerIndiceServo(indice) || !server.hasArg("dir")) {
    server.send(400, "text/plain", "Parametros incorrectos");
    return;
  }

  String direccion = server.arg("dir");
  if (direccion == "up") direccionServoActiva[indice] = 1;
  else if (direccion == "down") direccionServoActiva[indice] = -1;
  else {
    server.send(400, "text/plain", "Direccion incorrecta");
    return;
  }

  unsigned long ahora = millis();
  ultimoHeartbeatServo[indice] = ahora;
  ultimoPasoBotonServo[indice] = ahora - PERIODO_PASO_BOTON_MS[indice];
  server.send(200, "text/plain", "OK");
}

void manejarServoKeep() {
  int indice;
  if (!obtenerIndiceServo(indice)) {
    server.send(400, "text/plain", "Servo incorrecto");
    return;
  }
  ultimoHeartbeatServo[indice] = millis();
  server.send(200, "text/plain", "OK");
}

void manejarServoStop() {
  int indice;
  if (!obtenerIndiceServo(indice)) {
    server.send(400, "text/plain", "Servo incorrecto");
    return;
  }
  direccionServoActiva[indice] = 0;
  server.send(200, "text/plain", "OK");
}

void manejarEstado() {
  char json[220];
  snprintf(
    json,
    sizeof(json),
    "{\"actual\":[%d,%d,%d,%d,%d],\"objetivo\":[%d,%d,%d,%d,%d],\"motor\":\"%c\"}",
    anguloActual[0], anguloActual[1], anguloActual[2], anguloActual[3], anguloActual[4],
    anguloObjetivo[0], anguloObjetivo[1], anguloObjetivo[2], anguloObjetivo[3], anguloObjetivo[4],
    estadoMotor
  );
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", json);
}

void configurarServidor() {
  server.on("/", HTTP_GET, enviarInicio);
  server.on("/motor", HTTP_GET, manejarMotor);
  server.on("/servo/start", HTTP_GET, manejarServoStart);
  server.on("/servo/keep", HTTP_GET, manejarServoKeep);
  server.on("/servo/stop", HTTP_GET, manejarServoStop);
  server.on("/status", HTTP_GET, manejarEstado);

  // Rutas usadas por Android, iPhone y Windows para comprobar Internet.
  server.on("/generate_204", HTTP_ANY, redirigirInicio);
  server.on("/gen_204", HTTP_ANY, redirigirInicio);
  server.on("/hotspot-detect.html", HTTP_ANY, redirigirInicio);
  server.on("/ncsi.txt", HTTP_ANY, redirigirInicio);
  server.on("/connecttest.txt", HTTP_ANY, redirigirInicio);
  server.onNotFound(redirigirInicio);

  server.begin();
}

const char* textoReset(esp_reset_reason_t motivo) {
  switch (motivo) {
    case ESP_RST_POWERON: return "ENCENDIDO";
    case ESP_RST_BROWNOUT: return "CAIDA DE VOLTAJE / BROWNOUT";
    case ESP_RST_TASK_WDT: return "WATCHDOG DE TAREA";
    case ESP_RST_INT_WDT: return "WATCHDOG DE INTERRUPCION";
    case ESP_RST_SW: return "REINICIO POR SOFTWARE";
    case ESP_RST_PANIC: return "ERROR / PANIC";
    default: return "OTRO";
  }
}

void iniciarAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAPConfig(AP_IP, AP_GATEWAY, AP_SUBNET);

  bool iniciado = WiFi.softAP(AP_SSID, AP_PASSWORD, 6, 0, 2);
  dnsServer.start(53, "*", AP_IP);
  configurarServidor();

  Serial.println();
  Serial.println("====================================");
  Serial.println("ACCESS POINT INICIADO");
  Serial.print("Red: ");
  Serial.println(AP_SSID);
  Serial.print("Clave: ");
  Serial.println(AP_PASSWORD);
  Serial.print("Direccion: http://");
  Serial.println(WiFi.softAPIP());
  Serial.print("Estado: ");
  Serial.println(iniciado ? "OK" : "ERROR");
  Serial.println("====================================");
}

void iniciarServos() {
  // El AP ya esta visible antes de energizar los cinco servos.
  for (int i = 0; i < 5; i++) {
    servos[i].setPeriodHertz(50);
    servos[i].attach(PINES_SERVO[i], 500, 2500);
    servos[i].write(anguloActual[i]);
    delay(100);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  esp_reset_reason_t motivo = esp_reset_reason();
  Serial.print("Motivo del ultimo reinicio: ");
  Serial.println(textoReset(motivo));

  pinMode(L298_IN1, OUTPUT);
  pinMode(L298_IN2, OUTPUT);
  pinMode(L298_IN3, OUTPUT);
  pinMode(L298_IN4, OUTPUT);
  detenerMotores();

  iniciarAccessPoint();
  iniciarServos();

  Serial.println("TRACTOR LISTO");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  actualizarBotonesServo();
  actualizarServos();

  if (estadoMotor != 'X' && millis() - ultimoComandoMotor > TIMEOUT_MOTOR_MS) {
    detenerMotores();
  }

  delay(1);
}
