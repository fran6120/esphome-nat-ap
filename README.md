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

Para utilizar este componente, deberás estructurar tus archivos de la siguiente manera:

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
  - source: /config/components
    components: nat_ap
    refresh: always

esphome:
  name: esp32-c3-nat
  friendly_name: esp32-c3-nat
  platformio_options:
    build_flags:
      - "-DIP_NAPT=1"
      - "-DIP_NAPT_PORTMAP=1"
      - "-DLWIP_IPV4_NAPT=1"

esp32:
  board: esp32-c3-devkitm-1 # Ajusta a tu placa específica
  cpu_frequency: 160MHZ # Ajusta a la frecuencia de tu CPU
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_COMPILER_OPTIMIZATION_SIZE: y
      CONFIG_LWIP_IP_FORWARD: y
      CONFIG_LWIP_IPV4_NAPT: y
      CONFIG_LWIP_IPV4_NAPT_PORTMAP: y # Habilita el mapeo de puertos
      CONFIG_LWIP_DHCP_OPTIONS: y
      CONFIG_LWIP_DHCPS: y # Habilita el servidor DHCP
      CONFIG_LWIP_DHCP_DOES_ACD_CHECK: y
      CONFIG_LWIP_DHCPS_STATIC_ENTRIES: y
      CONFIG_LWIP_DHCPS_ADD_DNS: y # Permite al DHCPS añadir DNS
      CONFIG_LWIP_DHCP_DISABLE_VENDOR_CLASS_ID: y
      CONFIG_ESP_WIFI_SOFTAP_SUPPORT: y
      CONFIG_LWIP_L2_TO_L3_COPY: y
      CONFIG_LWIP_MEMP_NUM_NETCONN: "12"
      CONFIG_LWIP_MEMP_NUM_TCP_PCB: "12"
      CONFIG_LWIP_TCP_SND_BUF: "8192"
      CONFIG_LWIP_TCP_WND: "8192"
 
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
