/* Copyright (C) Uppsala University
 *
 * This file is distributed under the terms of the GNU general Public
 * License (GPL), see the file LICENSE
 *
 * Author: Erik Nordström, <erikn@it.uu.se>
 */
#ifndef _DSR_PKT_H
#define _DSR_PKT_H

#include "inet/common/INETDefs.h"
#include "inet/routing/extras/base/compatibility.h"
#include "inet/routing/extras/dsr/dsr-uu/dsr_options.h"

namespace inet {

namespace inetmanet {

class DSRPkt;
struct EtxCost;

struct dsr_srt
{
    struct in_addr src;
    struct in_addr dst;
    unsigned short flags;
    unsigned short index;
    unsigned int laddrs;    /* length in bytes if addrs */
    std::vector<unsigned int> cost;
    VectorAddressIn addrs;  /* Intermediate nodes */
    dsr_srt &  operator= (const dsr_srt &m);

    dsr_srt();
    ~dsr_srt();
};

#define MAX_RREP_OPTS 10
#define MAX_RERR_OPTS 10
#define MAX_ACK_OPTS  10

#define DEFAULT_TAILROOM 128

struct ethhdr
{
    unsigned char   h_dest[ETH_ALEN];   /* destination eth addr */
    unsigned char   h_source[ETH_ALEN]; /* source ether addr    */
    uint16_t    h_proto;        /* packet type ID field */

};

struct iphdr
{
    unsigned short ihl;
    unsigned int version:4;
    u_int8_t tos;
    u_int16_t tot_len;
    u_int16_t id;
    u_int16_t frag_off;
    u_int8_t ttl;
    u_int8_t protocol;
    u_int16_t check;
    L3Address saddr;
    L3Address daddr;
    /*The options start here. */
};

/* Internal representation of a packet. For portability */
struct dsr_pkt
{
    struct in_addr src; /* IP level data */
    struct in_addr dst;
    struct in_addr nxt_hop;
    struct in_addr prv_hop;
    int flags;
    int salvage;
    int numRetries;

    union
    {
        struct ethhdr *ethh;
        char *raw;
    } mac;
    char mac_data[16];
    union
    {
        struct iphdr *iph;
        char *raw;
    } nh;
    char ip_data[70];
    struct
    {
         std::vector<struct dsr_opt_hdr>opth;
    } dh;

    struct dsr_srt_opt *srt_opt = nullptr;
    std::vector<struct dsr_rreq_opt *> rreq_opt;  /* Can only be one */
    std::vector<struct dsr_rrep_opt *> rrep_opt;
    std::vector<struct dsr_rerr_opt *> rerr_opt;
    std::vector<struct dsr_ack_opt *> ack_opt;
    struct dsr_ack_req_opt *ack_req_opt = nullptr;
    struct dsr_srt *srt = nullptr;    /* Source route */
    int payload_len = 0;
    bool moreFragments = false;
    int fragmentOffset = 0;
    int totalPayloadLength = 0;

    Packet *payload = nullptr;
    int encapsulate_protocol = -1;
    // Etx cost

    std::vector<EtxCost> costVector;
    struct dsr_pkt * next;

    void clear();
    struct dsr_pkt *dup();
    int inputInterfaceId = -1;
};


/* Flags: */
#define PKT_PROMISC_RECV 0x01
#define PKT_REQUEST_ACK  0x02
#define PKT_PASSIVE_ACK  0x04
#define PKT_XMIT_JITTER  0x08

/* Packet actions: */
#define DSR_PKT_NONE           1
#define DSR_PKT_SRT_REMOVE     (DSR_PKT_NONE << 2)
#define DSR_PKT_SEND_ICMP      (DSR_PKT_NONE << 3)
#define DSR_PKT_SEND_RREP      (DSR_PKT_NONE << 4)
#define DSR_PKT_SEND_BUFFERED  (DSR_PKT_NONE << 5)
#define DSR_PKT_SEND_ACK       (DSR_PKT_NONE << 6)
#define DSR_PKT_FORWARD        (DSR_PKT_NONE << 7)
#define DSR_PKT_FORWARD_RREQ   (DSR_PKT_NONE << 8)
#define DSR_PKT_DROP           (DSR_PKT_NONE << 9)
#define DSR_PKT_ERROR          (DSR_PKT_NONE << 10)
#define DSR_PKT_DELIVER        (DSR_PKT_NONE << 11)
#define DSR_PKT_ACTION_LAST    (12)


} // namespace inetmanet

} // namespace inet

#endif              /* _DSR_PKT_H */
