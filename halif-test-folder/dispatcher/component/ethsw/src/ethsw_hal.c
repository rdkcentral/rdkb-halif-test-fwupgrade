/*
 * Process-local mock implementation of the Ethernet Switch HAL
 * (ccsp_hal_ethsw.h, rdkb-halif-ethsw).
 *
 * Contract notes taken from the header and docs/pages/EthSWHAlSpec.md:
 *  - Success is RETURN_OK (0); every failure is RETURN_ERR (-1).
 *  - GWP_GetEthWanLinkStatus() returns 1 (up), 0 (down) or a negative value.
 *  - CcspHalEthSwXxx() port calls use the 1-based CCSP_HAL_ETHSW_PORT enum;
 *    the Ethernet WAN port and eth_device_t.eth_port are 0-based indexes.
 *  - The HAL is not thread-safe: callers must serialize every call.
 *  - Outputs are written only after all arguments were validated, so a
 *    RETURN_ERR never leaves a partially written output.
 *
 * This file models a gateway with 18 ports (CCSP_HAL_ETHSW_EthPort1..18); the
 * first four are Ethernet ports usable as WAN and only ports 1-2 have a peer.
 * Peers, MAC addresses and counters below are fixtures, not hardware
 * readings. All state is process-local and resets when the process restarts;
 * no switch hardware, driver or service is touched. Error causes are
 * reported through syslog because RETURN_ERR carries no cause.
 *
 * Build-time feature macros mirror the header:
 *  - CcspHalExtSw_ethPortConfigure() needs FEATURE_RDKB_WAN_MANAGER and
 *    FEATURE_RDKB_AUTO_PORT_SWITCH.
 *  - CcspHalExtSw_getCurrentWanHWConf() needs FEATURE_RDKB_AUTO_PORT_SWITCH.
 */
#include "ccsp_hal_ethsw.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

/* External Ethernet ports usable as the Ethernet WAN: EthPort1..EthPort4
 * (0-based WAN indexes 0..3, interfaces eth0..eth3).
 */
#define MOCK_ETH_PORTS 4

/* Ethernet WAN port indexes accepted by setEthWanPort(): 0..19. Indexes
 * beyond the implemented switch ports can be selected but never have link.
 */
#define MOCK_WAN_PORTS 20

/* Implemented CCSP_HAL_ETHSW_PORT values: EthPort1 (1) through
 * InterconnectPort2 (18). CCSP_HAL_ETHSW_MgmtPort and PortMax are not
 * implemented and are rejected. Ports beyond the first four have no peer.
 */
#define MOCK_PORTS 18

/* ------------------------------------------------------------------ */
/* Port fixtures and state                                             */
/* ------------------------------------------------------------------ */

/* What is plugged into each port (fixture). A connected peer negotiates the
 * listed rate and duplex when the port is configured for Auto.
 */
typedef struct {
    BOOLEAN connected;
    CCSP_HAL_ETHSW_LINK_RATE peer_rate;
    CCSP_HAL_ETHSW_DUPLEX_MODE peer_duplex;
} mock_peer_t;

/* Entries beyond the listed ones are zero-initialised: no peer attached. */
static const mock_peer_t mock_peers[MOCK_PORTS] = {
    { TRUE,  CCSP_HAL_ETHSW_LINK_1Gbps,   CCSP_HAL_ETHSW_DUPLEX_Full },
    { TRUE,  CCSP_HAL_ETHSW_LINK_100Mbps, CCSP_HAL_ETHSW_DUPLEX_Full },
    { FALSE, CCSP_HAL_ETHSW_LINK_NULL,    CCSP_HAL_ETHSW_DUPLEX_Auto },
    { FALSE, CCSP_HAL_ETHSW_LINK_NULL,    CCSP_HAL_ETHSW_DUPLEX_Auto }
};

/* Configurable per-port state. */
typedef struct {
    CCSP_HAL_ETHSW_LINK_RATE cfg_rate;
    CCSP_HAL_ETHSW_DUPLEX_MODE cfg_duplex;
    CCSP_HAL_ETHSW_ADMIN_STATUS admin;
    INT aging_speed; /* write-only in the interface; vendor-defined unit */
} mock_port_t;

#define MOCK_PORT_DEFAULTS \
    { CCSP_HAL_ETHSW_LINK_Auto, CCSP_HAL_ETHSW_DUPLEX_Auto, \
      CCSP_HAL_ETHSW_AdminUp, 0 }

static mock_port_t mock_ports[MOCK_PORTS] = {
    MOCK_PORT_DEFAULTS, MOCK_PORT_DEFAULTS, MOCK_PORT_DEFAULTS,
    MOCK_PORT_DEFAULTS, MOCK_PORT_DEFAULTS, MOCK_PORT_DEFAULTS,
    MOCK_PORT_DEFAULTS, MOCK_PORT_DEFAULTS, MOCK_PORT_DEFAULTS,
    MOCK_PORT_DEFAULTS, MOCK_PORT_DEFAULTS, MOCK_PORT_DEFAULTS,
    MOCK_PORT_DEFAULTS, MOCK_PORT_DEFAULTS, MOCK_PORT_DEFAULTS,
    MOCK_PORT_DEFAULTS, MOCK_PORT_DEFAULTS, MOCK_PORT_DEFAULTS
};

/* Fixed counters served for every implemented port. Totals are consistent:
 * packets = unicast + multicast + broadcast.
 */
static const CCSP_HAL_ETH_STATS mock_stats = {
    1000000ULL, /* BytesSent */
    2000000ULL, /* BytesReceived */
    1000UL,     /* PacketsSent */
    2000UL,     /* PacketsReceived */
    0UL,        /* ErrorsSent */
    0UL,        /* ErrorsReceived */
    900UL,      /* UnicastPacketsSent */
    1800UL,     /* UnicastPacketsReceived */
    0UL,        /* DiscardPacketsSent */
    0UL,        /* DiscardPacketsReceived */
    50UL,       /* MulticastPacketsSent */
    100UL,      /* MulticastPacketsReceived */
    50UL,       /* BroadcastPacketsSent */
    100UL,      /* BroadcastPacketsReceived */
    0UL         /* UnknownProtoPacketsReceived */
};

/* Devices learned by the switch while their port has link (fixture).
 * eth_port is the 0-based external port index, as in eth_device_t.
 */
typedef struct {
    UCHAR mac[6];
    INT eth_port;
    INT vlan_id;
    INT tx_rate;
    INT rx_rate;
} mock_device_t;

static const mock_device_t mock_devices[] = {
    { { 0x02, 0x00, 0x00, 0x00, 0x10, 0x01 }, 0, 1, 1000, 1000 },
    { { 0x02, 0x00, 0x00, 0x00, 0x10, 0x02 }, 1, 1,  100,  100 }
};

#define MOCK_DEVICE_COUNT (sizeof(mock_devices) / sizeof(mock_devices[0]))

/* Ethernet WAN state. The WAN port is a 0-based index. The header default
 * is used when this four-port model implements it, otherwise port 0.
 */
static BOOLEAN ethwan_enabled = FALSE;
static UINT ethwan_port =
    (ETHWAN_DEF_INTF_NUM >= 0 && ETHWAN_DEF_INTF_NUM < MOCK_ETH_PORTS)
        ? ETHWAN_DEF_INTF_NUM : 0;

/* Registered callbacks. The interface cannot withdraw a registration. */
static CcspHalExtSw_ethAssociatedDevice_callback assoc_callback;
static appCallBack ethwan_callbacks;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Record why a call failed (RETURN_ERR carries no cause) and return it. */
static INT mock_fail(const char *api, const char *reason)
{
    syslog(LOG_ERR, "ethsw_hal: %s: %s", api, reason);
    return RETURN_ERR;
}

/* Map a CCSP_HAL_ETHSW_PORT to an index into the mock tables, or -1 when the
 * value is not an implemented port (including CCSP_HAL_ETHSW_PortMax).
 */
static int port_index(CCSP_HAL_ETHSW_PORT id)
{
    int value = (int)id;
    if (value < (int)CCSP_HAL_ETHSW_EthPort1 ||
        value >= (int)CCSP_HAL_ETHSW_EthPort1 + MOCK_PORTS)
        return -1;
    return value - (int)CCSP_HAL_ETHSW_EthPort1;
}

/* A link exists when a peer is attached and the port is administratively up. */
static BOOLEAN link_is_up(int idx)
{
    return (BOOLEAN)(mock_peers[idx].connected &&
                     mock_ports[idx].admin == CCSP_HAL_ETHSW_AdminUp);
}

static CCSP_HAL_ETHSW_LINK_STATUS link_status(int idx)
{
    if (link_is_up(idx))
        return CCSP_HAL_ETHSW_LINK_Up;
    /* Down: no link although a peer is attached. Disconnected: no peer. */
    return mock_peers[idx].connected ? CCSP_HAL_ETHSW_LINK_Down
                                     : CCSP_HAL_ETHSW_LINK_Disconnected;
}

/* The Ethernet WAN link is up only when the feature is enabled and the
 * selected port has link.
 */
static INT ethwan_link(void)
{
    return (ethwan_enabled && ethwan_port < MOCK_PORTS &&
            link_is_up((int)ethwan_port)) ? 1 : 0;
}

static BOOLEAN device_active(size_t i)
{
    return link_is_up(mock_devices[i].eth_port);
}

static void fill_device(size_t i, BOOLEAN active, eth_device_t *out)
{
    memset(out, 0, sizeof(*out));
    memcpy(out->eth_devMacAddress, mock_devices[i].mac,
           sizeof(out->eth_devMacAddress));
    out->eth_port = mock_devices[i].eth_port;
    out->eth_vlanid = mock_devices[i].vlan_id;
    out->eth_devTxRate = mock_devices[i].tx_rate;
    out->eth_devRxRate = mock_devices[i].rx_rate;
    out->eth_Active = active;
}

/* Observable state that can raise a notification. Take a snapshot before a
 * state change and pass it to notify_changes() afterwards.
 */
typedef struct {
    BOOLEAN device_active[MOCK_DEVICE_COUNT];
    INT wan_link;
} mock_snapshot_t;

static void take_snapshot(mock_snapshot_t *snap)
{
    size_t i;
    for (i = 0; i < MOCK_DEVICE_COUNT; ++i)
        snap->device_active[i] = device_active(i);
    snap->wan_link = ethwan_link();
}

/* Deliver association/disassociation and Ethernet WAN link events for every
 * change since the snapshot. Callbacks run synchronously on the caller's
 * thread, after the state change is complete.
 */
static void notify_changes(const mock_snapshot_t *before)
{
    size_t i;
    INT wan = ethwan_link();

    for (i = 0; i < MOCK_DEVICE_COUNT; ++i) {
        BOOLEAN active = device_active(i);
        if (active != before->device_active[i] && assoc_callback != NULL) {
            eth_device_t dev;
            fill_device(i, active, &dev);
            (void)assoc_callback(&dev);
        }
    }

    if (wan != before->wan_link) {
        if (wan == 1 && ethwan_callbacks.pGWP_act_EthWanLinkUP != NULL)
            ethwan_callbacks.pGWP_act_EthWanLinkUP();
        else if (wan == 0 && ethwan_callbacks.pGWP_act_EthWanLinkDown != NULL)
            ethwan_callbacks.pGWP_act_EthWanLinkDown();
    }
}

/* ------------------------------------------------------------------ */
/* Lifecycle                                                           */
/* ------------------------------------------------------------------ */

/* PUBLIC_INTERFACE */
/**
 * @brief Initialize the Ethernet Switch HAL.
 *
 * The mock needs no hardware, so initialization is an idempotent no-op that
 * keeps the current state. Callers should not use it to reset the HAL.
 *
 * @return RETURN_OK always.
 */
INT CcspHalEthSwInit(void)
{
    return RETURN_OK;
}

/* ------------------------------------------------------------------ */
/* Port status and configuration                                       */
/* ------------------------------------------------------------------ */

/* PUBLIC_INTERFACE */
/**
 * @brief Read the live link rate, duplex mode and link state of a port.
 *
 * With a link up, Auto configuration reports the rate and duplex the peer
 * negotiated; an explicit configuration reports the configured value. Without
 * a link the rate is CCSP_HAL_ETHSW_LINK_NULL and the duplex is Auto.
 *
 * @param[in]  PortId       Port to read (1-based CCSP_HAL_ETHSW_PORT).
 * @param[out] pLinkRate    Receives the link rate.
 * @param[out] pDuplexMode  Receives the duplex mode.
 * @param[out] pStatus      Receives the link state.
 * @return RETURN_OK on success; RETURN_ERR for a NULL output or a port that
 *         is not implemented.
 */
INT CcspHalEthSwGetPortStatus(CCSP_HAL_ETHSW_PORT PortId,
                              PCCSP_HAL_ETHSW_LINK_RATE pLinkRate,
                              PCCSP_HAL_ETHSW_DUPLEX_MODE pDuplexMode,
                              PCCSP_HAL_ETHSW_LINK_STATUS pStatus)
{
    int idx = port_index(PortId);
    CCSP_HAL_ETHSW_LINK_RATE rate = CCSP_HAL_ETHSW_LINK_NULL;
    CCSP_HAL_ETHSW_DUPLEX_MODE duplex = CCSP_HAL_ETHSW_DUPLEX_Auto;

    if (pLinkRate == NULL || pDuplexMode == NULL || pStatus == NULL)
        return mock_fail(__func__, "NULL output pointer");
    if (idx < 0)
        return mock_fail(__func__, "port not implemented");

    if (link_is_up(idx)) {
        rate = (mock_ports[idx].cfg_rate == CCSP_HAL_ETHSW_LINK_Auto)
                   ? mock_peers[idx].peer_rate : mock_ports[idx].cfg_rate;
        duplex = (mock_ports[idx].cfg_duplex == CCSP_HAL_ETHSW_DUPLEX_Auto)
                     ? mock_peers[idx].peer_duplex : mock_ports[idx].cfg_duplex;
    }

    *pLinkRate = rate;
    *pDuplexMode = duplex;
    *pStatus = link_status(idx);
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/**
 * @brief Read the configured (not negotiated) link rate and duplex mode.
 *
 * @param[in]  PortId       Port to read (1-based CCSP_HAL_ETHSW_PORT).
 * @param[out] pLinkRate    Receives the configured link rate.
 * @param[out] pDuplexMode  Receives the configured duplex mode.
 * @return RETURN_OK on success; RETURN_ERR for a NULL output or a port that
 *         is not implemented.
 */
INT CcspHalEthSwGetPortCfg(CCSP_HAL_ETHSW_PORT PortId,
                           PCCSP_HAL_ETHSW_LINK_RATE pLinkRate,
                           PCCSP_HAL_ETHSW_DUPLEX_MODE pDuplexMode)
{
    int idx = port_index(PortId);

    if (pLinkRate == NULL || pDuplexMode == NULL)
        return mock_fail(__func__, "NULL output pointer");
    if (idx < 0)
        return mock_fail(__func__, "port not implemented");

    *pLinkRate = mock_ports[idx].cfg_rate;
    *pDuplexMode = mock_ports[idx].cfg_duplex;
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/**
 * @brief Configure the link rate and duplex mode of a port.
 *
 * @param[in] PortId      Port to configure (1-based CCSP_HAL_ETHSW_PORT).
 * @param[in] LinkRate    CCSP_HAL_ETHSW_LINK_NULL..CCSP_HAL_ETHSW_LINK_Auto.
 *                        The L1 profile treats the whole enum (including
 *                        LINK_NULL, i.e. 0) as accepted input.
 * @param[in] DuplexMode  CCSP_HAL_ETHSW_DUPLEX_Auto..CCSP_HAL_ETHSW_DUPLEX_Full.
 * @return RETURN_OK on success; RETURN_ERR for a port that is not implemented
 *         or an out-of-range value. State is unchanged on failure.
 */
INT CcspHalEthSwSetPortCfg(CCSP_HAL_ETHSW_PORT PortId,
                           CCSP_HAL_ETHSW_LINK_RATE LinkRate,
                           CCSP_HAL_ETHSW_DUPLEX_MODE DuplexMode)
{
    int idx = port_index(PortId);
    mock_snapshot_t before;

    if (idx < 0)
        return mock_fail(__func__, "port not implemented");
    if ((int)LinkRate < (int)CCSP_HAL_ETHSW_LINK_NULL ||
        (int)LinkRate > (int)CCSP_HAL_ETHSW_LINK_Auto)
        return mock_fail(__func__, "invalid link rate");
    if ((int)DuplexMode < (int)CCSP_HAL_ETHSW_DUPLEX_Auto ||
        (int)DuplexMode > (int)CCSP_HAL_ETHSW_DUPLEX_Full)
        return mock_fail(__func__, "invalid duplex mode");

    take_snapshot(&before);
    mock_ports[idx].cfg_rate = LinkRate;
    mock_ports[idx].cfg_duplex = DuplexMode;
    notify_changes(&before);
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/**
 * @brief Read the administrative state of a port.
 *
 * The administrative state is what was requested and is independent of the
 * link state reported by CcspHalEthSwGetPortStatus().
 *
 * @param[in]  PortId        Port to read (1-based CCSP_HAL_ETHSW_PORT).
 * @param[out] pAdminStatus  Receives the administrative state.
 * @return RETURN_OK on success; RETURN_ERR for a NULL output or a port that
 *         is not implemented.
 */
INT CcspHalEthSwGetPortAdminStatus(CCSP_HAL_ETHSW_PORT PortId,
                                   PCCSP_HAL_ETHSW_ADMIN_STATUS pAdminStatus)
{
    int idx = port_index(PortId);

    if (pAdminStatus == NULL)
        return mock_fail(__func__, "NULL output pointer");
    if (idx < 0)
        return mock_fail(__func__, "port not implemented");

    *pAdminStatus = mock_ports[idx].admin;
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/**
 * @brief Set the administrative state of a port.
 *
 * Anything other than CCSP_HAL_ETHSW_AdminUp stops the port from
 * establishing a link. CCSP_HAL_ETHSW_AdminTest is vendor-defined; the mock
 * treats it as not forwarding. Association and Ethernet WAN link callbacks
 * fire when the change alters device presence or the WAN link.
 *
 * @param[in] PortId       Port to change (1-based CCSP_HAL_ETHSW_PORT).
 * @param[in] AdminStatus  CCSP_HAL_ETHSW_AdminUp, AdminDown or AdminTest.
 * @return RETURN_OK on success; RETURN_ERR for a port that is not implemented
 *         or an out-of-range value. State is unchanged on failure.
 */
INT CcspHalEthSwSetPortAdminStatus(CCSP_HAL_ETHSW_PORT PortId,
                                   CCSP_HAL_ETHSW_ADMIN_STATUS AdminStatus)
{
    int idx = port_index(PortId);
    mock_snapshot_t before;

    if (idx < 0)
        return mock_fail(__func__, "port not implemented");
    if ((int)AdminStatus < (int)CCSP_HAL_ETHSW_AdminUp ||
        (int)AdminStatus > (int)CCSP_HAL_ETHSW_AdminTest)
        return mock_fail(__func__, "invalid admin status");

    take_snapshot(&before);
    mock_ports[idx].admin = AdminStatus;
    notify_changes(&before);
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/**
 * @brief Set how quickly a port ages learned MAC addresses.
 *
 * The unit is vendor-defined and the interface has no getter. The mock
 * accepts any non-negative value.
 *
 * @param[in] PortId      Port to change (1-based CCSP_HAL_ETHSW_PORT).
 * @param[in] AgingSpeed  Aging speed, zero or greater.
 * @return RETURN_OK on success; RETURN_ERR for a port that is not implemented
 *         or a negative value.
 */
INT CcspHalEthSwSetAgingSpeed(CCSP_HAL_ETHSW_PORT PortId, INT AgingSpeed)
{
    int idx = port_index(PortId);

    if (idx < 0)
        return mock_fail(__func__, "port not implemented");
    if (AgingSpeed < 0)
        return mock_fail(__func__, "invalid aging speed");

    mock_ports[idx].aging_speed = AgingSpeed;
    return RETURN_OK;
}

/* ------------------------------------------------------------------ */
/* MAC address and associated device discovery                         */
/* ------------------------------------------------------------------ */

/* PUBLIC_INTERFACE */
/**
 * @brief Find the port a learned MAC address is attached to.
 *
 * Only devices on a port with link are learned. The reported number follows
 * the header: 0 is the MoCA port and 1-4 are Ethernet ports (the mock has no
 * MoCA device, so it only reports 1-4).
 *
 * @param[in]  mac   Pointer to the 6 binary octets of the address.
 * @param[out] port  Receives the port number.
 * @return RETURN_OK when found; RETURN_ERR for a NULL argument or an address
 *         that is not learned. @p port is unchanged on failure.
 */
INT CcspHalEthSwLocatePortByMacAddress(unsigned char *mac, INT *port)
{
    size_t i;

    if (mac == NULL || port == NULL)
        return mock_fail(__func__, "NULL argument");

    for (i = 0; i < MOCK_DEVICE_COUNT; ++i) {
        if (memcmp(mac, mock_devices[i].mac, sizeof(mock_devices[i].mac)) == 0 &&
            device_active(i)) {
            *port = mock_devices[i].eth_port + 1;
            return RETURN_OK;
        }
    }
    return mock_fail(__func__, "MAC address not learned");
}

/* PUBLIC_INTERFACE */
/**
 * @brief Snapshot the devices currently associated with the Ethernet ports.
 *
 * The array is allocated with calloc() and ownership passes to the caller,
 * who releases it with free(). The array is NULL with a count of 0 when no
 * device is present. On RETURN_ERR no array is handed over.
 *
 * @param[out] output_array_size  Receives the number of elements.
 * @param[out] output_struct      Receives the allocated eth_device_t array.
 * @return RETURN_OK on success; RETURN_ERR for a NULL argument or when the
 *         allocation fails.
 */
INT CcspHalExtSw_getAssociatedDevice(ULONG *output_array_size,
                                     eth_device_t **output_struct)
{
    size_t i, count = 0, next = 0;
    eth_device_t *devices = NULL;

    if (output_array_size == NULL || output_struct == NULL)
        return mock_fail(__func__, "NULL output pointer");

    for (i = 0; i < MOCK_DEVICE_COUNT; ++i)
        if (device_active(i))
            ++count;

    if (count > 0) {
        devices = calloc(count, sizeof(*devices));
        if (devices == NULL)
            return mock_fail(__func__, "out of memory");
        for (i = 0; i < MOCK_DEVICE_COUNT; ++i)
            if (device_active(i))
                fill_device(i, TRUE, &devices[next++]);
    }

    *output_array_size = (ULONG)count;
    *output_struct = devices;
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/**
 * @brief Register the device association/disassociation callback.
 *
 * Replaces any earlier registration; the interface has no way to withdraw
 * one. The callback is invoked synchronously, on the thread that made the
 * call that changed device presence, with eth_Active telling an arrival from
 * a departure. The structure passed to it is valid only during the call.
 *
 * @param[in] callback_proc  Function to call for each event.
 */
void CcspHalExtSw_ethAssociatedDevice_callback_register(
    CcspHalExtSw_ethAssociatedDevice_callback callback_proc)
{
    assoc_callback = callback_proc;
}

/* ------------------------------------------------------------------ */
/* Ethernet WAN selection and events                                   */
/* ------------------------------------------------------------------ */

#if defined(FEATURE_RDKB_WAN_MANAGER) && defined(FEATURE_RDKB_AUTO_PORT_SWITCH)
/* Map "eth0".."eth3" to a 0-based port index, or -1 for any other name. */
static int ifname_to_index(const char *ifname)
{
    if (ifname == NULL || strncmp(ifname, "eth", 3) != 0 ||
        ifname[3] < '0' || ifname[3] >= '0' + MOCK_ETH_PORTS ||
        ifname[4] != '\0')
        return -1;
    return ifname[3] - '0';
}

/* PUBLIC_INTERFACE */
/**
 * @brief Put one named Ethernet interface into or out of WAN mode.
 *
 * Enabling selects that interface as the Ethernet WAN port and enables
 * Ethernet WAN. Disabling turns Ethernet WAN off only when that interface is
 * the selected WAN port; otherwise nothing changes.
 *
 * @param[in] ifname   Interface name, "eth0".."eth3".
 * @param[in] WanMode  TRUE for WAN mode, FALSE to leave it.
 * @return RETURN_OK on success; RETURN_ERR for an unknown interface name or a
 *         WanMode other than TRUE/FALSE. State is unchanged on failure.
 */
int CcspHalExtSw_ethPortConfigure(char *ifname, BOOLEAN WanMode)
{
    int idx = ifname_to_index(ifname);
    mock_snapshot_t before;

    if (idx < 0)
        return mock_fail(__func__, "unknown interface name");
    if (WanMode != TRUE && WanMode != FALSE)
        return mock_fail(__func__, "invalid WanMode");

    take_snapshot(&before);
    if (WanMode == TRUE) {
        ethwan_port = (UINT)idx;
        ethwan_enabled = TRUE;
    } else if (ethwan_port == (UINT)idx) {
        ethwan_enabled = FALSE;
    }
    notify_changes(&before);
    return RETURN_OK;
}
#endif /* FEATURE_RDKB_WAN_MANAGER && FEATURE_RDKB_AUTO_PORT_SWITCH */

/* PUBLIC_INTERFACE */
/**
 * @brief Read whether the Ethernet WAN feature is enabled.
 *
 * @param[out] pFlag  Receives TRUE when enabled, FALSE when disabled.
 * @return RETURN_OK on success; RETURN_ERR when pFlag is NULL.
 */
INT CcspHalExtSw_getEthWanEnable(BOOLEAN *pFlag)
{
    if (pFlag == NULL)
        return mock_fail(__func__, "NULL output pointer");

    *pFlag = ethwan_enabled;
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/**
 * @brief Enable or disable Ethernet WAN on the currently selected port.
 *
 * Select the port first with CcspHalExtSw_setEthWanPort(). The Ethernet WAN
 * link callbacks fire when this changes the WAN link state.
 *
 * @param[in] Flag  TRUE to enable, FALSE to disable.
 * @return RETURN_OK on success; RETURN_ERR for a value other than TRUE/FALSE.
 */
INT CcspHalExtSw_setEthWanEnable(BOOLEAN Flag)
{
    mock_snapshot_t before;

    if (Flag != TRUE && Flag != FALSE)
        return mock_fail(__func__, "invalid flag");

    take_snapshot(&before);
    ethwan_enabled = Flag;
    notify_changes(&before);
    return RETURN_OK;
}

#ifdef FEATURE_RDKB_AUTO_PORT_SWITCH
/* PUBLIC_INTERFACE */
/**
 * @brief Report whether the hardware is configured for WAN or LAN use.
 *
 * There is no error channel. The hardware configuration is a fixed platform
 * property of the mock (LAN) and is independent of the Ethernet WAN enable
 * flag, the WAN link state and the selected WAN port.
 *
 * @return TRUE when the WAN configuration is active, FALSE for LAN.
 */
BOOLEAN CcspHalExtSw_getCurrentWanHWConf(void)
{
    static const BOOLEAN hw_wan_conf = FALSE;

    return hw_wan_conf;
}
#endif /* FEATURE_RDKB_AUTO_PORT_SWITCH */

/* PUBLIC_INTERFACE */
/**
 * @brief Read the Ethernet WAN port index.
 *
 * @param[out] pPort  Receives the 0-based port index (0..19 in this mock).
 * @return RETURN_OK on success; RETURN_ERR when pPort is NULL.
 */
INT CcspHalExtSw_getEthWanPort(UINT *pPort)
{
    if (pPort == NULL)
        return mock_fail(__func__, "NULL output pointer");

    *pPort = ethwan_port;
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/**
 * @brief Select the Ethernet WAN port by 0-based index.
 *
 * This is a different numbering from the 1-based CCSP_HAL_ETHSW_PORT enum.
 *
 * @param[in] Port  Port index, 0..19 in this mock.
 * @return RETURN_OK on success; RETURN_ERR for an index that is not
 *         implemented. State is unchanged on failure.
 */
INT CcspHalExtSw_setEthWanPort(UINT Port)
{
    mock_snapshot_t before;

    if (Port >= MOCK_WAN_PORTS)
        return mock_fail(__func__, "port index out of range");

    take_snapshot(&before);
    ethwan_port = Port;
    notify_changes(&before);
    return RETURN_OK;
}

/* PUBLIC_INTERFACE */
/**
 * @brief Register the Ethernet WAN link up/down callbacks.
 *
 * The pair is copied, so the caller's appCallBack need not outlive the call.
 * A NULL @p obj is ignored. Either member may be NULL to skip that event.
 * Callbacks take no argument; query GWP_GetEthWanLinkStatus() for state.
 *
 * @param[in] obj  Callbacks to install, replacing any earlier registration.
 */
void GWP_RegisterEthWan_Callback(appCallBack *obj)
{
    if (obj == NULL)
        return;

    ethwan_callbacks = *obj;
}

/* PUBLIC_INTERFACE */
/**
 * @brief Report the live Ethernet WAN link state.
 *
 * @return 1 when Ethernet WAN is enabled and the selected port has link,
 *         otherwise 0. The mock never fails, so it never returns a negative
 *         value.
 */
INT GWP_GetEthWanLinkStatus(void)
{
    return ethwan_link();
}

/* PUBLIC_INTERFACE */
/**
 * @brief Read the name of the Ethernet WAN interface.
 *
 * The name is "eth<index>" for the selected WAN port and is written
 * NUL-terminated.
 *
 * @param[out] Interface  Caller buffer; the header asks for at least 64 bytes.
 * @param[in]  maxSize    Buffer size, 11 to 262 bytes per the header.
 * @return RETURN_OK on success; RETURN_ERR for a NULL buffer or a maxSize
 *         outside 11..262. The buffer is untouched on failure.
 */
INT GWP_GetEthWanInterfaceName(unsigned char *Interface, ULONG maxSize)
{
    char name[16];
    int length;

    if (Interface == NULL)
        return mock_fail(__func__, "NULL output buffer");
    if (maxSize < 11UL || maxSize > 262UL)
        return mock_fail(__func__, "maxSize outside 11..262");

    length = snprintf(name, sizeof(name), "eth%u", ethwan_port);
    if (length < 0 || (ULONG)length >= maxSize)
        return mock_fail(__func__, "buffer too small");

    memcpy(Interface, name, (size_t)length + 1);
    return RETURN_OK;
}

/* ------------------------------------------------------------------ */
/* Statistics                                                          */
/* ------------------------------------------------------------------ */

/* PUBLIC_INTERFACE */
/**
 * @brief Read the traffic counters of a port.
 *
 * The mock serves a fixed, internally consistent set of counters for every
 * implemented port; they are fixtures and do not advance.
 *
 * @param[in]  PortId  Port to read (1-based CCSP_HAL_ETHSW_PORT).
 * @param[out] pStats  Receives the counters.
 * @return RETURN_OK on success; RETURN_ERR for a NULL output or a port that
 *         is not implemented.
 */
INT CcspHalEthSwGetEthPortStats(CCSP_HAL_ETHSW_PORT PortId,
                                PCCSP_HAL_ETH_STATS pStats)
{
    if (pStats == NULL)
        return mock_fail(__func__, "NULL output pointer");
    if (port_index(PortId) < 0)
        return mock_fail(__func__, "port not implemented");

    *pStats = mock_stats;
    return RETURN_OK;
}
