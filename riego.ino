#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <RTClib.h>

const char* ssid = "WIFI UTC EdificioH";
const char* password = "utcalvillo";

String idValvula1 = "67bb6f2e85118d10af317f79";  // ID para la válvula 1
String idValvula2 = "67bb79ac1c82e9d42d445882";  // ID para la válvula 2

String urlEstado1 = "https://apiriego.onrender.com/estado/" + idValvula1;
String urlConfig1 = "https://apiriego.onrender.com/config1/" + idValvula1;
String urlActualizarEstado1 = "https://apiriego.onrender.com/actualizarEstadoFalse/" + idValvula1;

String urlEstado2 = "https://apiriego.onrender.com/estado/" + idValvula2;
String urlConfig2 = "https://apiriego.onrender.com/config1/" + idValvula2;
String urlActualizarEstado2 = "https://apiriego.onrender.com/actualizarEstadoFalse/" + idValvula2;
// Nueva URL para restar la pausa
String urlRestarPausa1 = "https://apiriego.onrender.com/restarPausa/" + idValvula1;
String urlRestarPausa2 = "https://apiriego.onrender.com/restarPausa/" + idValvula2;
String urlDuracionPausa1 = "https://apiriego.onrender.com/duracionPausa/" + idValvula1;
String urlDuracionPausa2 = "https://apiriego.onrender.com/duracionPausa/" + idValvula2;



const int pinValvula1 = 23;
const int pinValvula2 = 22;

RTC_DS3231 rtc;

bool estadoRiego1, estadoRiego2;
String diasRiego1, diasRiego2;
int duracion1, duracion2;
int duracionPausa1, duracionPausa2;
String fechaInicio1, fechaFin1, horaInicio1;
String fechaInicio2, fechaFin2, horaInicio2;
int pausas1, pausas2;
bool isRiego1Activo = false;
bool isRiego2Activo = false;

unsigned long tiempoAnterior1 = 0; // Variable para almacenar el último tiempo válvula 1
unsigned long tiempoPausa1 = 0; // Variable para controlar el tiempo de pausa válvula 1
int actoRiego1 = 0; // Variable para saber en qué parte del riego estamos válvula 1

unsigned long tiempoAnterior2 = 0; // Variable para almacenar el último tiempo válvula 2
unsigned long tiempoPausa2 = 0; // Variable para controlar el tiempo de pausa válvula 2
int actoRiego2 = 0; // Variable para saber en qué parte del riego estamos válvula 2

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Wire.begin();

  pinMode(pinValvula1, OUTPUT);
  pinMode(pinValvula2, OUTPUT);
  digitalWrite(pinValvula1, LOW);
  digitalWrite(pinValvula2, LOW);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a WiFi.");

  if (!rtc.begin()) {
    Serial.println("Error: No se encontró el módulo RTC");
    while (1);
  }
}

void loop() {
  // Verificar conexión WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. Reconectando...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Conectando a WiFi...");
    }
    Serial.println("Conectado a WiFi.");
  }

  // Obtener estado de la válvula 1
  HTTPClient http;
  http.begin(urlEstado1);
  int httpCode1 = http.GET();

  if (httpCode1 == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Estado de Riego Válvula 1: " + payload);

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      estadoRiego1 = doc["Respuesta"]["estado"].as<bool>();
      Serial.print("Estado Válvula 1: ");
      Serial.println(estadoRiego1 ? "true" : "false");
    }
  }
  http.end();

  // Obtener estado de la válvula 2
  http.begin(urlEstado2);
  int httpCode2 = http.GET();

  if (httpCode2 == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.println("Estado de Riego Válvula 2: " + payload);

    StaticJsonDocument<200> doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      estadoRiego2 = doc["Respuesta"]["estado"].as<bool>();
      Serial.print("Estado Válvula 2: ");
      Serial.println(estadoRiego2 ? "true" : "false");
    }
  }
  http.end();

  // Si el riego está activado, obtener la configuración de ambas válvulas
  if (estadoRiego1) {
    obtenerConfiguracion(1);
    // Ejecutar riego si ya está activado, aunque no haya validación
    if (validarRiego(1) || isRiego1Activo) {
      ejecutarRiego(1);
    } else {
      Serial.println("Hoy no hay riego programado para la Válvula 1.");
    }
  }

  if (estadoRiego2) {
    obtenerConfiguracion(2);
    // Ejecutar riego si ya está activado, aunque no haya validación
    if (validarRiego(2) || isRiego2Activo) {
      ejecutarRiego(2);
    } else {
      Serial.println("Hoy no hay riego programado para la Válvula 2.");
    }
  }

  delay(5000);
}

void obtenerConfiguracion(int valvula) {
  HTTPClient http;
  String urlConfig;
  if (valvula == 1) {
    urlConfig = urlConfig1;
  }
  if (valvula == 2) {
    urlConfig = urlConfig2;
  }

  http.begin(urlConfig);
  int configCode = http.GET();

  if (configCode == HTTP_CODE_OK) {
    String configPayload = http.getString();
    Serial.println("Configuración Válvula " + String(valvula) + ": " + configPayload);

    StaticJsonDocument<500> configDoc;
    DeserializationError error = deserializeJson(configDoc, configPayload);

    if (!error) {
      JsonObject config = configDoc["Respuesta"][0];
      if (valvula == 1) {
        diasRiego1 = config["dias"].as<String>();
        duracion1 = config["duracion"];
        duracionPausa1 = config["duracionPausa"];
        fechaInicio1 = config["fechaInicio"].as<String>();
        fechaFin1 = config["fechaFin"].as<String>();
        horaInicio1 = config["horaInicio"].as<String>();
        pausas1 = config["pausas"];
      }
      if (valvula == 2) {
        diasRiego2 = config["dias"].as<String>();
        duracion2 = config["duracion"];
        duracionPausa2 = config["duracionPausa"];
        fechaInicio2 = config["fechaInicio"].as<String>();
        fechaFin2 = config["fechaFin"].as<String>();
        horaInicio2 = config["horaInicio"].as<String>();
        pausas2 = config["pausas"];
      }
    }
  }
  http.end();
}

bool validarRiego(int valvula) {
  // Si el riego ya se ha activado para esta válvula, no lo validamos nuevamente
  if ((valvula == 1 && isRiego1Activo) || (valvula == 2 && isRiego2Activo)) {
    Serial.println("Riego ya ha sido activado. No se valida nuevamente.");
    return false;
  }

  DateTime ahora = rtc.now();
  String diaActual = obtenerDiaSemana(ahora.dayOfTheWeek());
  String fechaActual = formatoFecha(ahora.year(), ahora.month(), ahora.day());
  String horaActual = formatoHora(ahora.hour(), ahora.minute());

  String horaInicio24 = convertirHora12a24(horaInicio1);
  String horaInicio224 = convertirHora12a24(horaInicio2);

  Serial.println("Fecha actual: " + fechaActual);
  Serial.println("Día actual: " + diaActual);
  Serial.println("Hora actual: " + horaActual);
  Serial.println("Hora inicio 1: " + horaInicio24);
  Serial.println("Hora inicio 2: " + horaInicio224);

  if (valvula == 1) {
    if (fechaActual >= fechaInicio1 && fechaActual <= fechaFin1) {
      if (diasRiego1.indexOf(diaActual) != -1) {
        if (horaActual == horaInicio24) {
          isRiego1Activo = true; // Activamos la bandera para evitar validaciones posteriores
          return true;
        }
      }
    }
  } 
  if (valvula == 2) {
    if (fechaActual >= fechaInicio2 && fechaActual <= fechaFin2) {
      if (diasRiego2.indexOf(diaActual) != -1) {
        if (horaActual == horaInicio224) {
          isRiego2Activo = true; // Activamos la bandera para evitar validaciones posteriores
          return true;
        }
      }
    }
  }

  return false;
}


void ejecutarRiego(int valvula) {
  unsigned long tiempoActual = millis(); // Obtener el tiempo actual

  // Verificar si la bandera no ha sido activada y ya pasó por actoRiego == 2
  if (valvula == 1) {


    if (actoRiego1 == 0) {
      Serial.println("Activando riego Válvula 1...");
      Serial.println("Válvula 1 abierta");
      digitalWrite(pinValvula1, HIGH); // Abre la válvula
      tiempoAnterior1 = tiempoActual; // Guardamos el tiempo en que se abrió la válvula
      actoRiego1 = 1; // Cambiamos el estado a 1 para esperar la duración
    }

    if (actoRiego1 == 1 && tiempoActual - tiempoAnterior1 >= duracion1 * 1000) {
      // Si ha pasado el tiempo de duración
      Serial.println("Válvula 1 cerrada");
      digitalWrite(pinValvula1, LOW); // Cierra la válvula
      actoRiego1 = 2; // Cambiamos el estado para manejar la pausa
      tiempoPausa1 = tiempoActual; // Guardamos el tiempo de la pausa
    }
    Serial.println("Pausas fuera: ");
    Serial.println(pausas1);
    if (pausas1 == 0 && duracionPausa1 > 0) {
          actualizarDuracionPausa(1);
          Serial.println("Duracion actualizada a 0");
        }

    if (actoRiego1 == 2 && tiempoActual - tiempoPausa1 >= duracionPausa1 * 1000) {
      Serial.println("Pausas if: ");
      Serial.println(pausas1);
      if (pausas1 > 0) {
        // Llamamos a la función para restar la pausa desde la base de datos
        restarPausa(1);
        Serial.println("Pausas: ");
        Serial.println(pausas1);
        if (pausas1 == 0) {
          actualizarDuracionPausa(1);
          Serial.println("Duracion actualizada a 0");
        }
        
        // Continuamos con el ciclo de riego
        Serial.println("Abriendo válvula para siguiente ciclo...");
        digitalWrite(pinValvula1, HIGH); // Abre la válvula nuevamente
        tiempoAnterior1 = tiempoActual; // Guardamos el tiempo en que se abrió la válvula
        Serial.println("Pausas: ");
        Serial.println(pausas1);
        actoRiego1 = 1; // Esperamos la duración para el siguiente ciclo
      } else {
        Serial.println("Riego completado para la válvula 1.");
        actoRiego1 = 3; // Fin del riego
        isRiego1Activo = false; // Restablecemos la bandera de riego
        // Reiniciar variables de control
        actoRiego1 = 0;
        tiempoAnterior1 = 0;
        tiempoPausa1 = 0;
        pausas1 = 0;
      }
    }
  }


  if (valvula == 2) {

    if (actoRiego2 == 0) {
      Serial.println("Activando riego Válvula 2...");
      Serial.println("Válvula 2 abierta");
      digitalWrite(pinValvula2, HIGH); // Abre la válvula
      tiempoAnterior2 = tiempoActual; // Guardamos el tiempo en que se abrió la válvula
      actoRiego2 = 1; // Cambiamos el estado a 1 para esperar la duración
    }

    if (actoRiego2 == 1 && tiempoActual - tiempoAnterior2 >= duracion2 * 1000) {
      // Si ha pasado el tiempo de duración
      Serial.println("Válvula 2 cerrada");
      digitalWrite(pinValvula2, LOW); // Cierra la válvula
      actoRiego2 = 2; // Cambiamos el estado para manejar la pausa
      tiempoPausa2 = tiempoActual; // Guardamos el tiempo de la pausa
    }
    Serial.println("Pausas fuera: ");
    Serial.println(pausas2);
    if (pausas2 == 0 && duracionPausa2 > 0) {
          actualizarDuracionPausa(2);
          Serial.println("Duracion actualizada a 0");
        }

    if (actoRiego2 == 2 && tiempoActual - tiempoPausa2 >= duracionPausa2 * 1000) {
      // Si ha pasado el tiempo de la pausa
      if (pausas2 > 0) {
        restarPausa(2);
        Serial.println("Pausas: ");
        Serial.println(pausas2);
        if (pausas2 == 0) {
          actualizarDuracionPausa(2);
          Serial.println("Duracion actualizada a 0");
        }
        Serial.println("Abriendo válvula para siguiente ciclo...");
        digitalWrite(pinValvula2, HIGH); // Abre la válvula nuevamente
        tiempoAnterior2 = tiempoActual; // Guardamos el tiempo en que se abrió la válvula
        actoRiego2 = 1; // Esperamos la duración para el siguiente ciclo
      } else {
        Serial.println("Riego completado para la válvula 2.");
        actoRiego2 = 3; // Fin del riego
        // Restablecer variables de control al finalizar el ciclo
        isRiego2Activo = false; // Restablecemos la bandera de riego
        // Reiniciar variables de control
        actoRiego2 = 0;
        tiempoAnterior2 = 0;
        tiempoPausa2 = 0;
        pausas2 = 0;

      }
    }
  }
}

void restarPausa(int valvula) {
  HTTPClient http;
  String urlRestarPausa;

  // Asignar la URL de acuerdo con la válvula
  if (valvula == 1) {
    urlRestarPausa = urlRestarPausa1;
  } else if (valvula == 2) {
    urlRestarPausa = urlRestarPausa2;
  }

  http.begin(urlRestarPausa);
  http.addHeader("Content-Type", "application/json");

  // Realizar la solicitud PUT sin cuerpo, solo llamando a la API
  int httpCode = http.PUT("");  // No se envía cuerpo en el PUT

  if (httpCode > 0) {
    Serial.println("Pausa restada correctamente.");
    Serial.println("Respuesta del servidor: " + http.getString());
  } else {
    Serial.println("Error al restar pausa.");
  }

  http.end();
}

void actualizarDuracionPausa(int valvula) {
  HTTPClient http;
  String urlActualizar;

  // Asignar la URL de acuerdo con la válvula
  if (valvula == 1) {
    urlActualizar = urlDuracionPausa1;
  } else if (valvula == 2) {
    urlActualizar = urlDuracionPausa2;
  }

  http.begin(urlActualizar);
  http.addHeader("Content-Type", "application/json");

  // Realizar la solicitud PUT sin cuerpo, solo llamando a la API
  int httpCode = http.PUT("");  // No se envía cuerpo en el PUT

  if (httpCode > 0) {
    Serial.println("Duración de la pausa actualizada a 0 correctamente.");
    Serial.println("Respuesta del servidor: " + http.getString());
  } else {
    Serial.println("Error al actualizar la duración de la pausa.");
  }

  http.end();
}




void actualizarEstadoRiego(int valvula) {
  HTTPClient http;
  String urlActualizar;
  if (valvula == 1) {
    urlActualizar = urlActualizarEstado1;
  } else {
    urlActualizar = urlActualizarEstado2;
  }

  http.begin(urlActualizar);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.PUT(""); // No se envía body, solo se llama a la API

  if (httpCode > 0) {
    Serial.println("Estado de riego actualizado correctamente.");
    Serial.println("Respuesta del servidor: " + http.getString());
  } else {
    Serial.println("Error al actualizar el estado de riego.");
  }

  http.end();
}

String obtenerDiaSemana(int dia) {
  String dias[] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};
  return dias[dia];
}

String formatoFecha(int anio, int mes, int dia) {
  char buffer[11];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", anio, mes, dia);
  return String(buffer);
}

String formatoHora(int hora, int minuto) {
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", hora, minuto);
  return String(buffer);
}

// Función para convertir hora AM/PM a 24 horas
String convertirHora12a24(String hora12) {
  int hora = hora12.substring(0, 2).toInt();
  int minuto = hora12.substring(3, 5).toInt();
  String ampm = hora12.substring(6, 8);

  // Convertir hora AM/PM a 24 horas
  if (ampm == "PM" && hora != 12) {
    hora += 12;
  }
  if (ampm == "AM" && hora == 12) {
    hora = 0;
  }

  // Formatear la hora a 24 horas
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", hora, minuto);
  return String(buffer);
}