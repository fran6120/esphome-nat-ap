#ifndef NAT_AP_H
#define NAT_AP_H

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>      // For esp_netif_t, esp_netif_ip_info_t, etc.
#include <string>
#include <vector>

// LWIP includes for NAPT and Port Mapping
#include "lwip/lwip_napt.h" // Crucial para NAPT en LWIP
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netif.h>     // For struct netif
#include <lwip/dns.h>       // For dns_setserver and ip_addr_t
#include <lwip/prot/ip.h>   // <-- Esta es la que define IP_PROTOCOL_TCP, IP_PROTOCOL_UDP
#include "lwip/opt.h"
                          
//#include <lwip/priv/tcp_portmap.h> // <-- Posiblemente necesario para ip_portmap_add,
                                    // o esta función esté en los headers de NAPT de martin-ger.

// For ESP-IDF DHCP server functions and types
#include "dhcpserver/dhcpserver.h" // For dhcps_offer_t and esp_netif_dhcps_option

// Definiciones globales para el evento Wi-Fi
static const char *TAG = "NatAp";
#define EXAMPLE_MAX_STA_CONN 4 // Número máximo de clientes AP

namespace esphome {

// --- Mover estas definiciones AQUI, antes de la clase NatAp ---
// Enumeración para el tipo de protocolo de redirección de puertos
enum PortForwardingProtocol {
    PROTOCOL_TCP,
    PROTOCOL_UDP,
    PROTOCOL_TCP_UDP // Opción para aplicar a ambos
};

// Estructura para una regla de redirección de puertos
struct PortForwardingRule {
    PortForwardingProtocol protocol;
    uint16_t external_port;
    ip4_addr_t internal_ip; // Se almacenará como ip4_addr_t de LWIP
    uint16_t internal_port;
};
// --- Fin de las definiciones movidas ---


class NatAp : public Component {
public:
    NatAp(const std::string& ap_ssid = "ESPHomeAP",
          const std::string& ap_password = "ESPHomeAPPass",
          const std::string& ap_ip_address = "192.168.4.1",
          bool hide_ssid = false) // <-- Nuevo parámetro con valor por defecto 'false'
        : ap_ssid_(ap_ssid), ap_password_(ap_password), ap_ip_address_(ap_ip_address),
          hide_ssid_(hide_ssid), // <-- Inicializar nueva variable
          esp_netif_ap(nullptr), esp_netif_sta(nullptr) {
        global_nat_ap_instance = this;
    }

    void setup() override {
        ESP_LOGI(TAG, "Configurando el componente NAT AP...");

        // 1. Crear la interfaz del AP y obtener la interfaz del STA.
        esp_netif_ap = esp_netif_create_default_wifi_ap();
        if (!esp_netif_ap) {
            ESP_LOGE(TAG, "FALLO: No se pudo crear la interfaz AP predeterminada.");
            this->mark_failed();
            return;
        }
        ESP_LOGI(TAG, "Interfaz AP predeterminada creada.");

        esp_netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (!esp_netif_sta) {
            ESP_LOGE(TAG, "FALLO: No se pudo obtener el handle de la interfaz STA (WIFI_STA_DEF). NAT no funcionará.");
            this->mark_failed();
            return;
        }
        ESP_LOGI(TAG, "Interfaz STA obtenida.");

        // 2. Configurar la interfaz del AP (IP estática, DHCP, DNS)
        configure_ap_interface();

        // 3. Establecer el modo Wi-Fi a APSTA
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_LOGI(TAG, "Modo Wi-Fi establecido a APSTA.");

        // 4. Configurar el Wi-Fi del AP (SSID, contraseña, canal, etc.)
        wifi_config_t ap_config = {
            .ap = {
                .ssid = {0},
                .password = {0},
                .channel = 1, // Puedes hacer esto configurable si lo necesitas
                .authmode = WIFI_AUTH_WPA_WPA2_PSK,
                .ssid_hidden = (uint8_t)hide_ssid_, // <-- ¡Aquí usamos hide_ssid_!
                .max_connection = EXAMPLE_MAX_STA_CONN,
                .beacon_interval = 100,
                .pmf_cfg = {
                    .capable = true,
                    .required = false
                },
            },
        };

        strncpy((char*)ap_config.ap.ssid, ap_ssid_.c_str(), sizeof(ap_config.ap.ssid) - 1);
        strncpy((char*)ap_config.ap.password, ap_password_.c_str(), sizeof(ap_config.ap.password) - 1);
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_LOGI(TAG, "Configuración Wi-Fi del AP aplicada.");

        // 5. Registrar el handler de eventos IP para la STA.
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &NatAp::s_wifi_event_handler, nullptr, &instance_got_ip));
        ESP_LOGI(TAG, "Handler de eventos IP para STA registrado.");

        // 6. Iniciar el Wi-Fi (esto levanta el AP y el STA)
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "Wi-Fi iniciado (AP y STA levantados).");

        // *** AÑADIR ESTA RE-VERIFICACIÓN ***
        esp_netif_ip_info_t sta_ip_info;
        if (esp_netif_get_ip_info(esp_netif_sta, &sta_ip_info) == ESP_OK && sta_ip_info.ip.addr != 0) {
            ESP_LOGI(TAG, "STA ya tenía IP al finalizar setup(): " IPSTR ". Habilitando NAPT de inmediato.", IP2STR(&sta_ip_info.ip));
            enable_napt();
        } else {
            ESP_LOGI(TAG, "Componente NAT AP configurado. Esperando IP de la STA (si aún no la tiene)...");
        }
    }

    void loop() override {
        // No hay tareas continuas activas aquí.
    }

    float get_setup_priority() const override { return esphome::setup_priority::AFTER_WIFI; }

    // Método para añadir una regla de redirección de puertos
    // Ahora 'PortForwardingProtocol' y 'PortForwardingRule' están declarados antes de la clase
    void add_port_forwarding_rule(PortForwardingProtocol protocol, uint16_t external_port,
                                  const std::string& internal_ip_str, uint16_t internal_port) {
        PortForwardingRule rule; // Ahora reconocida
        rule.protocol = protocol;
        rule.external_port = external_port;
        
        if (ip4addr_aton(internal_ip_str.c_str(), &rule.internal_ip) == 0) {
            ESP_LOGE(TAG, "IP interna inválida para la regla de redirección: %s", internal_ip_str.c_str());
            return;
        }
        rule.internal_port = internal_port;
        forwarding_rules_.push_back(rule); // Ahora reconocida
        ESP_LOGI(TAG, "Regla de redirección añadida (sin aplicar aún): Ext %u:%u -> Int %s:%u (Prot: %d)",
                 external_port, external_port, internal_ip_str.c_str(), internal_port, (int)protocol);
    }

protected:
    std::string ap_ssid_;
    std::string ap_password_;
    std::string ap_ip_address_;
    bool hide_ssid_;

    esp_netif_t *esp_netif_ap;
    esp_netif_t *esp_netif_sta;

    esp_event_handler_instance_t instance_got_ip;

    static NatAp* global_nat_ap_instance;

    std::vector<PortForwardingRule> forwarding_rules_; // <-- Aquí se declara el miembro de la clase
    bool napt_enabled_ = false;

    static void s_wifi_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data) {
        if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            ESP_LOGI(TAG, "STA obtuvo IP:" IPSTR, IP2STR(&event->ip_info.ip));
            if (global_nat_ap_instance) {
                global_nat_ap_instance->enable_napt();
            }
        }
    }

    void enable_napt() {
        if (napt_enabled_) {
            ESP_LOGI(TAG, "NAPT ya está habilitado. No es necesario volver a habilitarlo.");
            return;
        }

        ESP_LOGI(TAG, "Habilitando NAPT...");
        esp_netif_ip_info_t ap_ip_info;
        if (esp_netif_get_ip_info(esp_netif_ap, &ap_ip_info) != ESP_OK) {
            ESP_LOGE(TAG, "No se pudo obtener la información de IP del AP. NAPT no se habilitará.");
            return;
        }

        ip_napt_enable(ap_ip_info.ip.addr, 1);
        ESP_LOGI(TAG, "NAPT habilitado en la IP del AP: " IPSTR, IP2STR(&ap_ip_info.ip));
        
        napt_enabled_ = true; // <-- Marcar NAPT como habilitado

        apply_port_forwarding_rules(); 
    }

    void apply_port_forwarding_rules() {
        if (forwarding_rules_.empty()) {
            ESP_LOGI(TAG, "No hay reglas de redirección de puertos para aplicar.");
            return;
        }

        esp_netif_ip_info_t sta_ip_info;
        if (esp_netif_get_ip_info(esp_netif_sta, &sta_ip_info) != ESP_OK || sta_ip_info.ip.addr == 0) {
            ESP_LOGE(TAG, "No se pudo obtener la información de IP de la STA o STA no tiene IP. No se aplicarán las reglas de redirección.");
            return;
        }

        ESP_LOGI(TAG, "Aplicando %u reglas de redirección de puertos con IP externa STA: " IPSTR,
                 forwarding_rules_.size(), IP2STR(&sta_ip_info.ip));

        for (const auto& rule : forwarding_rules_) {
            uint8_t proto_lwip; // Esto debería ser de tipo u8_t como espera ip_portmap_add

            if (rule.protocol == PROTOCOL_TCP_UDP) {
                ESP_LOGI(TAG, "Aplicando regla TCP+UDP para ext:%u -> int:%s:%u",
                         rule.external_port, ip4addr_ntoa(&rule.internal_ip), rule.internal_port);

                // USAR IPPROTO_TCP/UDP
                ip_portmap_add(IPPROTO_TCP, sta_ip_info.ip.addr, rule.external_port,
                               rule.internal_ip.addr, rule.internal_port);

                ip_portmap_add(IPPROTO_UDP, sta_ip_info.ip.addr, rule.external_port,
                               rule.internal_ip.addr, rule.internal_port);
                continue;
            }

            if (rule.protocol == PROTOCOL_TCP) {
                proto_lwip = IPPROTO_TCP; // USAR IPPROTO_TCP
            } else if (rule.protocol == PROTOCOL_UDP) {
                proto_lwip = IPPROTO_UDP; // USAR IPPROTO_UDP
            } else {
                ESP_LOGE(TAG, "Protocolo desconocido para la regla de redirección.");
                continue;
            }

            ip_portmap_add(proto_lwip, sta_ip_info.ip.addr, rule.external_port,
                           rule.internal_ip.addr, rule.internal_port);

            ESP_LOGI(TAG, "Regla de redirección aplicada: %s ext:" IPSTR ":%u -> int:%s:%u",
                     (proto_lwip == IPPROTO_TCP ? "TCP" : "UDP"), // USAR IPPROTO_TCP en el log
                     IP2STR(&sta_ip_info.ip), rule.external_port,
                     ip4addr_ntoa(&rule.internal_ip), rule.internal_port);
        }
    }

    void configure_ap_interface() {
        ESP_LOGI(TAG, "Configurando la interfaz AP (IP estática, DNS y opciones DHCP)...");

        esp_err_t err = esp_netif_dhcps_stop(esp_netif_ap);
        if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
             ESP_LOGW(TAG, "esp_netif_dhcps_stop() falló inesperadamente: %s", esp_err_to_name(err));
        }

        // Usar la IP configurada desde el constructor
        esp_netif_ip_info_t ip_info{};
        ip4_addr_t ip4, gw4, sn4;
        
        // Convertir la IP de string a ip4_addr_t
        if (ip4addr_aton(ap_ip_address_.c_str(), &ip4) == 0) { // <-- Usar ap_ip_address_
            ESP_LOGE(TAG, "FALLO: IP del AP inválida: %s. Usando 192.168.4.1 por defecto.", ap_ip_address_.c_str());
            ip4addr_aton("192.168.4.1", &ip4); // Fallback a una IP conocida
        }
        
        gw4 = ip4; // El gateway es la misma IP del AP
        ip4addr_aton("255.255.255.0", &sn4);

        ip_info.ip.addr      = ip4.addr;
        ip_info.gw.addr      = gw4.addr;
        ip_info.netmask.addr = sn4.addr;

        err = esp_netif_set_ip_info(esp_netif_ap, &ip_info);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "IP estática del AP configurada: " IPSTR, IP2STR(&ip_info.ip)); // Log actualizado
        } else {
            ESP_LOGE(TAG, "esp_netif_set_ip_info() falló: %s", esp_err_to_name(err));
        }

        // ... (el resto de configure_ap_interface sin cambios, la parte de DNS) ...
    }
};

NatAp* NatAp::global_nat_ap_instance = nullptr;

} // namespace esphome

#endif // NAT_AP_H
