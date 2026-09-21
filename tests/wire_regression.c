/* Exercise production STP construction, VLAN insertion, validation and decode.
 * Only the interface lookup and sendto system call are replaced by test doubles.
 * Golden bytes are specified independently of the C structure layout.
 * All device-like values are synthetic: locally administered MAC
 * 02:00:00:00:00:01, VLANs 123/456/4094, and port identifier 17. */
#include "stp_inc.h"
#include <stddef.h>

STPD_CONTEXT stpd_context;
DEBUG_GLOBAL debugGlobal;
static INTERFACE_NODE test_intf = { .kif_index = 7, .port_id = 0 };
static STPD_INTF_STATS stats;
static STPD_INTF_STATS *stats_ptrs[] = { &stats };
static STP_CLASS test_class;
static STP_PORT_CLASS test_port;
static unsigned int bitmap_word;
static BITMAP_T untag_mask = { .nbits = 1, .size = 1, .arr = &bitmap_word };
static unsigned char sent[STP_MAX_PKT_LEN];
static size_t sent_len;
static unsigned int sends;

static const unsigned char golden_pvst123[] = {
    0x01,0x00,0x0c,0xcc,0xcc,0xcd, 0x02,0x00,0x00,0x00,0x00,0x01,
    0x81,0x00,0xe0,0x7b, 0x00,0x32,
    0xaa,0xaa,0x03,0x00,0x00,0x0c,0x01,0x0b,
    0x00,0x00,0x00,0x00,0x81,
    0x80,0x7b,0x02,0x00,0x00,0x00,0x00,0x01,
    0x01,0x02,0x03,0x04,
    0x80,0x7b,0x02,0x00,0x00,0x00,0x00,0x01,
    0x80,0x11,0x01,0x00,0x14,0x00,0x02,0x00,0x0f,0x00,
    0x00,0x00,0x00,0x00,0x02,0x00,0x7b
};

INTERFACE_NODE *stp_intf_get_node(uint32_t port_id)
{
    assert(port_id == 0);
    return &test_intf;
}

bool stp_intf_get_mac(int port_id, MAC_ADDRESS *mac)
{
    const unsigned char address[] = {0x02,0x00,0x00,0x00,0x00,0x01};
    assert(port_id == 0);
    memcpy(mac, address, sizeof(address));
    return true;
}

ssize_t __wrap_sendto(int fd, const void *buf, size_t len, int flags,
                     const struct sockaddr *addr, socklen_t addrlen)
{
    (void)fd; (void)flags;
    assert(addrlen == sizeof(struct sockaddr_ll));
    assert(((const struct sockaddr_ll *)addr)->sll_ifindex == 7);
    assert(len <= sizeof(sent));
    memcpy(sent, buf, len);
    sent_len = len;
    sends++;
    return (ssize_t)len;
}

static void init_case(uint16_t vlan, bool untagged)
{
    memset(&stp_global, 0, sizeof(stp_global));
    memset(&test_class, 0, sizeof(test_class));
    memset(&test_port, 0, sizeof(test_port));
    stpd_context.dbg_stats.intf = stats_ptrs;
    g_stp_instances = 1;
    g_stp_class_array = &test_class;
    g_stp_port_array = &test_port;
    test_class.vlan_id = vlan;
    test_class.state = STP_CLASS_ACTIVE;
    test_class.untag_mask = &untag_mask;
    bitmap_word = untagged ? 1 : 0;
    stpdata_init_bpdu_structures();
    STP_CONFIG_BPDU *bpdu = &g_stp_config_bpdu;
    bpdu->flags.topology_change = 1;
    bpdu->flags.topology_change_acknowledgement = 1;
    bpdu->root_id.priority = 8;
    bpdu->root_id.system_id = vlan;
    bpdu->root_id.address._ulong = 0x02000000;
    bpdu->root_id.address._ushort = 0x0001;
    bpdu->bridge_id = bpdu->root_id;
    bpdu->root_path_cost = 0x01020304;
    bpdu->port_id.priority = 8;
    bpdu->port_id.number = 17;
    bpdu->message_age = 1 << 8;
    bpdu->max_age = 20 << 8;
    bpdu->hello_time = 2 << 8;
    bpdu->forward_delay = 15 << 8;
    sent_len = 0;
    sends = 0;
}

static void expect_packet(const unsigned char *golden, size_t len)
{
    assert(sends == 1);
    assert(sent_len == len);
    for (size_t i = 0; i < len; ++i) {
        if (sent[i] != golden[i]) {
            fprintf(stderr, "byte %zu: got %02x expected %02x\n", i, sent[i], golden[i]);
            abort();
        }
    }
}

static void pcap_record(FILE *pcap)
{
    /* Linux/amd64 little-endian PCAP with Ethernet link type. */
    uint32_t record[] = {0, 0, (uint32_t)sent_len, (uint32_t)sent_len};
    assert(fwrite(record, sizeof(record), 1, pcap) == 1);
    assert(fwrite(sent, sent_len, 1, pcap) == 1);
}

static void test_pvst(FILE *pcap)
{
    const uint16_t vlans[] = {123, 456, 4094};
    for (size_t v = 0; v < sizeof(vlans)/sizeof(vlans[0]); ++v) {
        uint16_t vlan = vlans[v];
        unsigned char golden[68];
        memcpy(golden, golden_pvst123, sizeof(golden));
        golden[14] = 0xe0 | (vlan >> 8);
        golden[15] = vlan & 255;
        golden[31] = golden[43] = 0x80 | (vlan >> 8);
        golden[32] = golden[44] = vlan & 255;
        golden[66] = vlan >> 8;
        golden[67] = vlan & 255;
        init_case(vlan, false);
        stputil_send_pvst_bpdu(&test_class, 0, CONFIG_BPDU_TYPE);
        expect_packet(golden, sizeof(golden));
        pcap_record(pcap);

        /* Strip the VLAN as Linux AUXDATA delivery does, then use real RX code.
         * Move to deliberately odd storage to catch invalid aligned accesses. */
        unsigned char rx_storage[65];
        unsigned char *rx = rx_storage + 1;
        memcpy(rx, golden, 12);
        memcpy(rx + 12, golden + 16, 52);
        assert(stputil_validate_pvst_bpdu((PVST_CONFIG_BPDU *)rx));
        PVST_CONFIG_BPDU *pvst = (PVST_CONFIG_BPDU *)rx;
        assert(pvst->vlan_id == vlan && pvst->tag_length == 2);
        STP_CONFIG_BPDU ieee = {0};
        memcpy((unsigned char *)&ieee + STP_BPDU_OFFSET,
               rx + PVST_BPDU_OFFSET, STP_SIZEOF_CONFIG_BPDU);
        stputil_decode_bpdu(&ieee);
        assert(ieee.root_id.system_id == vlan && ieee.root_id.priority == 8);
        assert(ieee.root_id.address._ulong == 0x02000000);
        assert(ieee.root_id.address._ushort == 0x0001);
        assert(ieee.root_path_cost == 0x01020304);
        assert(ieee.port_id.priority == 8 && ieee.port_id.number == 17);
        assert(ieee.message_age == 1 && ieee.max_age == 20);
        assert(ieee.hello_time == 2 && ieee.forward_delay == 15);

        init_case(vlan, true);
        stputil_send_pvst_bpdu(&test_class, 0, CONFIG_BPDU_TYPE);
        unsigned char untagged[64];
        memcpy(untagged, golden, 12);
        memcpy(untagged + 12, golden + 16, 52);
        expect_packet(untagged, sizeof(untagged));
        pcap_record(pcap);
        printf("PASS PVST VLAN %u tagged/untagged bytes and RX decode\n", vlan);
    }
}

static void test_ieee_and_tcn(FILE *pcap)
{
    unsigned char ieee[52] = {
        0x01,0x80,0xc2,0x00,0x00,0x00, 0x02,0x00,0x00,0x00,0x00,0x01,
        0x00,0x26, 0x42,0x42,0x03
    };
    memcpy(ieee + 17, golden_pvst123 + 26, 35);
    init_case(123, true);
    /* The IEEE sender receives the BPDU already encoded by the PVST path. */
    stputil_encode_bpdu(&g_stp_config_bpdu);
    stputil_send_bpdu(&test_class, 0, CONFIG_BPDU_TYPE);
    expect_packet(ieee, sizeof(ieee));
    assert(stputil_validate_bpdu((STP_CONFIG_BPDU *)sent));
    pcap_record(pcap);

    unsigned char tcn[24] = {
        0x01,0x80,0xc2,0x00,0x00,0x00, 0x02,0x00,0x00,0x00,0x00,0x01,
        0x00,0x07, 0x42,0x42,0x03, 0x00,0x00,0x00,0x80,0x00,0x00,0x00
    };
    init_case(123, true);
    stputil_send_bpdu(&test_class, 0, TCN_BPDU_TYPE);
    expect_packet(tcn, sizeof(tcn));
    assert(stputil_validate_bpdu((STP_CONFIG_BPDU *)sent));
    pcap_record(pcap);

    unsigned char pvst_tcn[68] = {0};
    memcpy(pvst_tcn, golden_pvst123, 26);
    pvst_tcn[29] = 0x80;
    init_case(123, false);
    stputil_send_pvst_bpdu(&test_class, 0, TCN_BPDU_TYPE);
    expect_packet(pvst_tcn, sizeof(pvst_tcn));
    pcap_record(pcap);
    puts("PASS IEEE configuration, IEEE TCN and tagged PVST TCN bytes");
}

static void test_alignment_and_bounds(void)
{
    for (unsigned int offset = 0; offset < 8; ++offset) {
        unsigned char bytes[32];
        memset(bytes, 0x5a, sizeof(bytes));
        MAC_ADDRESS original = {0x02000000, 0x0001};
        memcpy(bytes + offset, &original, 6);
        HOST_TO_NET_MAC(bytes + offset, bytes + offset);
        assert(memcmp(bytes + offset, golden_pvst123 + 6, 6) == 0);
        NET_TO_HOST_MAC(bytes + offset, bytes + offset);
        assert(memcmp(bytes + offset, &original, 6) == 0);
        assert(bytes[offset + 6] == 0x5a);
        stp_store_u16(bytes + offset, 0x8011);
        assert(stp_load_u16(bytes + offset) == 0x8011);
        assert(stputil_compare_port_id((PORT_IDENTIFIER *)(bytes + offset),
                                      (PORT_IDENTIFIER *)(bytes + offset)) == EQUAL_TO);
    }
    init_case(123, false);
    char packet[STP_MAX_PKT_LEN] = {0};
    uint64_t errors = stats.pkt_tx_err;
    assert(stp_pkt_tx_handler(0, 123, packet, 13, true) == -1);
    assert(stp_pkt_tx_handler(0, 123, packet, STP_MAX_PKT_LEN - 3, true) == -1);
    assert(stp_pkt_tx_handler(0, 123, packet, UINT16_MAX, false) == -1);
    assert(stp_pkt_tx_handler(0, 123, NULL, 64, true) == -1);
    assert(sends == 0 && stats.pkt_tx_err == errors + 4);
    assert(stp_pkt_tx_handler(0, 123, packet, STP_MAX_PKT_LEN - 4, true) == STP_MAX_PKT_LEN);
    assert(stp_pkt_tx_handler(0, 123, packet, STP_MAX_PKT_LEN, false) == STP_MAX_PKT_LEN);
    puts("PASS odd-address conversion and transmit size guards");
}

int main(void)
{
    setbuf(stdout, NULL);
    printf("sizeof(MAC_ADDRESS)=%zu MAC_HEADER=%zu STP_CONFIG_BPDU=%zu PVST_CONFIG_BPDU=%zu\n",
           sizeof(MAC_ADDRESS), sizeof(MAC_HEADER), sizeof(STP_CONFIG_BPDU), sizeof(PVST_CONFIG_BPDU));
    FILE *pcap = fopen("wire-regression.pcap", "wb");
    assert(pcap);
    const uint32_t header[] = {0xa1b2c3d4, 0x00040002, 0, 0, 65535, 1};
    assert(fwrite(header, sizeof(header), 1, pcap) == 1);
    test_pvst(pcap);
    test_ieee_and_tcn(pcap);
    test_alignment_and_bounds();
    assert(fclose(pcap) == 0);
    puts("ALL WIRE REGRESSION TESTS PASSED");
    return 0;
}
