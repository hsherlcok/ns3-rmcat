/******************************************************************************
 * Copyright 2016-2018 Cisco Systems, Inc.                                    *
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

#ifndef GFP_HEADER_H
#define GFP_HEADER_H

#include "ns3/header.h"
#include "ns3/type-id.h"
#include <map>
#include <vector>
#include <set>

namespace ns3 {

void GfpHdrSetBit (uint8_t& val, uint8_t pos, bool bit);
bool GfpHdrGetBit (uint8_t val, uint8_t pos);

const uint8_t GCC_RTP_VERSION = 2; //From RFC

// From RFC 3550, RTP: A Transport Protocol for Real-Time Applications.
//
// RTCP Header (RFC 3550).

// RTCP Receive Report (RR) Block
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |V=2|P| Type/Cnt|    PT (201)   |          length               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                 SSRC of RTCP packet sender                    |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  0 |                 SSRC_1 (SSRC of first source)                 |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 | fraction lost |       cumulative number of packets lost       |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |           extended highest sequence number received           |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 12 |                      interarrival jitter                      |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 16 |                         last SR (LSR)                         |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 20 |                   delay since last SR (DLSR)                  |
// 24 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//    .                                                               .
//    .                                                               .
//    .                                                               .
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                   SSRC of nth RTP Stream                      |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// RTCP Sender Report (SR) Block
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |V=2|P| Type/Cnt|    PT (200)   |          length               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  0 |                         SSRC of sender                        |
//    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  4 |              NTP timestamp, most significant word             |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |             NTP timestamp, least significant word             |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 12 |                         RTP timestamp                         |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 16 |                     sender's packet count                     |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 20 |                      sender's octet count                     |
//    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// 24 |                 SSRC_1 (SSRC of first source)                 | RR Report
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 28 | fraction lost |       cumulative number of packets lost       |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 32 |           extended highest sequence number received           |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 36 |                      interarrival jitter                      |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 40 |                         last SR (LSR)                         |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 44 |                   delay since last SR (DLSR)                  |
// 48 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//    .                                                               .
//    .                                                               .
//    .                                                               .
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |                   SSRC of nth RTP Stream                      |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// RTCP Bye Packet (This packet indicates that one or more sources are no longer active.
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |V=2|P|    SC   |   PT=BYE=203  |             length            |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                           SSRC/CSRC                           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   :                              ...                              :
//   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//   (opt) |     length    |               reason for leaving            ...
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class GccRtcpHeader : public Header
{
public:
    enum PayloadType {
        RTCP_SMPTETC = 194,
        RTCP_IJ      = 195,
        RTCP_SR      = 200,
        RTCP_RR      = 201,
        RTCP_SDES    = 202,
        RTCP_BYE     = 203,
        RTCP_APP     = 204,
        RTP_FB       = 205,
        RTP_REMB     = 206,
        RTP_XR       = 207,
        RTP_RSI      = 209,
        RTP_TOKEN    = 210,
        RTP_IDMS     = 211,
        RTP_RSNM     = 213,
    };

    enum SdesType{
        RTCP_SDES_END   = 0,
        RTCP_SDES_CNAME = 1,
        RTCP_SDES_NAME  = 2,
        RTCP_SDES_EMAIL = 3,
        RTCP_SDES_PHONE = 4,
        RTCP_SDES_LOC   = 5,
        RTCP_SDES_TOOL  = 6,
        RTCP_SDES_NOTE  = 7,
        RTCP_SDES_PRIV  = 8,
        RTCP_SDES_APSI  = 10,
    };

    enum RtpFeedbackType {
        RTCP_RTPFB_GNACK  =  1,
        RTCP_RTPFB_TMMBR  =  3,
        RTCP_RTPFB_TMMBN  =  4,
        RTCP_RTPFB_SR_REQ =  5,
        RTCP_RTPFB_RAMS   =  6,
        RTCP_RTPFB_TLLEI  =  7,
        RTCP_RTPFB_ECN_FB =  8,
        RTCP_RTPFB_PR     =  9,
        RTCP_RTPFB_CC     = 15,  // TODO (deferred): Change to IANA-assigned value
    };
 
    enum RejectReason {
        RTCP_NONE,      /**< Feedback was added correctly */
        RTCP_TYPE_ERR, /**< incorrect packet type */
        RTCP_TOO_LONG,  /**< Adding this sequence number would make the packet too long */
    };

    enum WebRtcReportPara {
      MAX_RB_NUM = 31,  //from WebRTC
    };

public:
    class RecvReportBlock
    {
      public:
        uint32_t m_sourceSsrc;
        uint8_t m_fractionLost;
        uint32_t m_cumNumLost;
        uint32_t m_highestSeqNum;
        uint32_t m_interArrivalJitter=0; //In our codes, this is not used, so set 0.
        uint32_t m_lastSRTime;
        uint32_t m_SRDelay;
    };
    
    /*
     * Following Blocks are not affect performacne in our gcc simulator, so we set all value 0.
     */
    class SendReportBlock 
    {
      public:
      //  uint32_t m_sendSsrc; already included in RTCP header
        uint8_t m_mostSigNTP = 0;
        uint32_t m_leastSigNTP = 0;
        uint32_t m_rtpTimeStamp = 0;
        uint32_t m_packetCnt = 0;
        uint32_t m_octCnt = 0;
    };

    //Bye Block (at the end of sending)
    class ByeBlock
    {
      public:
        std::set<uint32_t> m_ssrcSet;
    };

public:
    GccRtcpHeader ();
    GccRtcpHeader (uint8_t packetType);
    GccRtcpHeader (uint8_t packetType, uint8_t subType);
    virtual ~GccRtcpHeader ();
    virtual void Clear ();

    static ns3::TypeId GetTypeId ();
    virtual ns3::TypeId GetInstanceTypeId () const;
    virtual uint32_t GetSerializedSize () const;
    virtual void Serialize (ns3::Buffer::Iterator start) const;
    virtual uint32_t Deserialize (ns3::Buffer::Iterator start);
    virtual void Print (std::ostream& os) const;
    
    RejectReason AddRRFeedback (std::vector<RecvReportBlock>& rrbs);
    RejectReason AddSRFeedback (SendReportBlock& srb);
    RejectReason AddByeFeedback (std::set<uint32_t>& ssrcSet);

    bool IsPadding () const;
    void SetPadding (bool padding);
    uint8_t GetTypeOrCount () const;
    void SetTypeOrCount (uint8_t typeOrCnt);
    uint8_t GetPacketType () const;
    void SetPacketType (uint8_t packetType);
    uint32_t GetSendSsrc () const;
    void SetSendSsrc (uint32_t sendSsrc);
    std::vector<RecvReportBlock> GetRRBs () const;
    SendReportBlock GetSRB () const;
    ByeBlock GetBye() const;

private:
    bool UpdateRRLength ();
    bool UpdateSRLength ();
    bool UpdateByeLength ();

protected:
    void PrintN (std::ostream& os) const;
    void SerializeCommon (ns3::Buffer::Iterator& start) const;
    uint32_t DeserializeCommon (ns3::Buffer::Iterator& start);

    bool m_padding;
    uint8_t m_typeOrCnt;
    uint8_t m_packetType;
    uint16_t m_length;
    uint32_t m_sendSsrc; //only for RR

private:
    std::vector<RecvReportBlock> m_recvReportBlocks;
    SendReportBlock m_sendReportBlock;
    ByeBlock m_byeBlock;
};

// Receiver Estimated Max Bitrate (REMB) (draft-alvestrand-rmcat-remb).
// header~
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |V=2|P| FMT=15  |   PT=206      |             length            |
//    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  0 |                  SSRC of REMB packet sender                   |  
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 |                       Unused = 0                              |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |  Unique identifier 'R' 'E' 'M' 'B'                            |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 12 |  Num SSRC     | BR Exp    |  BR Mantissa                      |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 16 |   SSRC feedback                                               |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    :  ...                                                          : 

class RembHeader : public GccRtcpHeader
{

public:
    enum WebRtcRembPara {
      MAX_SSRCS_NUM = 0xff,  //from WebRTC
    };
public:

    class RembBlock {
      //uint32_t m_sendSrcc; already included in rtcp header
      public:
        uint32_t m_unused = 0;
        uint32_t m_id = 'R'+'E'+'M'+'B';
        uint8_t m_numSsrc;
        uint8_t m_exp;
        uint32_t m_mantisa;
        std::set<uint32_t> m_ssrcs;

        uint32_t m_bitrate; //not stored in header. just used in the siumlation...
    };

public:
    RembHeader ();
    RembHeader (uint8_t packetType);
    RembHeader (uint8_t packetType, uint8_t subType);
    virtual ~RembHeader ();
    
    static ns3::TypeId GetTypeId ();
    virtual ns3::TypeId GetInstanceTypeId () const;
    virtual uint32_t GetSerializedSize () const;
    virtual void Serialize (ns3::Buffer::Iterator start) const;
    virtual uint32_t Deserialize (ns3::Buffer::Iterator start);
    
    RejectReason AddRembFeedback (uint32_t bitrate, std::set<uint32_t>& slist);
    RembBlock GetRembBlock() const;
private:
    bool UpdateRembLength();

private:
    RembBlock m_rembBlock;
};

}

#endif /* RTP_HEADER_H */
