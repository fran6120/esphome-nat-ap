# 📡 ESP32-C3 NAT AP para ESPHome

Este proyecto transforma un dispositivo **ESP32-C3** en un **punto de acceso Wi-Fi (AP)** con capacidad de **Traducción de Direcciones de Red (NAT)**, actuando como un **mini-router** configurable directamente desde **ESPHome**. Implementado como un **componente externo**, permite una integración limpia y declarativa en tus configuraciones YAML.

---

## 📝 Descripción General

Este **componente externo personalizado** para ESPHome permite que tu ESP32-C3 cree su propia red Wi-Fi local (AP), mientras se conecta a una red Wi-Fi existente (modo Estación o STA). Los dispositivos conectados al AP del ESP32 pueden acceder a internet a través de la conexión STA del ESP32, gracias a la implementación de NAT (Network Address Translation). Además, ofrece la posibilidad de configurar redirecciones de puertos (Port Forwarding) para exponer servicios internos al exterior.

---

## ✨ Funcionalidades Clave

* **Punto de Acceso Wi-Fi (AP) Configurable**: Define el SSID, la contraseña y la visibilidad (oculto/visible) de tu red Wi-Fi creada por el ESP32.

* **Modo Estación (STA) Gestionado por ESPHome**: El ESP32 se conecta a tu red Wi-Fi principal como un cliente estándar, con la configuración de Wi-Fi gestionada por ESPHome.

* **Traducción de Direcciones de Red (NAT)**: Los dispositivos en la red del AP pueden acceder a internet a través de la conexión STA del ESP32.

* **Redirección de Puertos (Port Forwarding)**: Configura reglas para redirigir el tráfico entrante desde tu red principal a dispositivos específicos en la red interna del AP.

* **IP Interna del AP Personalizable**: Define la dirección IP de la red Wi-Fi generada por el ESP32 (ej., `192.168.10.1`) directamente desde tu configuración YAML.

* **Basado en ESP-IDF 5.1.6**: Utiliza las últimas APIs y funcionalidades de ESP-IDF para una implementación robusta y moderna.

---

## 🙏 Agradecimientos y Créditos

Este proyecto se ha desarrollado partiendo del concepto y la base del componente [mag1024/esphome-nat-ap](https://github.com/mag1024/esphome-nat-ap). La implementación del núcleo de NAT y la adopción de las APIs modernas de ESP-IDF se han inspirado y adaptado crucialmente del proyecto [martin-ger/esp32_nat_router](https://github.com/martin-ger/esp32_nat_router).

La redacción de este `README.md` y la asistencia en la adaptación y depuración del código ha sido posible gracias a la ayuda de **Gemini**.

---

## 🚀 Configuración Mínima de ESPHome

yaml más abajo, al final de la descripción en inglés.

---

# 📡 ESP32-C3 NAT AP for ESPHome

This project transforms an ESP32-C3 device into a Wi-Fi access point (AP) capable of Network Address Translation (NAT), acting as a mini-router that can be configured directly from ESPHome. Implemented as an external component, it allows for clean, declarative integration into your YAML configurations.

---

## 📝 Overview

This custom external component for ESPHome allows your ESP32-C3 to create its own local Wi-Fi network (AP) while connecting to an existing Wi-Fi network (Station or STA mode). Devices connected to the ESP32 AP can access the internet through the ESP32 STA connection, thanks to the implementation of NAT (Network Address Translation). It also offers the ability to configure port forwarding to expose internal services to the outside world.

---

## ✨ Key Features

* **Configurable Wi-Fi Access Point (AP):** Define the SSID, password, and visibility (hidden/visible) of your Wi-Fi network created by the ESP32.

* **Station Mode (STA) Managed by ESPHome:** The ESP32 connects to your main Wi-Fi network as a standard client, with Wi-Fi settings managed by ESPHome.

* **Network Address Translation (NAT):** Devices on the AP's network can access the internet through the ESP32's STA connection.

* **Port Forwarding:** Configure rules to redirect incoming traffic from your main network to specific devices on the AP's internal network.

* **Customizable AP Internal IP:** Define the Wi-Fi network IP address generated by the ESP32 (e.g., `192.168.10.1`) directly from your YAML configuration.

* **Based on ESP-IDF 5.1.6:** Uses the latest ESP-IDF APIs and features for a robust and modern implementation.

---

## 🙏 Acknowledgments and Credits

This project was developed based on the concept and foundation of the [mag1024/esphome-nat-ap](https://github.com/mag1024/esphome-nat-ap) component. The core NAT implementation and the adoption of modern ESP-IDF APIs were crucially inspired and adapted from the [martin-ger/esp32_nat_router](https://github.com/martin-ger/esp32_nat_router) project.

The writing of this `README.md` and assistance in adapting and debugging the code were made possible with the help of **Gemini**.

---

## 🚀 ESPHome Minimal Configuration

To use this component, you must structure your files as follows:

```
config_esphome/
├── config.yaml
└── components/
    └── nat_ap/
        ├── __init__.py
        ├── nat_ap.h
        └── nat_ap.cpp
```

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/fran6120/esphome-nat-ap
    components: nat_ap
    refresh: always

esphome:
  name: esp32-c3-nat
  friendly_name: esp32-c3-nat

esp32:
  board: esp32-c3-devkitm-1
  cpu_frequency: 160MHZ
  framework:
    type: esp-idf

logger:
  level: DEBUG 

api:
  encryption:
    key: "" 
  reboot_timeout: 0s
    
ota:
  - platform: esphome
    password: "" 

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

nat_ap:
  ap_ssid: "ESPHomeNATAP"
  ap_password: "ESPHomeNATAP"
  ap_ip_address: "192.168.10.1"
  hide_ssid: true
  port_forwarding:
    - protocol: TCP
      external_port: 10001
      internal_ip: "192.168.10.2"
      internal_port: 10001
    - protocol: UDP
      external_port: 12345
      internal_ip: "192.168.10.2"
      internal_port: 12345
```
