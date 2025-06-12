#include "nat_ap.h"

namespace esphome { // <-- La clase NatAp estará directamente aquí

// Inicialización de la variable estática fuera de la clase
NatAp* NatAp::global_nat_ap_instance = nullptr; // Sigue siendo esphome::NatAp*

NatAp::NatAp() // Constructor por defecto
    : ap_ssid_("ESPHomeAP"), ap_password_("ESPHomeAPPass"), ap_ip_address_("192.168.4.1"),
      hide_ssid_(false),
      esp_netif_ap(nullptr), esp_netif_sta(nullptr) {
    global_nat_ap_instance = this;
}

// ... (Resto de la implementación de los métodos de NatAp) ...

void NatAp::add_port_forwarding_rule(PortForwardingProtocol protocol, uint16_t external_port,
                                     const std::string& internal_ip_str, uint16_t internal_port) {
    PortForwardingRule rule;
    rule.protocol = protocol;
    rule.external_port = external_port;
    
    if (ip4addr_aton(internal_ip_str.c_str(), &rule.internal_ip) == 0) {
        ESP_LOGE(TAG, "IP interna inválida para la regla de redirección: %s", internal_ip_str.c_str());
        return;
    }
    rule.internal_port = internal_port;
    forwarding_rules_.push_back(rule);
    ESP_LOGI(TAG, "Regla de redirección añadida (sin aplicar aún): Ext %u -> Int %s:%u (Prot: %d)",
             external_port, internal_ip_str.c_str(), internal_port, (int)protocol);
}

void NatAp::setup() {
    ESP_LOGI(TAG, "Configurando el componente NAT AP...");

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

    configure_ap_interface();

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_LOGI(TAG, "Modo Wi-Fi establecido a APSTA.");

    wifi_config_t ap_config = {
        .ap = {
            .ssid = {0},
            .password = {0},
            .channel = 1,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .ssid_hidden = (uint8_t)hide_ssid_,
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

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STACONNECTED, &NatAp::s_wifi_event_handler_ap_connected, nullptr, &instance_sta_connected));
    ESP_LOGI(TAG, "Handler de eventos AP_STACONNECTED registrado.");
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_STADISCONNECTED, &NatAp::s_wifi_event_handler_ap_disconnected, nullptr, &instance_sta_disconnected));
    ESP_LOGI(TAG, "Handler de eventos AP_STADISCONNECTED registrado.");

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &NatAp::s_wifi_event_handler, nullptr, &instance_got_ip));
    ESP_LOGI(TAG, "Handler de eventos IP para STA registrado.");

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi iniciado (AP y STA levantados).");

    esp_netif_ip_info_t sta_ip_info;
    if (esp_netif_get_ip_info(esp_netif_sta, &sta_ip_info) == ESP_OK && sta_ip_info.ip.addr != 0) {
        ESP_LOGI(TAG, "STA ya tenía IP al finalizar setup(): " IPSTR ". Habilitando NAPT de inmediato.", IP2STR(&sta_ip_info.ip));
        enable_napt();
    } else {
        ESP_LOGI(TAG, "Componente NAT AP configurado. Esperando IP de la STA (si aún no la tiene)...");
    }
}

void NatAp::loop() {
    // No hay tareas continuas activas aquí.
}


void NatAp::s_wifi_event_handler_ap_connected(void* arg, esp_event_base_t event_base,
                                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data; // Es la misma estructura que stadisconnected para MAC y AID
        ESP_LOGI(TAG, "Cliente conectado al AP: MAC " MACSTR ", AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void NatAp::s_wifi_event_handler_ap_disconnected(void* arg, esp_event_base_t event_base,
                                                 int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Cliente desconectado del AP: MAC " MACSTR ", AID=%d, Razón=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}

void NatAp::s_wifi_event_handler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data) {
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "STA obtuvo IP:" IPSTR, IP2STR(&event->ip_info.ip));
        if (global_nat_ap_instance) {
            global_nat_ap_instance->enable_napt();
        }
    }
}

void NatAp::enable_napt() {
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
    
    napt_enabled_ = true;

    apply_port_forwarding_rules(); 
}

void NatAp::apply_port_forwarding_rules() {
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
        uint8_t proto_lwip;
        
        if (rule.protocol == PROTOCOL_TCP_UDP) {
            ESP_LOGI(TAG, "Aplicando regla TCP+UDP para ext:%u -> int:%s:%u",
                     rule.external_port, ip4addr_ntoa(&rule.internal_ip), rule.internal_port);
            
            ip_portmap_add(IPPROTO_TCP, sta_ip_info.ip.addr, rule.external_port,
                           rule.internal_ip.addr, rule.internal_port);

            ip_portmap_add(IPPROTO_UDP, sta_ip_info.ip.addr, rule.external_port,
                           rule.internal_ip.addr, rule.internal_port);
            continue;
        }

        if (rule.protocol == PROTOCOL_TCP) {
            proto_lwip = IPPROTO_TCP;
        } else if (rule.protocol == PROTOCOL_UDP) {
            proto_lwip = IPPROTO_UDP;
        } else {
            ESP_LOGE(TAG, "Protocolo desconocido para la regla de redirección.");
            continue;
        }

        ip_portmap_add(proto_lwip, sta_ip_info.ip.addr, rule.external_port,
                       rule.internal_ip.addr, rule.internal_port);
        
        ESP_LOGI(TAG, "Regla de redirección aplicada: %s ext:" IPSTR ":%u -> int:%s:%u",
                 (proto_lwip == IPPROTO_TCP ? "TCP" : "UDP"),
                 IP2STR(&sta_ip_info.ip), rule.external_port,
                 ip4addr_ntoa(&rule.internal_ip), rule.internal_port);
    }
}

void NatAp::configure_ap_interface() {
    ESP_LOGI(TAG, "Configurando la interfaz AP (IP estática, DNS y opciones DHCP)...");

    esp_err_t err = esp_netif_dhcps_stop(esp_netif_ap);
    if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
         ESP_LOGW(TAG, "esp_netif_dhcps_stop() falló inesperadamente: %s", esp_err_to_name(err));
    }

    esp_netif_ip_info_t ip_info{};
    ip4_addr_t ip4, gw4, sn4;
    
    if (ip4addr_aton(ap_ip_address_.c_str(), &ip4) == 0) {
        ESP_LOGE(TAG, "FALLO: IP del AP inválida: %s. Usando 192.168.4.1 por defecto.", ap_ip_address_.c_str());
        ip4addr_aton("192.168.4.1", &ip4);
    }
    
    gw4 = ip4;
    ip4addr_aton("255.255.255.0", &sn4);

    ip_info.ip.addr      = ip4.addr;
    ip_info.gw.addr      = gw4.addr;
    ip_info.netmask.addr = sn4.addr;

    err = esp_netif_set_ip_info(esp_netif_ap, &ip_info);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "IP estática del AP configurada: " IPSTR, IP2STR(&ip_info.ip));
    } else {
        ESP_LOGE(TAG, "esp_netif_set_ip_info() falló: %s", esp_err_to_name(err));
    }

    esp_netif_dns_info_t dns_info{};
    if (esp_netif_sta && esp_netif_get_dns_info(esp_netif_sta, ESP_NETIF_DNS_MAIN, &dns_info) == ESP_OK) {
        uint32_t raw_dns_ip = dns_info.ip.u_addr.ip4.addr;
        ESP_LOGI(TAG, "DNS obtenido de STA: %u.%u.%u.%u",
                        raw_dns_ip & 0xFF, (raw_dns_ip >> 8) & 0xFF,
                        (raw_dns_ip >> 16) & 0xFF, (raw_dns_ip >> 24) & 0xFF);

        dhcps_offer_t dhcps_dns_value = OFFER_DNS;
        err = esp_netif_dhcps_option(esp_netif_ap,
                                     ESP_NETIF_OP_SET,
                                     ESP_NETIF_DOMAIN_NAME_SERVER,
                                     &dhcps_dns_value,
                                     sizeof(dhcps_dns_value));
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "esp_netif_dhcps_option(OFFER_DNS) falló: %s", esp_err_to_name(err));
        }

        err = esp_netif_set_dns_info(esp_netif_ap, ESP_NETIF_DNS_MAIN, &dns_info);
        if (err == ESP_OK) {
            uint32_t raw_dns_ip2 = dns_info.ip.u_addr.ip4.addr;
            ESP_LOGI(TAG, "DHCP del AP ofrecerá DNS: %u.%u.%u.%u",
                            raw_dns_ip2 & 0xFF, (raw_dns_ip2 >> 8) & 0xFF,
                            (raw_dns_ip2 >> 16) & 0xFF, (raw_dns_ip2 >> 24) & 0xFF);
        } else {
            ESP_LOGW(TAG, "esp_netif_set_dns_info() falló: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGW(TAG, "Imposible obtener DNS de la STA (¿aún no conectada?). Clientes del AP podrían tener DNS por defecto o no funcionar.");
    }
}

} // namespace esphome
