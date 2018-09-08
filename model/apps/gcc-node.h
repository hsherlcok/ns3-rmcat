/******************************************************************************
 * Copyright 2016-2017 Cisco Systems, Inc.                                    *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 *                                                                            *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 ******************************************************************************/

/**
 * @file
 * Sender application interface for rmcat ns3 module.
 *
 * @version 0.1.1
 * @author Jiantao Fu
 * @author Sergio Mena
 * @author Xiaoqing Zhu
 */

#ifndef GCC_NODE_H
#define GCC_NODE_H

#include "rtp-header.h"
#include "gfp-header.h"
#include "rmcat-constants.h"
#include "ns3/syncodecs.h"
#include "ns3/socket.h"
#include "ns3/application.h"
#include <memory>
#include "ns3/gcc-receiver-controller.h"
#include "ns3/gcc-sender-controller.h"
#include <map>

namespace ns3 {

class GccNode: public Application
{
public:

    GccNode ();
    virtual ~GccNode ();

    void SetCodec (std::shared_ptr<syncodecs::Codec> codec);
    void SetCodecType (SyncodecType codecType);

    void SetDest (Ipv4Address dest_ip, uint16_t dest_port);
    void SetUp (uint16_t local_port, uint64_t stream_size);
    bool AddMulStream(uint32_t num, uint64_t* stream_size);

private:
    virtual void StartApplication ();
    virtual void StopApplication ();

    /*Send Methods*/
    void EnqueuePacket ();
    void SendPacket (uint64_t msSlept);
    void SendOverSleep (uint32_t bytesToSend, uint32_t send_ssrc);
    void SendRtcp(GccRtcpHeader header, bool reschedule);
    void CreateRtcp();
//   void SendBye(); /*Bye packet should be followed by SR/RR as RFC 3550*/
    void SendRemb();
    uint32_t GetNextRtcpTime();


    /*Recv Methods*/
    void RecvPacket (Ptr<Socket> socket);
    void RecvDataPacket(Ptr<Packet> p, Address remoteAddr);
    void RecvRtcp(Ptr<Packet> p, Address remoteAddr);
    void RecvRembPacket(Ptr<Packet> p, Address remoteAddr);


private:
    std::shared_ptr<syncodecs::Codec> m_codec;
    std::shared_ptr<rmcat::GccRecvController> m_recvController;
    std::shared_ptr<rmcat::GccSenderController> m_senderController;
    

    Ipv4Address m_destIp;
    uint16_t m_destPort;
    uint16_t m_localPort;
    Ptr<Socket> m_socket;

    //Sender 
    uint32_t m_localSsrc; //main ssrc
    std::set<uint32_t> m_srcSsrcSet; //ssrcs for multiple stream
    uint32_t m_numSrcSsrc;
    
    std::map<uint32_t, uint32_t> m_sequence;
    std::map<uint32_t, uint32_t> m_rtpTsOffset;
   
    EventId m_enqueueEvent;
    EventId m_sendEvent;
    EventId m_sendOversleepEvent;
    EventId m_rtcpEvent;
    EventId m_rembEvent;

    double m_rSend; //bps
    
    std::map<uint32_t, std::deque<uint32_t>> m_rateShapingBuf;
    std::map<uint32_t, uint32_t> m_rateShapingBytes;
    uint64_t m_totalRateShapingBuf;

    uint32_t m_nextEnqSsrcIndex;
    uint32_t m_nextSendSsrcIndex;
    std::map<uint32_t, uint64_t> m_enqBytes;
    std::map<uint32_t, uint64_t> m_maxSize; //0 means infinite size
    bool m_complete;

    std::set<uint32_t> m_byeSet;

    bool m_sending;

    //Receiver
    uint32_t m_remoteSsrc; //main ssrc
    std::set<uint32_t> m_recvSsrcSet; //ssrc for multiple stream
    std::map<uint32_t, uint32_t> m_recvSeq;
    std::map<uint32_t, uint32_t> m_lost;
    std::map<uint32_t, uint32_t> m_cumLost;
    std::map<uint32_t, uint32_t> m_recvPackets;
    std::map<uint32_t, uint32_t> m_lastSrRecvTime;
    bool m_receiving;


    //Log
    double m_pDelay;
    double m_recvDataBytes;
    double m_recvAllBytes;
    ns3::Time m_lastThroCheckTime;
    ns3::Time m_lastDelayCheckTime;

};

}

#endif /* GCC_NODE_H */
