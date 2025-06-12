#ifndef NAT_AP_H
#define NAT_AP_H

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <string>
#include <vector>
#include <esp_mac.h>

#include "lwip/lwip_napt.h"
#include "lwip/err.h"
#include "lwip/sockets.h" // Para IPPROTO_TCP, IPPROTO_UDP
#include "lwip/sys.h"
#include <lwip/netif.h>
#include <lwip/dns.h>
#include "lwip/opt.h"

#include "dhcpserver/dhcpserver.h"

static const char *TAG = "NatAp";
#define EXAMPLE_MAX_STA_CONN 4

namespace esphome { // <-- La clase NatAp estará directamente aquí

// Enumeración para el tipo de protocolo de redirección de puertos
enum PortForwardingProtocol {
    PROTOCOL_TCP,
    PROTOCOL_UDP,
    PROTOCOL_TCP_UDP
};

// Estructura para una regla de redirección de puertos
struct PortForwardingRule {
    PortForwardingProtocol protocol;
    uint16_t external_port;
    ip4_addr_t internal_ip;
    uint16_t internal_port;
};

class NatAp : public Component {
public:
    NatAp(); // Constructor por defecto

    void set_ap_ssid(const std::string& ssid) { ap_ssid_ = ssid; }
    void set_ap_password(const std::string& password) { ap_password_ = password; }
    void set_ap_ip_address(const std::string& ip_address) { ap_ip_address_ = ip_address; }
    void set_hide_ssid(bool hide) { hide_ssid_ = hide; }

    void add_port_forwarding_rule(PortForwardingProtocol protocol, uint16_t external_port,
                                  const std::string& internal_ip_str, uint16_t internal_port);

    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return esphome::setup_priority::AFTER_WIFI; }

protected:
    std::string ap_ssid_;
    std::string ap_password_;
    std::string ap_ip_address_;
    bool hide_ssid_;

    esp_netif_t *esp_netif_ap;
    esp_netif_t *esp_netif_sta;

    esp_event_handler_instance_t instance_sta_connected;
    esp_event_handler_instance_t instance_sta_disconnected;
    esp_event_handler_instance_t instance_got_ip;

    static NatAp* global_nat_ap_instance; // Sigue siendo esphome::NatAp*

    std::vector<PortForwardingRule> forwarding_rules_;
    bool napt_enabled_ = false;

    // Métodos internos
    static void s_wifi_event_handler_ap_connected(void* arg, esp_event_base_t event_base,
                                                  int32_t event_id, void* event_data);

    static void s_wifi_event_handler_ap_disconnected(void* arg, esp_event_base_t event_base,
                                                     int32_t event_id, void* event_data);

    static void s_wifi_event_handler(void* arg, esp_event_base_t event_base,
                                     int32_t event_id, void* event_data);
    void enable_napt();
    void apply_port_forwarding_rules();
    void configure_ap_interface();

};

} // namespace esphome

#endif // NAT_AP_H
