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
 * Dummy controller (CBR) interface for rmcat ns3 module.
 *
 * @version 0.1.1
 * @author Jiantao Fu
 * @author Sergio Mena
 * @author Xiaoqing Zhu
 */


#include <ns3/gfp-header.h>
#include <iostream>   
#include <vector>       
#include <sstream>    
#include <cassert>   
#include <map> 
#include <utility>
#include <deque>
#include <algorithm>

#ifndef GCC_SENDER_CONTROLLER_H
#define GCC_SENDER_CONTROLLER_H


namespace rmcat {

/**
 * Simplistic implementation of a sender-based congestion controller. The
 * algorithm simply returns a constant, hard-coded bandwidth when queried.
 */
class GccSenderController
{
public:
    /** Class constructor */
     GccSenderController();

    /** Class destructor */
    ~GccSenderController();

    /**
     * Set the current bandwidth estimation. This can be useful in test environments
     * to temporarily disrupt the current bandwidth estimation
     *
     * @param [in] newBw Bandwidth estimation to overwrite the current estimation
     */
    // virtual void setCurrentBw(float newBw);

    /**
     * Reset the internal state of the congestion controller
     */
    void reset();

    /**
     * Simplistic implementation of feedback packet processing. It simply
     * prints calculated metrics at regular intervals
     */
    void ApplyLossBasedBitrate(const std::vector<ns3::GccRtcpHeader::RecvReportBlock>& report_blocks, int64_t nowMs);
    
    void ApplyReceiverEstimatedBitrate(uint32_t Received_Estimated_Bitrate);

    void OnReceivedRtcpReceiverReportBlocks(const std::vector<ns3::GccRtcpHeader::RecvReportBlock>& report_blocks, int64_t nowMs);
 
    void UpdatePacketsLost(int packet_lost, int number_of_packets, int64_t nowMs);

    bool IsInStartPhase(int64_t nowMs);

    void UpdateEstimate(int64_t nowMs);

    void UpdateMinHistory(int64_t nowMs);

    void CapBitrate(uint32_t bitrate_bps);
    /**
     * Simplistic implementation of bandwidth getter. It returns a hard-coded
     * bandwidth value in bits per second
     */

    uint32_t getBitrate();

    void processBye(uint32_t ssrc);

private:

    void SetSendBitrate(int bitrate);
    void SetMinMaxBitrate(int min_bitrate, int max_bitrate);

    bool m_lastTimeCalcValid;
    
    uint32_t m_ploss;  /**< packet loss count within configured window */
    float m_plr;       /**< packet loss ratio within packet history window */

    uint32_t min_configured_bitrate_bps_;
    uint32_t max_configured_bitrate_bps_;

    uint32_t current_bitrate_bps_;  /* Estimated Sending Bps*/
    uint32_t remb_bitrate_;

    uint32_t min_bitrate_configured_;
    uint32_t max_bitrate_configured_;
    
    std::map<uint32_t, ns3::GccRtcpHeader::RecvReportBlock> last_report_blocks_;
    
    int64_t last_feedback_ms_; 
    int64_t first_report_time_ms_;
    int lost_packets_since_last_loss_update_;
    int expected_packets_since_last_loss_update_;
    bool has_decreased_since_last_fraction_loss_;
    int64_t last_packet_report_ms_;
    int64_t time_last_decrease_ms_;  
    int64_t rttMs;   
    int64_t last_round_trip_time_ms_;
    uint8_t last_fraction_loss_; 

    std::deque<std::pair<int64_t, uint32_t> > min_bitrate_history_;

    bool m_Bitrate_valid_;
};

}
#endif /* GCC_SENDER_CONTROLLER_H */
