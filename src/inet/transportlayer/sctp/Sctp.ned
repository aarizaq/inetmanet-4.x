//
// Copyright (C) 2005-2010 Irene Ruengeler
// Copyright (C) 2009-2012 Thomas Dreibholz
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//


package inet.transportlayer.sctp;

import inet.common.SimpleModule;
import inet.transportlayer.contract.ISctp;

//
// Implements the SCTP protocol.
//
simple Sctp extends SimpleModule like ISctp
{
    parameters:
        @class(Sctp);
        string interfaceTableModule;
        string routingTableModule;
        string checksumMode @enum("declared", "computed") = default("declared");

        //#====== SCTP Association Parameters =================================
        int numGapReports = default(3);
        double rtoInitial @unit(s) = default(3s);
        double rtoMin @unit(s) = default(1s);
        double rtoMax @unit(s) = default(60s);
        double rtoAlpha = default(0.125);
        double rtoBeta = default(0.250);
        int maxBurst = default(4);
        string maxBurstVariant = default("maxBurst");
        int initialWindow = default(0);
        int assocMaxRetrans = default(10);
        int pathMaxRetrans = default(5);
        int maxInitRetrans = default(8);
        double sackPeriod @unit(s) = default(200ms);
        int sackFrequency = default(2);
        bool reactivatePrimaryPath = default(false);
        int sendQueueLimit = default(0);
        double validCookieLifetime @unit(s) = default(10s);

        //#====== Testing =====================================================

        //#====== Heartbeats ==================================================
        bool enableHeartbeats = default(true);
        double hbInterval @unit(s) = default(30s);
        bool sendHeartbeatsOnActivePaths = default(false);

        //#====== Nagle Algorithm =============================================
        bool nagleEnabled = default(true);
        int naglePoint = default(1468);

        //#====== Fragmentation =============================================
        int fragPoint = default(1452);    // Size of a message that has to be fragmented, i.e. maximum size of a fragment

        //#====== Congestion Control ==========================================
        bool fastRecoverySupported = default(true);
        string sctpAlgorithmClass = default("SctpAlg");
        int ccModule = default(0);           // RFC4960=0
        bool osbWithHeader = default(false);
        int RTXMethod = default(0);        // 0=Once after 3 Sacks; 1=once per RTT; 2=Switch off Fast Retransmit; 3=Always after 3 Sacks

        //#====== Other Parameters ============================================
        int ssModule = default(0);           // ROUND_ROBIN=0
        int arwnd = default(65535);
        int swsLimit = default(3000);        // Limit for SWS
        bool udpEncapsEnabled = default(false);
        bool natFriendly = default(false);
        int bytesToAddPerRcvdChunk = default(0);        // For flow control
        int bytesToAddPerPeerChunk = default(0);        // For flow control
        bool tellArwnd = default(false);            // For flow control
        int messageAcceptLimit = default(0);        // For flow control
        double fairStart @unit(s) = default(0s);
        double fairStop @unit(s) = default(0s);
        string streamsToPaths = default("");
        int startEndToEndDelay = default(0);
        int stopEndToEndDelay = default(10);
        double throughputInterval = default(1);

        //#====== Chunk Authentication ========================================
        bool auth = default(false);
        string chunks = default("");
        bool padding = default(false);

        //#====== Add-IP ======================================================
        bool addIP = default(false);
        double addTime @unit(s) = default(0s);
        string addAddress = default("");
        string addIpType = default("0");      // SET_PRIMARY_ADDRESS=49156,ADD_IP_ADDRESS=49153,DELETE_IP_ADDRESS=49154

        //#====== Packet Drop =================================================
        bool packetDrop = default(false);

        //#====== SACK Now ====================================================
        bool sackNow = default(false);

        //#====== High-Speed CC ===============================================
        bool highSpeedCC = default(false);

        //#====== SACK Sequence Numbers =======================================
        bool checkSackSeqNumber = default(false);

        //#====== NR-SACK =====================================================
        bool nrSack = default(false);
        bool disableReneging = default(false);
        int gapReportLimit = default(100000000);
        string gapListOptimizationVariant = default("none");
        bool smartOverfullSACKHandling = default(false);

        //#====== QoS-SCTP ====================================================
        bool allowQoSTracking = default(false);
        double qosTrackingInterval @unit(s) = default(1.0s);

        //#====== CMT-SCTP ====================================================
        string cmtSendAllVariant = default("normal");
        string cmtRetransmissionVariant = default("normal");
        string cmtCUCVariant = default("normal");
        string cmtBufferSplitVariant = default("none");
        bool cmtBufferSplittingUsesOSB = default(false);
        bool cmtUseSFR = default(true);
        bool cmtUseDAC = default(true);
        bool cmtUseFRC = default(true);
        bool cmtSmartT3Reset = default(true);
        bool cmtSmartFastRTX = default(true);
        bool cmtSmartReneging = default(false);
        bool cmtSlowPathRTTUpdate = default(false);
        string cmtChunkReschedulingVariant = default("none");
        double cmtChunkReschedulingThreshold = default(0.5);
        bool cmtMovedChunksReduceCwnd = default(true);
        double movedChunkFastRTXFactor = default(2.0);
        string cmtSackPath = default("standard");

        //#====== CMT/RP-SCTP =================================================
        bool strictCwndBooking = default(false);
        // CMT Congestion Control Variant
        // off     = use over every path the default RFC4960 CC (New Reno)
        // lia     = First resource pooling congestion control RFC6356 adapted for SCTP - https://datatracker.ietf.org/doc/rfc6356/
        // olia    = alternative MPTCP CC, adapted for SCTP- https://tools.ietf.org/html/draft-khalili-mptcp-congestion-control-05
        // cmtrpv1 = First resource pooling based CC for SCTP - https://www.tdr.wiwi.uni-due.de/fileadmin/fileupload/I-TDR/SCTP/Paper/AINA2010.pdf
        // cmtrpv2 = Updated version of cmtrpv1 - https://www.tdr.wiwi.uni-due.de/fileadmin/fileupload/I-TDR/SCTP/Paper/ConTEL2011.pdf
        string cmtCCVariant = default("off");
        string cmtCCPathGroups = default("");
        bool rpPathBlocking = default(false);
        bool rpScaleBlockingTimeout = default(false);
        int rpMinCwnd = default(1);

        @display("i=block/wheelbarrow");

    gates:
        input appIn @messageKinds(inet::SctpCommandCode);
        input ipIn;
        output appOut @labels(SctpCommand/up) @messageKinds(inet::SctpStatusInd);
        output ipOut;
}

