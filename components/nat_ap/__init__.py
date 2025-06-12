import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@tu_usuario_github"]

DEPENDENCIES = ["wifi"]

# Definir el componente y el namespace C++
nat_ap_ns = cg.esphome_ns # La clase NatAp está directamente en esphome::
NatAp = nat_ap_ns.class_("NatAp", cg.Component)

# --- Referencia explícita a la enumeración C++ ---
# Obtenemos una referencia al tipo de enumeración C++ PortForwardingProtocol
# Esto le dice a CodeGen cómo debe renderizar los valores de esta enumeración en C++
PortForwardingProtocolEnum = nat_ap_ns.enum("PortForwardingProtocol")

# Esquema para una regla de redirección de puertos
PORT_FORWARDING_PROTOCOL_SCHEMA = cv.enum(
    {
        "TCP": PortForwardingProtocolEnum.PROTOCOL_TCP,    # Ahora usamos la referencia explícita
        "UDP": PortForwardingProtocolEnum.PROTOCOL_UDP,    # que se traduce a "PortForwardingProtocol::PROTOCOL_TCP"
        "TCP_UDP": PortForwardingProtocolEnum.PROTOCOL_TCP_UDP,
    },
    upper=True,
)

PORT_FORWARDING_RULE_SCHEMA = cv.Schema(
    {
        cv.Required("protocol"): PORT_FORWARDING_PROTOCOL_SCHEMA,
        cv.Required("external_port"): cv.port,
        cv.Required("internal_ip"): cv.ip_address,
        cv.Required("internal_port"): cv.port,
    }
)

# Esquema principal para el componente nat_ap
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NatAp),
        cv.Optional("ap_ssid", default="ESPHomeAP"): cv.string,
        cv.Optional("ap_password", default="ESPHomeAPPass"): cv.All(cv.string, cv.Length(min=8)),
        cv.Optional("ap_ip_address", default="192.168.4.1"): cv.ip_address,
        cv.Optional("hide_ssid", default=False): cv.boolean,
        cv.Optional("port_forwarding"): cv.All(
            cv.ensure_list(PORT_FORWARDING_RULE_SCHEMA), cv.Length(min=0)
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

# --- Función para Generar el Código C++ ---
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    cg.add(var.set_ap_ssid(config["ap_ssid"]))
    cg.add(var.set_ap_password(config["ap_password"]))
    cg.add(var.set_ap_ip_address(str(config["ap_ip_address"])))
    cg.add(var.set_hide_ssid(config["hide_ssid"]))

    if "port_forwarding" in config:
        for rule in config["port_forwarding"]:
            # Pasamos directamente el valor del enum validado por el esquema.
            # CodeGen debería saber cómo renderizar esto correctamente con "::"
            cg.add(
                var.add_port_forwarding_rule(
                    rule["protocol"], # <-- Esto es un EnumRef, CodeGen lo debería renderizar bien
                    rule["external_port"],
                    str(rule["internal_ip"]),
                    rule["internal_port"],
                )
            )

    await cg.register_component(var, config)

    cg.add_build_flag("-DIP_NAPT=1")
    cg.add_build_flag("-DIP_NAPT_PORTMAP=1")
    cg.add_build_flag("-DLWIP_IPV4_NAPT=1")

    add_idf_sdkconfig_option("CONFIG_LWIP_IP_FORWARD", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_IPV4_NAPT", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_IPV4_NAPT_PORTMAP", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_DHCP_OPTIONS", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_DHCPS", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_DHCP_DOES_ACD_CHECK", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_DHCPS_STATIC_ENTRIES", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_DHCPS_ADD_DNS", True)
    add_idf_sdkconfig_option("CONFIG_ESP_WIFI_SOFTAP_SUPPORT", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_L2_TO_L3_COPY", True)
    add_idf_sdkconfig_option("CONFIG_LWIP_MEMP_NUM_NETCONN", "12")
    add_idf_sdkconfig_option("CONFIG_LWIP_MEMP_NUM_TCP_PCB", "12")
    add_idf_sdkconfig_option("CONFIG_LWIP_TCP_SND_BUF", "8192")
    add_idf_sdkconfig_option("CONFIG_LWIP_TCP_WND", "8192")

    cg.add_build_flag("-fno-tree-switch-conversion")
