/*****************************************************************************
 *
 * Copyright (C) 2001 Uppsala University & Ericsson AB.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Erik Nordstr�m, <erik.nordstrom@it.uu.se>
 *
 *****************************************************************************/

#include "inet/routing/extras/aodv-uu/aodv_uu_omnet.h"
#include "inet/linklayer/common/Ieee802Ctrl.h"
#include "inet/networklayer/ipv4/Ipv4Header_m.h"
#include "inet/networklayer/common/L3Tools.h"
#include "inet/linklayer/common/MacAddressTag_m.h"

namespace inet {

namespace inetmanet {

#define GARBAGE_COLLECT

void NS_CLASS packet_queue_init(void)
{
    PQ.pkQueue.clear();
#ifdef GARBAGE_COLLECT
    /* Set up garbage collector */
    timer_init(&PQ.garbage_collect_timer, &NS_CLASS packet_queue_timeout, &PQ);

    timer_set_timeout(&PQ.garbage_collect_timer, GARBAGE_COLLECT_TIME);
#endif
}


void NS_CLASS packet_queue_destroy(void)
{
    int count = 0;
    while (!PQ.pkQueue.empty())
    {
        struct q_pkt *qp = PQ.pkQueue.back();
        if (getNetworkProtocol())
            getNetworkProtocol()->dropQueuedDatagram(qp->p);
        else
            delete qp->p;
        PQ.pkQueue.pop_back();
        // TODO: Discard
        delete qp;
        count++;
    }
    DEBUG(LOG_INFO, 0, "Destroyed %d buffered packets!", count);
}

/* Garbage collect packets which have been queued for too long... */
int NS_CLASS packet_queue_garbage_collect(void)
{
    int count = 0;
    struct timeval now;

    gettimeofday(&now, nullptr);

    for (unsigned int i=0; i < PQ.pkQueue.size();)
    {
        auto qp = PQ.pkQueue[i];
        if (timeval_diff(&now, &qp->q_time) > MAX_QUEUE_TIME)
        {
            PQ.pkQueue.erase(PQ.pkQueue.begin()+i);
            sendICMP(qp->p);
            getNetworkProtocol()->dropQueuedDatagram(qp->p);
            delete qp;
            count++;
        }
        else
            i++;
    }

    if (count)
    {
        DEBUG(LOG_DEBUG, 0, "Removed %d packet(s)!", count);
    }

    return count;
}
/* Buffer a packet in a FIFO queue. Implemented as a linked list,
   where we add elements at the end and remove at the beginning.... */

void NS_CLASS packet_queue_add(Packet * p, struct in_addr dest_addr)
{
    q_pkt *qp;

    if (PQ.pkQueue.size() >= MAX_QUEUE_LENGTH)
    {
        DEBUG(LOG_DEBUG, 0, "MAX Queue length! Removing first packet.");
        qp = PQ.pkQueue.front();
        //PQ.pkQueue.pop_front();
        PQ.pkQueue.erase(PQ.pkQueue.begin());
        auto dgram = qp->p;
        sendICMP(dgram);
        getNetworkProtocol()->dropQueuedDatagram(dgram);
        free(qp);
    }

    qp = new q_pkt;

    if (qp == nullptr)
    {
        fprintf(stderr, "Malloc failed!\n");
        exit(-1);
    }
    qp->p = p;
    qp->dest_addr = dest_addr;

    gettimeofday(&qp->q_time, nullptr);
    PQ.pkQueue.push_back(qp);

    DEBUG(LOG_INFO, 0, "buffered pkt to %s qlen=%u",
          ip_to_str(dest_addr), PQ.length());
}


void NS_CLASS packet_queue_add_inject(Packet * p, struct in_addr dest_addr)
{
    struct q_pkt *qp;

    if (PQ.pkQueue.size() >= MAX_QUEUE_LENGTH)
    {
        DEBUG(LOG_DEBUG, 0, "MAX Queue length! Removing first packet.");
        qp = PQ.pkQueue.front();
        //PQ.pkQueue.pop_front();
        PQ.pkQueue.erase(PQ.pkQueue.begin());
        auto dgram = qp->p;
        sendICMP(dgram);
        getNetworkProtocol()->dropQueuedDatagram(dgram);
        delete qp;
    }

    qp = new q_pkt;

    if (qp == nullptr)
    {
        fprintf(stderr, "Malloc failed!\n");
        exit(-1);
    }
    qp->p = p;
    qp->dest_addr = dest_addr;

    if (getNetworkProtocol()) {
        drop((cOwnedObject *)p);
        getNetworkProtocol()->enqueueRoutingHook(p, IHook::Type::PREROUTING);
    }

    gettimeofday(&qp->q_time, nullptr);
    PQ.pkQueue.push_back(qp);

    DEBUG(LOG_INFO, 0, "buffered pkt to %s qlen=%u",
          ip_to_str(dest_addr), PQ.length());
}



int NS_CLASS packet_queue_set_verdict(struct in_addr dest_addr, int verdict)
{
    int count = 0;
    rt_table_t *rt, *next_hop_rt, *inet_rt = nullptr;
    struct in_addr gw_addr;

    double delay = 0;
#define ARP_DELAY 0.005

    if (verdict == PQ_ENC_SEND)
    {
        gw_addr.s_addr = L3Address(*gateWayAddress);
        rt = rt_table_find(gw_addr);
    }
    else
        rt = rt_table_find(dest_addr);

    std::vector<L3Address> list;
    if (isInMacLayer())
    {
        std::vector<MacAddress> listMac;
        getApList(dest_addr.s_addr .toMac(), listMac);
        while (!listMac.empty())
        {
            list.push_back(L3Address(listMac.back()));
            listMac.pop_back();
        }
    }
    else
    {
        std::vector<Ipv4Address> listIp;
        getApListIp(dest_addr.s_addr.toIpv4(),listIp);
        while (!listIp.empty())
        {
            list.push_back(L3Address(listIp.back()));
            listIp.pop_back();
        }
    }

    while (!list.empty())
    {
        struct in_addr dest_addr;
        dest_addr.s_addr = list.back();
        list.pop_back();
        for (unsigned int i=0; i < PQ.pkQueue.size();)
        {
            auto qp = PQ.pkQueue[i];
            if (qp->dest_addr.s_addr == dest_addr.s_addr)
            {
                PQ.pkQueue.erase(PQ.pkQueue.begin()+i);
                switch (verdict)
                {
                    case PQ_ENC_SEND:
                        throw cRuntimeError("Encapsulated Gateway not supported");
                        // TODO: Encapsulated IP support.
                        if (qp->p->peekAtFront<Ipv4Header>())
                        {
                            //qp->p = pkt_encapsulate(qp->p, *gateWayAddress);
                            // now Ip layer decremented again
                            /* Apparently, the link layer implementation can't handle a burst of packets. So to keep ARP happy, buffered
                             * packets are sent with ARP_DELAY seconds between sends. */
                            if (getNetworkProtocol())
                                getNetworkProtocol()->reinjectQueuedDatagram(qp->p);
                            else
                                sendDelayed(qp->p, delay, "ipOut");
                            //delay += ARP_DELAY;
                        }
                        else
                        {
                            sendICMP(qp->p);
                            getNetworkProtocol()->dropQueuedDatagram(qp->p);
                        }
                    break;
                    case PQ_SEND:
                        if (!rt)
                            return -1;
                        // now Ip layer decremented again
                        if (isInMacLayer())
                        {
                            // Ieee802Ctrl *ctrl = new Ieee802Ctrl();

                            L3Address nextHop;
                            int iface;
                            double cost;

                            getNextHop(dest_addr.s_addr, nextHop, iface, cost);
                            qp->p->addTagIfAbsent<MacAddressReq>()->setDestAddress(nextHop.toMac());
                            // ctrl->setDest(nextHop.toMac());
                            //TODO ctrl->setEtherType(...);
                            // qp->p->setControlInfo(ctrl);
                        }
                        /* Apparently, the link layer implementation can't handle
                         * a burst of packets. So to keep ARP happy, buffered
                         * packets are sent with ARP_DELAY seconds between
                         * sends. */
                         // now Ip layer decremented again
                        if (getNetworkProtocol())
                            getNetworkProtocol()->reinjectQueuedDatagram(qp->p);
                        else
                            sendDelayed(qp->p, delay, "ipOut");
                        delay += ARP_DELAY;
                    break;
                    case PQ_DROP:
                        sendICMP(qp->p);
                        getNetworkProtocol()->dropQueuedDatagram(qp->p);
                    break;
                }
                delete qp;
                count++;
            }
            else
                i++;
        }
    }
    /* Update rt timeouts */
    if (rt && rt->state == VALID &&
            (verdict == PQ_SEND || verdict == PQ_ENC_SEND))
    {
        if (dest_addr.s_addr != DEV_IFINDEX(rt->ifindex).ipaddr.s_addr)
        {
            if (verdict == PQ_ENC_SEND && inet_rt)
                rt_table_update_timeout(inet_rt, ACTIVE_ROUTE_TIMEOUT);

            rt_table_update_timeout(rt, ACTIVE_ROUTE_TIMEOUT);

            next_hop_rt = rt_table_find(rt->next_hop);

            if (next_hop_rt && next_hop_rt->state == VALID &&
                    next_hop_rt->dest_addr.s_addr != rt->dest_addr.s_addr)
                rt_table_update_timeout(next_hop_rt, ACTIVE_ROUTE_TIMEOUT);
        }

        DEBUG(LOG_INFO, 0, "SENT %d packets to %s qlen=%u",
              count, ip_to_str(dest_addr), PQ.length());
    }
    else if (verdict == PQ_DROP)
    {
        DEBUG(LOG_INFO, 0, "DROPPED %d packets for %s!",
              count, ip_to_str(dest_addr));
    }

    return count;
}

} // namespace inetmanet

} // namespace inet

