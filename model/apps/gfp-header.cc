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

#include "gfp-header.h"
#include "ns3/log.h"

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (GccRtcpHeader);
NS_OBJECT_ENSURE_REGISTERED (RembHeader);

NS_LOG_COMPONENT_DEFINE ("GfpHeader");

void GfpHdrSetBit (uint8_t& val, uint8_t pos, bool bit)
{
    NS_ASSERT (pos < 8);
    if (bit) {
        val |= (1u << pos);
    } else {
        val &= (~(1u << pos));
    }
}

bool GfpHdrGetBit (uint8_t val, uint8_t pos)
{
    return bool (val & (1u << pos));
}

GccRtcpHeader::GccRtcpHeader ()
: Header{}
, m_padding{false}
, m_typeOrCnt{0}
, m_packetType{0}
, m_length{1} //1 for ssrc of sender
, m_sendSsrc{0}
{}

GccRtcpHeader::GccRtcpHeader (uint8_t packetType)
: Header{}
, m_padding{false}
, m_typeOrCnt{0}
, m_packetType{packetType}
, m_length{1} //1 for ssrc of sender
, m_sendSsrc{0}
{}

GccRtcpHeader::GccRtcpHeader (uint8_t packetType, uint8_t subType)
: Header{}
, m_padding{false}
, m_typeOrCnt{subType}
, m_packetType{packetType}
, m_length{1} //1 for ssrc of sender
, m_sendSsrc{0}
{
    NS_ASSERT (subType <= 0x1f);
}

GccRtcpHeader::~GccRtcpHeader () {}

void GccRtcpHeader::Clear ()
{
    m_padding = false;
    m_typeOrCnt = 0;
    m_packetType = 0;
    m_length = 1;
    m_sendSsrc = 0;
}

TypeId GccRtcpHeader::GetTypeId ()
{
    static TypeId tid = TypeId ("GccRtcpHeader")
      .SetParent<Header> ()
      .AddConstructor<GccRtcpHeader> ()
    ;
    return tid;
}

TypeId GccRtcpHeader::GetInstanceTypeId () const
{
    return GetTypeId ();
}

uint32_t GccRtcpHeader::GetSerializedSize () const
{
    return 1 + sizeof (m_packetType) + sizeof (m_length) + m_length*4;
}

void GccRtcpHeader::SerializeCommon (Buffer::Iterator& start) const
{
    NS_ASSERT (m_typeOrCnt <= 0x1f);
    uint8_t octet1 = 0;
    octet1 |= (GCC_RTP_VERSION << 6);
    GfpHdrSetBit (octet1, 5, m_padding);
    octet1 |= uint8_t (m_typeOrCnt & 0x1f);
    start.WriteU8 (octet1);

    start.WriteU8 (m_packetType);
    start.WriteHtonU16 (m_length);
    start.WriteHtonU32 (m_sendSsrc);
}

uint32_t GccRtcpHeader::DeserializeCommon (Buffer::Iterator& start)
{
    const auto octet1 = start.ReadU8 ();
    const uint8_t version = (octet1 >> 6);
    m_padding = GfpHdrGetBit (octet1, 5);
    m_typeOrCnt = octet1 & 0x1f;

    m_packetType = start.ReadU8 ();
    m_length = start.ReadNtohU16 ();
    m_sendSsrc = start.ReadNtohU32 ();
    NS_ASSERT (version == GCC_RTP_VERSION);

    return GetSerializedSize ();

}

void GccRtcpHeader::Serialize (Buffer::Iterator start) const
{
    NS_ASSERT (m_length >= 1);
    SerializeCommon (start);
    NS_LOG_INFO("GccRtcpHeader::Serialize");
    
    if(m_packetType == RTCP_SR)
    {
      start.WriteHtonU32 (m_sendReportBlock.m_mostSigNTP);
      start.WriteHtonU32 (m_sendReportBlock.m_leastSigNTP);
      start.WriteHtonU32 (m_sendReportBlock.m_rtpTimeStamp);
      start.WriteHtonU32 (m_sendReportBlock.m_packetCnt);
      start.WriteHtonU32 (m_sendReportBlock.m_octCnt);
    }

    if(m_packetType == RTCP_SR || m_packetType == RTCP_RR)
    {
      for(const auto& rrb : m_recvReportBlocks)
      {
        start.WriteHtonU32 (rrb.m_sourceSsrc);
        start.WriteU8 (rrb.m_fractionLost);
        
        uint8_t octet1 = 0;
        uint16_t octet2 = 0; //named 'octet' but... it is 16bit.
        NS_ASSERT(rrb.m_cumNumLost <= 0xffffff); //24bit
        octet1 |= uint8_t(rrb.m_cumNumLost >> 16); //8bit
        octet2 |= uint16_t(rrb.m_cumNumLost & 0xffff); //16bit
        start.WriteU8 (octet1);
        start.WriteHtonU16 (octet2);
        
        start.WriteHtonU32(rrb.m_highestSeqNum);
        start.WriteHtonU32(rrb.m_interArrivalJitter);
        start.WriteHtonU32(rrb.m_lastSRTime);
        start.WriteHtonU32(rrb.m_SRDelay);
      }
    }

    if(m_packetType == RTCP_SR || m_packetType == RTCP_RR || m_packetType == RTCP_BYE)
    {
      for(const auto& bb : m_byeBlock.m_ssrcSet)
      {
        start.WriteHtonU32 (bb);
      }
    }

    if(!(m_packetType == RTCP_SR || m_packetType == RTCP_RR || m_packetType == RTCP_BYE))
      NS_LOG_ERROR("RTCP Packet Type ERROR! (Serialize)");
}

uint32_t GccRtcpHeader::Deserialize (Buffer::Iterator start)
{
    DeserializeCommon(start);
    NS_LOG_INFO("GccRtcpHeader::Deserialize");
    size_t len = m_length-1; //m_sendSsrc

    if(m_packetType == RTCP_SR)
    {
      m_sendReportBlock.m_mostSigNTP = start.ReadNtohU32();
      m_sendReportBlock.m_leastSigNTP = start.ReadNtohU32();
      m_sendReportBlock.m_rtpTimeStamp = start.ReadNtohU32();
      m_sendReportBlock.m_packetCnt  = start.ReadNtohU32();
      m_sendReportBlock.m_octCnt = start.ReadNtohU32();

      len -= 5; //SendReportBlock
    }
    
    if(m_packetType == RTCP_SR || m_packetType == RTCP_RR)
    {
      for(uint32_t i=0;i<m_typeOrCnt;i++)
      {
        NS_ASSERT (len >= 6);
        RecvReportBlock rrb;
        rrb.m_sourceSsrc = start.ReadNtohU32();
        rrb.m_fractionLost = start.ReadU8();

        uint8_t octet1 = start.ReadU8();
        uint16_t octet2 = start.ReadNtohU16(); //16bit... but... named octet for convenience
        uint32_t cumLost = (uint32_t (octet1)<<16);
        cumLost |= uint32_t(octet2);
        rrb.m_cumNumLost = cumLost;

        rrb.m_highestSeqNum = start.ReadNtohU32();
        rrb.m_interArrivalJitter = start.ReadNtohU32();
        rrb.m_lastSRTime = start.ReadNtohU32();
        rrb.m_SRDelay = start.ReadNtohU32();

        m_recvReportBlocks.push_back(rrb);
        len -= 6;
      }
    }

    if(m_packetType == RTCP_SR || m_packetType == RTCP_RR || m_packetType == RTCP_BYE)
    {
      while(len > 0) //same as "for(i=0;i<m_typeOrCnt;i++)" in case of RTCP_BYE only
      {
        NS_ASSERT(len>=1);
        m_byeBlock.m_ssrcSet.insert(start.ReadNtohU32());

        len--;
      }
    }
    
    NS_ASSERT(len == 0);

    return GetSerializedSize();
}

GccRtcpHeader::RejectReason
GccRtcpHeader::AddRRFeedback (std::vector<RecvReportBlock>& rrbs)
{
  if(m_packetType == RTCP_RR || m_packetType == RTCP_SR)
  {
    if(rrbs.size() < MAX_RB_NUM)
    {
      m_recvReportBlocks = rrbs;
      if(!UpdateRRLength())
        NS_LOG_ERROR("Failed to update RR RTCP Feedback Length");
    }
    else
      return RTCP_TOO_LONG;
  }
  else
    return RTCP_TYPE_ERR;
    
  return RTCP_NONE;
}

GccRtcpHeader::RejectReason
GccRtcpHeader::AddSRFeedback (SendReportBlock& srb)
{
  if(m_packetType == RTCP_SR)
  {
    m_sendReportBlock = srb;
    if(!UpdateSRLength())
      NS_LOG_ERROR("Failed to update SR RTCP Feedback Length");
  }
  else
    return RTCP_TYPE_ERR;
    
  return RTCP_NONE;
}

GccRtcpHeader::RejectReason
GccRtcpHeader::AddByeFeedback(std::set<uint32_t>& ssrcSet)
{
  if(m_packetType == RTCP_BYE || m_packetType == RTCP_SR || m_packetType == RTCP_RR)
  {
    NS_ASSERT(ssrcSet.size() >= 1);

    if(m_packetType == RTCP_BYE)
    {
      auto it = ssrcSet.begin();
      m_sendSsrc = *it; //bye packet doesn't need to use m_sendSsrc, so we store first element of ssrcSet in m_sendSsrc
      ssrcSet.erase(it);
    }

    m_byeBlock.m_ssrcSet = ssrcSet;
    
    if(!UpdateByeLength())
      NS_LOG_ERROR("Failed to update Bye RTCP Feedback Length");
  }
  else
    return RTCP_TYPE_ERR;

  return RTCP_NONE;
}

std::vector<GccRtcpHeader::RecvReportBlock> GccRtcpHeader::GetRRBs () const
{
    return m_recvReportBlocks;
}

GccRtcpHeader::SendReportBlock GccRtcpHeader::GetSRB () const
{
  return m_sendReportBlock;
}
GccRtcpHeader::ByeBlock GccRtcpHeader::GetBye() const
{
  ByeBlock ret = m_byeBlock;

  if(m_packetType == RTCP_BYE)
    ret.m_ssrcSet.insert(m_sendSsrc); //insert m_sendSsrc

  return ret;
}

void GccRtcpHeader::PrintN (std::ostream& os) const
{
    os << "GccRtcp/Remb Common Header - version = " << int (GCC_RTP_VERSION)
       << ", padding = " << (m_padding ? "yes" : "no")
       << ", type/count = " << int (m_typeOrCnt)
       << ", packet type = " << int (m_packetType)
       << ", length = " << m_length
       << ", ssrc of RTCP sender = " << m_sendSsrc;
}
void GccRtcpHeader::Print (std::ostream& os) const
{
    PrintN (os);
    os << std::endl;
}

bool GccRtcpHeader::IsPadding () const
{
    return m_padding;
}

void GccRtcpHeader::SetPadding (bool padding)
{
    m_padding = padding;
}

uint8_t GccRtcpHeader::GetTypeOrCount () const
{
    return m_typeOrCnt;
}

void GccRtcpHeader::SetTypeOrCount (uint8_t typeOrCnt)
{
    m_typeOrCnt = typeOrCnt;
}

uint8_t GccRtcpHeader::GetPacketType () const
{
    return m_packetType;
}

void GccRtcpHeader::SetPacketType (uint8_t packetType)
{
    m_packetType = packetType;
}

uint32_t GccRtcpHeader::GetSendSsrc () const
{
    return m_sendSsrc;
}

void GccRtcpHeader::SetSendSsrc (uint32_t sendSsrc)
{
    m_sendSsrc = sendSsrc;
}

bool GccRtcpHeader::UpdateRRLength ()
{
    size_t len = m_length;
    len += m_recvReportBlocks.size()*6; //24bytes per block
    
    if (len > 0xffff) {
        return false;
    }
    
    m_length = len;
    return true;
}

bool GccRtcpHeader::UpdateSRLength ()
{
    size_t len = m_length;
    len += 5; //20 bytes per block
    
    if (len > 0xffff) {
        return false;
    }
    
    m_length = len;
    return true;
}


bool GccRtcpHeader::UpdateByeLength ()
{
    size_t len = m_length;
    len += m_byeBlock.m_ssrcSet.size();
    
    if (len > 0xffff) {
        return false;
    }
    
    m_length = len;
    return true;
}


RembHeader::RembHeader ()
: GccRtcpHeader{}
{}

RembHeader::RembHeader (uint8_t packetType)
: GccRtcpHeader{packetType}
{
}

RembHeader::RembHeader (uint8_t packetType, uint8_t subType)
: GccRtcpHeader{packetType,subType}
{
    NS_ASSERT (subType <= 0x1f);
}

RembHeader::~RembHeader () {}

TypeId RembHeader::GetTypeId ()
{
    static TypeId tid = TypeId ("RembHeader")
      .SetParent<GccRtcpHeader> ()
      .AddConstructor<RembHeader> ()
    ;
    return tid;
}

TypeId RembHeader::GetInstanceTypeId () const
{
    return GetTypeId ();
}

uint32_t RembHeader::GetSerializedSize () const
{
    return 1 + sizeof (m_packetType) + sizeof (m_length) + m_length*4;
}


RembHeader::RejectReason 
RembHeader::AddRembFeedback (uint32_t bitrate, std::set<uint32_t>& slist)
{
  if(m_packetType != RTP_REMB)
    NS_LOG_ERROR("REMB Packet Type Error!");

  const uint32_t kMaxMantissa = 0x3ffff;  // 18 bits.
  uint64_t mantissa = bitrate;
  uint8_t exponenta = 0;

  while (mantissa > kMaxMantissa) {
    mantissa >>= 1;
    ++exponenta;
  }

  m_rembBlock.m_mantisa = uint32_t(mantissa);
  m_rembBlock.m_exp = exponenta;
  m_rembBlock.m_bitrate = bitrate;

  m_rembBlock.m_ssrcs = slist;
  m_rembBlock.m_numSsrc = slist.size();

  if(!UpdateRembLength())
    NS_LOG_ERROR("Failed to update REMB Feedback Length");

  return RTCP_NONE;
}

void RembHeader::Serialize (Buffer::Iterator start) const
{
    NS_LOG_INFO("RembHeader::Serialize");
    NS_ASSERT (m_length > 1);
    SerializeCommon (start);

    if(m_packetType == RTP_REMB)
    {
      start.WriteHtonU32 (m_rembBlock.m_unused);
      start.WriteHtonU32 (m_rembBlock.m_id);
      start.WriteU8 (m_rembBlock.m_numSsrc);

      NS_ASSERT(m_rembBlock.m_exp <= 0x3f);
      NS_ASSERT(m_rembBlock.m_mantisa <= 0x3ffff);

      NS_LOG_INFO("REMB Serial ssrc : "<<m_sendSsrc<<" " <<m_rembBlock.m_exp<<" "<<m_rembBlock.m_mantisa);

      uint8_t octet1 = uint8_t(m_rembBlock.m_exp<<2);
      octet1 |= uint8_t(m_rembBlock.m_mantisa >> 16);
      start.WriteU8(octet1);
      start.WriteHtonU16 (uint16_t(m_rembBlock.m_mantisa & 0xffff));

      for(const auto& s : m_rembBlock.m_ssrcs)
        start.WriteHtonU32 (s);
    }
    else
      NS_LOG_ERROR("REMB Packet Type ERROR! (Serialize)");
}

uint32_t RembHeader::Deserialize (ns3::Buffer::Iterator start)
{
    NS_LOG_INFO("RembHeader::Deserialize");
    GccRtcpHeader::DeserializeCommon (start);

    size_t len = m_length-1; //m_sendSsrc

    if(m_packetType == RTP_REMB)
    {
      m_rembBlock.m_unused = start.ReadNtohU32();
      m_rembBlock.m_id = start.ReadNtohU32();
      m_rembBlock.m_numSsrc = start.ReadU8();

      uint8_t octet1 = start.ReadU8();
      m_rembBlock.m_exp = uint8_t(octet1>>2);
      m_rembBlock.m_mantisa = (uint32_t(octet1)<<30) >> 14;
      m_rembBlock.m_mantisa |= (uint32_t(start.ReadNtohU16()) & 0xffff);
      
      NS_LOG_INFO("REMB Deserial ssrc : "<<m_sendSsrc<<" " <<m_rembBlock.m_exp<<" "<<m_rembBlock.m_mantisa);

      len -= 3;

      while(len > 0)
      {
        NS_ASSERT (len >= 1);
        m_rembBlock.m_ssrcs.insert(start.ReadNtohU32());
        len --;
      }
    }

    NS_ASSERT(len == 0);
  
    /* calculate bitrate --- not deserialization*/
    m_rembBlock.m_bitrate = m_rembBlock.m_mantisa;

    for(uint32_t i=0;i<m_rembBlock.m_exp;i++)
      m_rembBlock.m_bitrate <<= 1;

    return GetSerializedSize();
}


bool RembHeader::UpdateRembLength()
{
    size_t len = m_length;
    len += 3 /*unused+id+numSsrc+exp+mantisa*/ + m_rembBlock.m_numSsrc;
    
    if (len > 0xffff) {
        return false;
    }
    
    m_length = len;
    return true;
}


RembHeader::RembBlock
RembHeader::GetRembBlock() const
{
  return m_rembBlock;
}
}
