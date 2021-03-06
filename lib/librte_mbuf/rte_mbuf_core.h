/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright 2014 6WIND S.A.
 */

#ifndef _RTE_MBUF_CORE_H_
#define _RTE_MBUF_CORE_H_

/**
 * @file
 * This file contains definion of RTE mbuf structure itself,
 * packet offload flags and some related macros.
 * For majority of DPDK entities, it is not recommended to include
 * this file directly, use include <rte_mbuf.h> instead.
 *
 * New fields and flags should fit in the "dynamic space".
 */

#include <stdint.h>
#include <rte_compat.h>
#include <generic/rte_atomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Packet Offload Features Flags. It also carry packet type information.
 * Critical resources. Both rx/tx shared these bits. Be cautious on any change
 *
 * - RX flags start at bit position zero, and get added to the left of previous
 *   flags.
 * - The most-significant 3 bits are reserved for generic mbuf flags
 * - TX flags therefore start at bit position 60 (i.e. 63-3), and new flags get
 *   added to the right of the previously defined flags i.e. they should count
 *   downwards, not upwards.
 *
 * Keep these flags synchronized with rte_get_rx_ol_flag_name() and
 * rte_get_tx_ol_flag_name().
 */

/**
 * The RX packet is a 802.1q VLAN packet, and the tci has been
 * saved in in mbuf->vlan_tci.
 * If the flag PKT_RX_VLAN_STRIPPED is also present, the VLAN
 * header has been stripped from mbuf data, else it is still
 * present.
 */
#define PKT_RX_VLAN          (1ULL << 0)

/** RX packet with RSS hash result. */
#define PKT_RX_RSS_HASH      (1ULL << 1)

 /** RX packet with FDIR match indicate. */
#define PKT_RX_FDIR          (1ULL << 2)

/**
 * Deprecated.
 * Checking this flag alone is deprecated: check the 2 bits of
 * PKT_RX_L4_CKSUM_MASK.
 * This flag was set when the L4 checksum of a packet was detected as
 * wrong by the hardware.
 */
#define PKT_RX_L4_CKSUM_BAD  (1ULL << 3)

/**
 * Deprecated.
 * Checking this flag alone is deprecated: check the 2 bits of
 * PKT_RX_IP_CKSUM_MASK.
 * This flag was set when the IP checksum of a packet was detected as
 * wrong by the hardware.
 */
#define PKT_RX_IP_CKSUM_BAD  (1ULL << 4)

 /** External IP header checksum error. */
#define PKT_RX_EIP_CKSUM_BAD (1ULL << 5)

/**
 * A vlan has been stripped by the hardware and its tci is saved in
 * mbuf->vlan_tci. This can only happen if vlan stripping is enabled
 * in the RX configuration of the PMD.
 * When PKT_RX_VLAN_STRIPPED is set, PKT_RX_VLAN must also be set.
 */
#define PKT_RX_VLAN_STRIPPED (1ULL << 6)

/**
 * Mask of bits used to determine the status of RX IP checksum.
 * - PKT_RX_IP_CKSUM_UNKNOWN: no information about the RX IP checksum
 * - PKT_RX_IP_CKSUM_BAD: the IP checksum in the packet is wrong
 * - PKT_RX_IP_CKSUM_GOOD: the IP checksum in the packet is valid
 * - PKT_RX_IP_CKSUM_NONE: the IP checksum is not correct in the packet
 *   data, but the integrity of the IP header is verified.
 */
#define PKT_RX_IP_CKSUM_MASK ((1ULL << 4) | (1ULL << 7))

#define PKT_RX_IP_CKSUM_UNKNOWN 0
#define PKT_RX_IP_CKSUM_BAD     (1ULL << 4)
#define PKT_RX_IP_CKSUM_GOOD    (1ULL << 7)
#define PKT_RX_IP_CKSUM_NONE    ((1ULL << 4) | (1ULL << 7))

/**
 * Mask of bits used to determine the status of RX L4 checksum.
 * - PKT_RX_L4_CKSUM_UNKNOWN: no information about the RX L4 checksum
 * - PKT_RX_L4_CKSUM_BAD: the L4 checksum in the packet is wrong
 * - PKT_RX_L4_CKSUM_GOOD: the L4 checksum in the packet is valid
 * - PKT_RX_L4_CKSUM_NONE: the L4 checksum is not correct in the packet
 *   data, but the integrity of the L4 data is verified.
 */
#define PKT_RX_L4_CKSUM_MASK ((1ULL << 3) | (1ULL << 8))

#define PKT_RX_L4_CKSUM_UNKNOWN 0
#define PKT_RX_L4_CKSUM_BAD     (1ULL << 3)
#define PKT_RX_L4_CKSUM_GOOD    (1ULL << 8)
#define PKT_RX_L4_CKSUM_NONE    ((1ULL << 3) | (1ULL << 8))

/** RX IEEE1588 L2 Ethernet PT Packet. */
#define PKT_RX_IEEE1588_PTP  (1ULL << 9)

/** RX IEEE1588 L2/L4 timestamped packet.*/
#define PKT_RX_IEEE1588_TMST (1ULL << 10)

/** FD id reported if FDIR match. */
#define PKT_RX_FDIR_ID       (1ULL << 13)

/** Flexible bytes reported if FDIR match. */
#define PKT_RX_FDIR_FLX      (1ULL << 14)

/**
 * The 2 vlans have been stripped by the hardware and their tci are
 * saved in mbuf->vlan_tci (inner) and mbuf->vlan_tci_outer (outer).
 * This can only happen if vlan stripping is enabled in the RX
 * configuration of the PMD.
 * When PKT_RX_QINQ_STRIPPED is set, the flags (PKT_RX_VLAN |
 * PKT_RX_VLAN_STRIPPED | PKT_RX_QINQ) must also be set.
 */
#define PKT_RX_QINQ_STRIPPED (1ULL << 15)

/**
 * When packets are coalesced by a hardware or virtual driver, this flag
 * can be set in the RX mbuf, meaning that the m->tso_segsz field is
 * valid and is set to the segment size of original packets.
 */
#define PKT_RX_LRO           (1ULL << 16)

/**
 * Indicate that the timestamp field in the mbuf is valid.
 */
#define PKT_RX_TIMESTAMP     (1ULL << 17)

/**
 * Indicate that security offload processing was applied on the RX packet.
 */
#define PKT_RX_SEC_OFFLOAD	(1ULL << 18)

/**
 * Indicate that security offload processing failed on the RX packet.
 */
#define PKT_RX_SEC_OFFLOAD_FAILED	(1ULL << 19)

/**
 * The RX packet is a double VLAN, and the outer tci has been
 * saved in in mbuf->vlan_tci_outer. If PKT_RX_QINQ set, PKT_RX_VLAN
 * also should be set and inner tci should be saved to mbuf->vlan_tci.
 * If the flag PKT_RX_QINQ_STRIPPED is also present, both VLANs
 * headers have been stripped from mbuf data, else they are still
 * present.
 */
#define PKT_RX_QINQ          (1ULL << 20)

/**
 * Mask of bits used to determine the status of outer RX L4 checksum.
 * - PKT_RX_OUTER_L4_CKSUM_UNKNOWN: no info about the outer RX L4 checksum
 * - PKT_RX_OUTER_L4_CKSUM_BAD: the outer L4 checksum in the packet is wrong
 * - PKT_RX_OUTER_L4_CKSUM_GOOD: the outer L4 checksum in the packet is valid
 * - PKT_RX_OUTER_L4_CKSUM_INVALID: invalid outer L4 checksum state.
 *
 * The detection of PKT_RX_OUTER_L4_CKSUM_GOOD shall be based on the given
 * HW capability, At minimum, the PMD should support
 * PKT_RX_OUTER_L4_CKSUM_UNKNOWN and PKT_RX_OUTER_L4_CKSUM_BAD states
 * if the DEV_RX_OFFLOAD_OUTER_UDP_CKSUM offload is available.
 */
#define PKT_RX_OUTER_L4_CKSUM_MASK	((1ULL << 21) | (1ULL << 22))

#define PKT_RX_OUTER_L4_CKSUM_UNKNOWN	0
#define PKT_RX_OUTER_L4_CKSUM_BAD	(1ULL << 21)
#define PKT_RX_OUTER_L4_CKSUM_GOOD	(1ULL << 22)
#define PKT_RX_OUTER_L4_CKSUM_INVALID	((1ULL << 21) | (1ULL << 22))

/* add new RX flags here, don't forget to update PKT_FIRST_FREE */

#define PKT_FIRST_FREE (1ULL << 23)
#define PKT_LAST_FREE (1ULL << 40)

/* add new TX flags here, don't forget to update PKT_LAST_FREE  */

/**
 * Outer UDP checksum offload flag. This flag is used for enabling
 * outer UDP checksum in PMD. To use outer UDP checksum, the user needs to
 * 1) Enable the following in mbuf,
 * a) Fill outer_l2_len and outer_l3_len in mbuf.
 * b) Set the PKT_TX_OUTER_UDP_CKSUM flag.
 * c) Set the PKT_TX_OUTER_IPV4 or PKT_TX_OUTER_IPV6 flag.
 * 2) Configure DEV_TX_OFFLOAD_OUTER_UDP_CKSUM offload flag.
 */
#define PKT_TX_OUTER_UDP_CKSUM     (1ULL << 41)

/**
 * UDP Fragmentation Offload flag. This flag is used for enabling UDP
 * fragmentation in SW or in HW. When use UFO, mbuf->tso_segsz is used
 * to store the MSS of UDP fragments.
 */
#define PKT_TX_UDP_SEG	(1ULL << 42)

/**
 * Request security offload processing on the TX packet.
 */
#define PKT_TX_SEC_OFFLOAD	(1ULL << 43)

/**
 * Offload the MACsec. This flag must be set by the application to enable
 * this offload feature for a packet to be transmitted.
 */
#define PKT_TX_MACSEC        (1ULL << 44)

/**
 * Bits 45:48 used for the tunnel type.
 * The tunnel type must be specified for TSO or checksum on the inner part
 * of tunnel packets.
 * These flags can be used with PKT_TX_TCP_SEG for TSO, or PKT_TX_xxx_CKSUM.
 * The mbuf fields for inner and outer header lengths are required:
 * outer_l2_len, outer_l3_len, l2_len, l3_len, l4_len and tso_segsz for TSO.
 */
#define PKT_TX_TUNNEL_VXLAN   (0x1ULL << 45)
#define PKT_TX_TUNNEL_GRE     (0x2ULL << 45)
#define PKT_TX_TUNNEL_IPIP    (0x3ULL << 45)
#define PKT_TX_TUNNEL_GENEVE  (0x4ULL << 45)
/** TX packet with MPLS-in-UDP RFC 7510 header. */
#define PKT_TX_TUNNEL_MPLSINUDP (0x5ULL << 45)
#define PKT_TX_TUNNEL_VXLAN_GPE (0x6ULL << 45)
#define PKT_TX_TUNNEL_GTP       (0x7ULL << 45)
/**
 * Generic IP encapsulated tunnel type, used for TSO and checksum offload.
 * It can be used for tunnels which are not standards or listed above.
 * It is preferred to use specific tunnel flags like PKT_TX_TUNNEL_GRE
 * or PKT_TX_TUNNEL_IPIP if possible.
 * The ethdev must be configured with DEV_TX_OFFLOAD_IP_TNL_TSO.
 * Outer and inner checksums are done according to the existing flags like
 * PKT_TX_xxx_CKSUM.
 * Specific tunnel headers that contain payload length, sequence id
 * or checksum are not expected to be updated.
 */
#define PKT_TX_TUNNEL_IP (0xDULL << 45)
/**
 * Generic UDP encapsulated tunnel type, used for TSO and checksum offload.
 * UDP tunnel type implies outer IP layer.
 * It can be used for tunnels which are not standards or listed above.
 * It is preferred to use specific tunnel flags like PKT_TX_TUNNEL_VXLAN
 * if possible.
 * The ethdev must be configured with DEV_TX_OFFLOAD_UDP_TNL_TSO.
 * Outer and inner checksums are done according to the existing flags like
 * PKT_TX_xxx_CKSUM.
 * Specific tunnel headers that contain payload length, sequence id
 * or checksum are not expected to be updated.
 */
#define PKT_TX_TUNNEL_UDP (0xEULL << 45)
/* add new TX TUNNEL type here */
#define PKT_TX_TUNNEL_MASK    (0xFULL << 45)

/**
 * Double VLAN insertion (QinQ) request to driver, driver may offload the
 * insertion based on device capability.
 * mbuf 'vlan_tci' & 'vlan_tci_outer' must be valid when this flag is set.
 */
#define PKT_TX_QINQ        (1ULL << 49)
/* this old name is deprecated */
#define PKT_TX_QINQ_PKT    PKT_TX_QINQ

/**
 * TCP segmentation offload. To enable this offload feature for a
 * packet to be transmitted on hardware supporting TSO:
 *  - set the PKT_TX_TCP_SEG flag in mbuf->ol_flags (this flag implies
 *    PKT_TX_TCP_CKSUM)
 *  - set the flag PKT_TX_IPV4 or PKT_TX_IPV6
 *  - if it's IPv4, set the PKT_TX_IP_CKSUM flag
 *  - fill the mbuf offload information: l2_len, l3_len, l4_len, tso_segsz
 */
#define PKT_TX_TCP_SEG       (1ULL << 50) //tso功能标记

/** TX IEEE1588 packet to timestamp. */
#define PKT_TX_IEEE1588_TMST (1ULL << 51)

/**
 * Bits 52+53 used for L4 packet type with checksum enabled: 00: Reserved,
 * 01: TCP checksum, 10: SCTP checksum, 11: UDP checksum. To use hardware
 * L4 checksum offload, the user needs to:
 *  - fill l2_len and l3_len in mbuf
 *  - set the flags PKT_TX_TCP_CKSUM, PKT_TX_SCTP_CKSUM or PKT_TX_UDP_CKSUM
 *  - set the flag PKT_TX_IPV4 or PKT_TX_IPV6
 */
#define PKT_TX_L4_NO_CKSUM   (0ULL << 52) /**< Disable L4 cksum of TX pkt. */

/** TCP cksum of TX pkt. computed by NIC. */
#define PKT_TX_TCP_CKSUM     (1ULL << 52)

/** SCTP cksum of TX pkt. computed by NIC. */
#define PKT_TX_SCTP_CKSUM    (2ULL << 52)

/** UDP cksum of TX pkt. computed by NIC. */
#define PKT_TX_UDP_CKSUM     (3ULL << 52)

/** Mask for L4 cksum offload request. */
//硬件计算4层checksum　mask
#define PKT_TX_L4_MASK       (3ULL << 52)

/**
 * Offload the IP checksum in the hardware. The flag PKT_TX_IPV4 should
 * also be set by the application, although a PMD will only check
 * PKT_TX_IP_CKSUM.
 *  - fill the mbuf offload information: l2_len, l3_len
 */
#define PKT_TX_IP_CKSUM      (1ULL << 54) //ip层checksum

/**
 * Packet is IPv4. This flag must be set when using any offload feature
 * (TSO, L3 or L4 checksum) to tell the NIC that the packet is an IPv4
 * packet. If the packet is a tunneled packet, this flag is related to
 * the inner headers.
 */
#define PKT_TX_IPV4          (1ULL << 55) //指明报文为ipv4,为了使用tso需要告知网卡报文为ipv4

/**
 * Packet is IPv6. This flag must be set when using an offload feature
 * (TSO or L4 checksum) to tell the NIC that the packet is an IPv6
 * packet. If the packet is a tunneled packet, this flag is related to
 * the inner headers.
 */
#define PKT_TX_IPV6          (1ULL << 56)

/**
 * VLAN tag insertion request to driver, driver may offload the insertion
 * based on the device capability.
 * mbuf 'vlan_tci' field must be valid when this flag is set.
 */
#define PKT_TX_VLAN          (1ULL << 57)
/* this old name is deprecated */
#define PKT_TX_VLAN_PKT      PKT_TX_VLAN

/**
 * Offload the IP checksum of an external header in the hardware. The
 * flag PKT_TX_OUTER_IPV4 should also be set by the application, although
 * a PMD will only check PKT_TX_OUTER_IP_CKSUM.
 *  - fill the mbuf offload information: outer_l2_len, outer_l3_len
 */
#define PKT_TX_OUTER_IP_CKSUM   (1ULL << 58)

/**
 * Packet outer header is IPv4. This flag must be set when using any
 * outer offload feature (L3 or L4 checksum) to tell the NIC that the
 * outer header of the tunneled packet is an IPv4 packet.
 */
#define PKT_TX_OUTER_IPV4   (1ULL << 59)

/**
 * Packet outer header is IPv6. This flag must be set when using any
 * outer offload feature (L4 checksum) to tell the NIC that the outer
 * header of the tunneled packet is an IPv6 packet.
 */
#define PKT_TX_OUTER_IPV6    (1ULL << 60)

/**
 * Bitmask of all supported packet Tx offload features flags,
 * which can be set for packet.
 */
#define PKT_TX_OFFLOAD_MASK (    \
		PKT_TX_OUTER_IPV6 |	 \
		PKT_TX_OUTER_IPV4 |	 \
		PKT_TX_OUTER_IP_CKSUM |  \
		PKT_TX_VLAN_PKT |        \
		PKT_TX_IPV6 |		 \
		PKT_TX_IPV4 |		 \
		PKT_TX_IP_CKSUM |        \
		PKT_TX_L4_MASK |         \
		PKT_TX_IEEE1588_TMST |	 \
		PKT_TX_TCP_SEG |         \
		PKT_TX_QINQ_PKT |        \
		PKT_TX_TUNNEL_MASK |	 \
		PKT_TX_MACSEC |		 \
		PKT_TX_SEC_OFFLOAD |	 \
		PKT_TX_UDP_SEG |	 \
		PKT_TX_OUTER_UDP_CKSUM)

/**
 * Mbuf having an external buffer attached. shinfo in mbuf must be filled.
 */
#define EXT_ATTACHED_MBUF    (1ULL << 61)

#define IND_ATTACHED_MBUF    (1ULL << 62) /**< Indirect attached mbuf */

/** Alignment constraint of mbuf private area. */
#define RTE_MBUF_PRIV_ALIGN 8

/**
 * Some NICs need at least 2KB buffer to RX standard Ethernet frame without
 * splitting it into multiple segments.
 * So, for mbufs that planned to be involved into RX/TX, the recommended
 * minimal buffer length is 2KB + RTE_PKTMBUF_HEADROOM.
 */
#define	RTE_MBUF_DEFAULT_DATAROOM	2048
#define	RTE_MBUF_DEFAULT_BUF_SIZE	\
	(RTE_MBUF_DEFAULT_DATAROOM + RTE_PKTMBUF_HEADROOM)

struct rte_mbuf_sched {
	uint32_t queue_id;   /**< Queue ID. */
	uint8_t traffic_class;
	/**< Traffic class ID. Traffic class 0
	 * is the highest priority traffic class.
	 */
	uint8_t color;
	/**< Color. @see enum rte_color.*/
	uint16_t reserved;   /**< Reserved. */
}; /**< Hierarchical scheduler */

/**
 * enum for the tx_offload bit-fields lengths and offsets.
 * defines the layout of rte_mbuf tx_offload field.
 */
enum {
	RTE_MBUF_L2_LEN_BITS = 7,
	RTE_MBUF_L3_LEN_BITS = 9,
	RTE_MBUF_L4_LEN_BITS = 8,
	RTE_MBUF_TSO_SEGSZ_BITS = 16,
	RTE_MBUF_OUTL3_LEN_BITS = 9,
	RTE_MBUF_OUTL2_LEN_BITS = 7,
	RTE_MBUF_TXOFLD_UNUSED_BITS = sizeof(uint64_t) * CHAR_BIT -
		RTE_MBUF_L2_LEN_BITS -
		RTE_MBUF_L3_LEN_BITS -
		RTE_MBUF_L4_LEN_BITS -
		RTE_MBUF_TSO_SEGSZ_BITS -
		RTE_MBUF_OUTL3_LEN_BITS -
		RTE_MBUF_OUTL2_LEN_BITS,
#if RTE_BYTE_ORDER == RTE_BIG_ENDIAN
	RTE_MBUF_L2_LEN_OFS =
		sizeof(uint64_t) * CHAR_BIT - RTE_MBUF_L2_LEN_BITS,
	RTE_MBUF_L3_LEN_OFS = RTE_MBUF_L2_LEN_OFS - RTE_MBUF_L3_LEN_BITS,
	RTE_MBUF_L4_LEN_OFS = RTE_MBUF_L3_LEN_OFS - RTE_MBUF_L4_LEN_BITS,
	RTE_MBUF_TSO_SEGSZ_OFS = RTE_MBUF_L4_LEN_OFS - RTE_MBUF_TSO_SEGSZ_BITS,
	RTE_MBUF_OUTL3_LEN_OFS =
		RTE_MBUF_TSO_SEGSZ_OFS - RTE_MBUF_OUTL3_LEN_BITS,
	RTE_MBUF_OUTL2_LEN_OFS =
		RTE_MBUF_OUTL3_LEN_OFS - RTE_MBUF_OUTL2_LEN_BITS,
	RTE_MBUF_TXOFLD_UNUSED_OFS =
		RTE_MBUF_OUTL2_LEN_OFS - RTE_MBUF_TXOFLD_UNUSED_BITS,
#else
	RTE_MBUF_L2_LEN_OFS = 0,
	RTE_MBUF_L3_LEN_OFS = RTE_MBUF_L2_LEN_OFS + RTE_MBUF_L2_LEN_BITS,
	RTE_MBUF_L4_LEN_OFS = RTE_MBUF_L3_LEN_OFS + RTE_MBUF_L3_LEN_BITS,
	RTE_MBUF_TSO_SEGSZ_OFS = RTE_MBUF_L4_LEN_OFS + RTE_MBUF_L4_LEN_BITS,
	RTE_MBUF_OUTL3_LEN_OFS =
		RTE_MBUF_TSO_SEGSZ_OFS + RTE_MBUF_TSO_SEGSZ_BITS,
	RTE_MBUF_OUTL2_LEN_OFS =
		RTE_MBUF_OUTL3_LEN_OFS + RTE_MBUF_OUTL3_LEN_BITS,
	RTE_MBUF_TXOFLD_UNUSED_OFS =
		RTE_MBUF_OUTL2_LEN_OFS + RTE_MBUF_OUTL2_LEN_BITS,
#endif
};

/**
 * The generic rte_mbuf, containing a packet mbuf.
 */
struct rte_mbuf {
	RTE_MARKER cacheline0;// 这种写法很帅气

	//mbuf的数据缓冲起始地址
	void *buf_addr;           /**< Virtual address of segment buffer. */
	/**
	 * Physical address of segment buffer.
	 * Force alignment to 8-bytes, so as to ensure we have the exact
	 * same mbuf cacheline0 layout for 32-bit and 64-bit. This makes
	 * working on vector drivers easier.
	 */
	RTE_STD_C11
	union {
		rte_iova_t buf_iova;//buf_addr的物理地址
		rte_iova_t buf_physaddr; /**< deprecated */
	} __rte_aligned(sizeof(rte_iova_t));

	/* next 8 bytes are initialised on RX descriptor rearm */
	RTE_MARKER64 rearm_data;
	uint16_t data_off;//真实存放数据的位置（自buf_addr偏移data_off)

	/**
	 * Reference counter. Its size should at least equal to the size
	 * of port field (16 bits), to support zero-copy broadcast.
	 * It should only be accessed using the following functions:
	 * rte_mbuf_refcnt_update(), rte_mbuf_refcnt_read(), and
	 * rte_mbuf_refcnt_set(). The functionality of these functions (atomic,
	 * or non-atomic) is controlled by the CONFIG_RTE_MBUF_REFCNT_ATOMIC
	 * config option.
	 */
	RTE_STD_C11
	union {
		rte_atomic16_t refcnt_atomic; /**< Atomically accessed refcnt */
		/** Non-atomically accessed refcnt */
		uint16_t refcnt;
	};
	uint16_t nb_segs;         /**< Number of segments. */

	/** Input port (16 bits to support more than 256 virtual ports).
	 * The event eth Tx adapter uses this field to specify the output port.
	 */
	uint16_t port;

	//mbuf的offload功能标记（dpdk定义了一组功能，然后由驱动对这组功能进行映射，并对应到各自硬件特性上）
	uint64_t ol_flags;        /**< Offload features. */

	/* remaining bytes are set on RX when pulling packet from descriptor */
	RTE_MARKER rx_descriptor_fields1;

	/*
	 * The packet type, which is the combination of outer/inner L2, L3, L4
	 * and tunnel types. The packet_type is about data really present in the
	 * mbuf. Example: if vlan stripping is enabled, a received vlan packet
	 * would have RTE_PTYPE_L2_ETHER and not RTE_PTYPE_L2_VLAN because the
	 * vlan is stripped from the data.
	 */
	RTE_STD_C11
	union {
		uint32_t packet_type; /**< L2/L3/L4 and tunnel information. */
		__extension__
		struct {
			uint8_t l2_type:4;   /**< (Outer) L2 type. */
			uint8_t l3_type:4;   /**< (Outer) L3 type. */
			uint8_t l4_type:4;   /**< (Outer) L4 type. */
			uint8_t tun_type:4;  /**< Tunnel type. */
			RTE_STD_C11
			union {
				uint8_t inner_esp_next_proto;
				/**< ESP next protocol type, valid if
				 * RTE_PTYPE_TUNNEL_ESP tunnel type is set
				 * on both Tx and Rx.
				 */
				__extension__
				struct {
					uint8_t inner_l2_type:4;
					/**< Inner L2 type. */
					uint8_t inner_l3_type:4;
					/**< Inner L3 type. */
				};
			};
			uint8_t inner_l4_type:4; /**< Inner L4 type. */
		};
	};

	//总的报文长度(可能有多个seg,用next串起来)
	uint32_t pkt_len;         /**< Total pkt len: sum of all segments. */
	//mbuf中的数据长度
	uint16_t data_len;        /**< Amount of data in segment buffer. */
	/** VLAN TCI (CPU order), valid if PKT_RX_VLAN is set. */
	//为了让网卡来插入vlan,这里将vlan放在此种（cpu序），等报文发送时，网卡将报文插入
	uint16_t vlan_tci;

	RTE_STD_C11
	union {
		union {
			uint32_t rss;     /**< RSS hash result if RSS enabled */
			struct {
				union {
					struct {
						uint16_t hash;
						uint16_t id;
					};
					uint32_t lo;
					/**< Second 4 flexible bytes */
				};
				uint32_t hi;
				/**< First 4 flexible bytes or FD ID, dependent
				 * on PKT_RX_FDIR_* flag in ol_flags.
				 */
			} fdir;	/**< Filter identifier if FDIR enabled */
			struct rte_mbuf_sched sched;
			/**< Hierarchical scheduler : 8 bytes */
			struct {
				uint32_t reserved1;
				uint16_t reserved2;
				uint16_t txq;
				/**< The event eth Tx adapter uses this field
				 * to store Tx queue id.
				 * @see rte_event_eth_tx_adapter_txq_set()
				 */
			} txadapter; /**< Eventdev ethdev Tx adapter */
			/**< User defined tags. See rte_distributor_process() */
			uint32_t usr;
		} hash;                   /**< hash information */
	};

	/** Outer VLAN TCI (CPU order), valid if PKT_RX_QINQ is set. */
	uint16_t vlan_tci_outer;

	uint16_t buf_len;         /**< Length of segment buffer. */

	/** Valid if PKT_RX_TIMESTAMP is set. The unit and time reference
	 * are not normalized but are always the same for a given port.
	 * Some devices allow to query rte_eth_read_clock that will return the
	 * current device timestamp.
	 */
	uint64_t timestamp;

	/* second cache line - fields only used in slow path or on TX */
	RTE_MARKER cacheline1 __rte_cache_min_aligned;

	RTE_STD_C11
	union {
		void *userdata;   /**< Can be used for external metadata */
		uint64_t udata64; /**< Allow 8-byte userdata on 32-bit */
	};

	struct rte_mempool *pool; /**< Pool from which mbuf was allocated. */
	struct rte_mbuf *next;    /**< Next segment of scattered packet. */

	/* fields to support TX offloads */
	RTE_STD_C11
	union {
		uint64_t tx_offload;       /**< combined for easy fetch */
		__extension__
		struct {
		    //2层头长度
			uint64_t l2_len:RTE_MBUF_L2_LEN_BITS;
			/**< L2 (MAC) Header Length for non-tunneling pkt.
			 * Outer_L4_len + ... + Inner_L2_len for tunneling pkt.
			 */
			uint64_t l3_len:RTE_MBUF_L3_LEN_BITS;//3层头部的长度
			/**< L3 (IP) Header Length. */
			uint64_t l4_len:RTE_MBUF_L4_LEN_BITS;//3层头部的长度
			/**< L4 (TCP/UDP) Header Length. */
            //在不支持TSO的网卡上，TCP层向IP层发送数据会考虑mss，使得TCP向下发送的数据可以包含在一个IP分组中而不会造成分片，
            //mss是在TCP初始建立连接时由网卡MTU确定并和对端协商的，所以在一个MTU＝1500的网卡上，TCP向下发送的数据不会大于
            //min(mss_local, mss_remote)-ip头-tcp头。
            //网卡支持TSO时，TCP层会逐渐增大mss（总是整数倍数增加），当TCP层向下发送大块数据时，仅仅计算TCP头，网卡接到到了
            //IP层传下的大数 据包后自己重新分成若干个IP数据包，添加IP头，复制TCP头并且重新计算校验和等相关数据，这样就把一部
            //分CPU相关的处理工作转移到由网卡来处理。
			uint64_t tso_segsz:RTE_MBUF_TSO_SEGSZ_BITS;
			/**< TCP TSO segment size */

			/*
			 * Fields for Tx offloading of tunnels.
			 * These are undefined for packets which don't request
			 * any tunnel offloads (outer IP or UDP checksum,
			 * tunnel TSO).
			 *
			 * PMDs should not use these fields unconditionally
			 * when calculating offsets.
			 *
			 * Applications are expected to set appropriate tunnel
			 * offload flags when they fill in these fields.
			 */
			uint64_t outer_l3_len:RTE_MBUF_OUTL3_LEN_BITS;
			/**< Outer L3 (IP) Hdr Length. */
			uint64_t outer_l2_len:RTE_MBUF_OUTL2_LEN_BITS;
			/**< Outer L2 (MAC) Hdr Length. */

			/* uint64_t unused:RTE_MBUF_TXOFLD_UNUSED_BITS; */
		};
	};

	/** Size of the application private data. In case of an indirect
	 * mbuf, it stores the direct mbuf private data size.
	 */
    //mbuf缓冲的头部存放mbuf的控制信息（rte_mbuf_t结构内容），接着在后面有一个
    //priv_size用于存放用户定制的信息，例如crypto中存放rte_crypto_op
	uint16_t priv_size;

	/** Timesync flags for use with IEEE1588. */
	uint16_t timesync;

	/** Sequence number. See also rte_reorder_insert(). */
	uint32_t seqn;

	/** Shared data for external buffer attached to mbuf. See
	 * rte_pktmbuf_attach_extbuf().
	 */
	struct rte_mbuf_ext_shared_info *shinfo;

	uint64_t dynfield1[2]; /**< Reserved for dynamic fields. */
} __rte_cache_aligned;

/**
 * Function typedef of callback to free externally attached buffer.
 */
typedef void (*rte_mbuf_extbuf_free_callback_t)(void *addr, void *opaque);

/**
 * Shared data at the end of an external buffer.
 */
struct rte_mbuf_ext_shared_info {
	rte_mbuf_extbuf_free_callback_t free_cb; /**< Free callback function */
	void *fcb_opaque;                        /**< Free callback argument */
	RTE_STD_C11
	union {
		rte_atomic16_t refcnt_atomic; /**< Atomically accessed refcnt */
		uint16_t refcnt;
	};
};

/**< Maximum number of nb_segs allowed. */
#define RTE_MBUF_MAX_NB_SEGS	UINT16_MAX

/**
 * Returns TRUE if given mbuf is cloned by mbuf indirection, or FALSE
 * otherwise.
 *
 * If a mbuf has its data in another mbuf and references it by mbuf
 * indirection, this mbuf can be defined as a cloned mbuf.
 */
#define RTE_MBUF_CLONED(mb)     ((mb)->ol_flags & IND_ATTACHED_MBUF)

/**
 * Returns TRUE if given mbuf has an external buffer, or FALSE otherwise.
 *
 * External buffer is a user-provided anonymous buffer.
 */
#define RTE_MBUF_HAS_EXTBUF(mb) ((mb)->ol_flags & EXT_ATTACHED_MBUF)

/**
 * Returns TRUE if given mbuf is direct, or FALSE otherwise.
 *
 * If a mbuf embeds its own data after the rte_mbuf structure, this mbuf
 * can be defined as a direct mbuf.
 */
#define RTE_MBUF_DIRECT(mb) \
	(!((mb)->ol_flags & (IND_ATTACHED_MBUF | EXT_ATTACHED_MBUF)))

#define MBUF_INVALID_PORT UINT16_MAX

/**
 * A macro that points to an offset into the data in the mbuf.
 *
 * The returned pointer is cast to type t. Before using this
 * function, the user must ensure that the first segment is large
 * enough to accommodate its data.
 *
 * @param m
 *   The packet mbuf.
 * @param o
 *   The offset into the mbuf data.
 * @param t
 *   The type to cast the result into.
 */
//buf_addr + data_off可以找到真正数据的起始位置，在这个位置处再进行o的偏移，以便扩大或缩小headroom的空间
#define rte_pktmbuf_mtod_offset(m, t, o)	\
	((t)((char *)(m)->buf_addr + (m)->data_off + (o)))

/**
 * A macro that points to the start of the data in the mbuf.
 *
 * The returned pointer is cast to type t. Before using this
 * function, the user must ensure that the first segment is large
 * enough to accommodate its data.
 *
 * @param m
 *   The packet mbuf.
 * @param t
 *   The type to cast the result into.
 */
//mtod ＝ move to data
//返回指向数据区起始位置的指针
#define rte_pktmbuf_mtod(m, t) rte_pktmbuf_mtod_offset(m, t, 0)

/**
 * A macro that returns the IO address that points to an offset of the
 * start of the data in the mbuf
 *
 * @param m
 *   The packet mbuf.
 * @param o
 *   The offset into the data to calculate address from.
 */
#define rte_pktmbuf_iova_offset(m, o) \
	(rte_iova_t)((m)->buf_iova + (m)->data_off + (o))

/**
 * A macro that returns the IO address that points to the start of the
 * data in the mbuf
 *
 * @param m
 *   The packet mbuf.
 */
#define rte_pktmbuf_iova(m) rte_pktmbuf_iova_offset(m, 0)

#ifdef __cplusplus
}
#endif

#endif /* _RTE_MBUF_CORE_H_ */
