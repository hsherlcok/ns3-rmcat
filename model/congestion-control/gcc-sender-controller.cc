/******************************************************************************
 * q
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
 * Gcc Sender Side controller (Loss Based Congestion Control) implementation for gcc ns3module.
 *
 * @version 0.1.0
 */
#include "gcc-sender-controller.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/object.h"

NS_LOG_COMPONENT_DEFINE("GccSenderController");


namespace rmcat {
const int kDefaultMaxBitrateBps = 1000000000;
const int64_t kLowBitrateLogPeriodMs = 10000;
const int kMinBitrateBps = 10000;
const int kMaxBitrateBps = 1500000;
const int kInitialBitrateBps = 300000;

const int64_t kBweIncreaseIntervalMs = 1000;                                    
const int64_t kBweDecreaseIntervalMs = 300;                                     
const int64_t kStartPhaseMs = 2000;                                             
const int64_t kBweConverganceTimeMs = 20000;                                    
const int kLimitNumPackets = 20;                                                
const int64_t kRtcEventLogPeriodMs = 5000;                                      
const int64_t kFeedbackIntervalMs = 5000;                                       
const int64_t kFeedbackTimeoutIntervals = 3;                                    
const int64_t kTimeoutIntervalMs = 1000;                                        
                                                                                
const float kDefaultLowLossThreshold = 0.02f;                                   
const float kDefaultHighLossThreshold = 0.1f;                                   
const int kDefaultBitrateThresholdKbps = 0;                                     



GccSenderController::GccSenderController() :

    m_lastTimeCalcValid{false},
    m_ploss{0},
    m_plr{0.f},
    
    min_configured_bitrate_bps_{150000},
    max_configured_bitrate_bps_{1500000},
    
    current_bitrate_bps_{0},  //uint32_t m_sending_rate -> current_bitrate_bps_
    remb_bitrate_{0},                                   //uint32_t
    
    min_bitrate_configured_{min_configured_bitrate_bps_},
    max_bitrate_configured_{max_configured_bitrate_bps_},

	last_feedback_ms_{-1},
	first_report_time_ms_{-1},
    lost_packets_since_last_loss_update_{0},
    expected_packets_since_last_loss_update_{0},
    has_decreased_since_last_fraction_loss_{false},
    last_packet_report_ms_{-1},
    time_last_decrease_ms_{0},
    rttMs{0},   
    last_round_trip_time_ms_{0},
    last_fraction_loss_{0},

    m_Bitrate_valid_{false}
    
    {

    }

  


GccSenderController::~GccSenderController() {}

/*
void GccSenderController::setCurrentBw(float newBw) {
    m_initBw = newBw;
}
*/

void GccSenderController::reset() {
    m_lastTimeCalcValid = false;

    m_ploss = 0;
    m_plr = 0.f;
}

uint32_t GccSenderController::getBitrate() {
    return current_bitrate_bps_;
}

void GccSenderController::processBye(uint32_t ssrc){
    last_report_blocks_.erase(ssrc);
}

void GccSenderController::SetSendBitrate(int bitrate) {

    CapBitrate(bitrate);

}

void GccSenderController::SetMinMaxBitrate(int min_bitrate, int max_bitrate) {

    min_bitrate_configured_ = std::max(min_bitrate, (int)min_bitrate_configured_);

    if(max_bitrate > 0) {
	    max_bitrate_configured_ = std::max<uint32_t>((int)min_bitrate_configured_, max_bitrate);
    } else {
	    max_bitrate_configured_ = kDefaultMaxBitrateBps;
    }

}

void GccSenderController::ApplyReceiverEstimatedBitrate(uint32_t Received_Estimated_Bitrate) {
    // TODO Called when the REMB messages are received from receiver.
    // Calculate proper m_sending_rate_ according to delay-based estimated bitrate.
    
    if (!m_Bitrate_valid_){
        // First ApplyDelayBasedBitrate called.
        // We need to initialize m_sending_rate_ and min, max configured bitrate.

        m_Bitrate_valid_ = true;

	    SetMinMaxBitrate(kMinBitrateBps, kMaxBitrateBps);
	    SetSendBitrate(kInitialBitrateBps);

    }
   
    remb_bitrate_ = Received_Estimated_Bitrate;
    NS_LOG_INFO( ns3::Simulator::Now().ToDouble(ns3::Time::S) << " GccSenderController::ApplyReceiverEstimatedBitrate::remb bitrate :  " << remb_bitrate_);    
  
    CapBitrate(current_bitrate_bps_);
}


void GccSenderController::ApplyLossBasedBitrate(const std::vector<ns3::GccRtcpHeader::RecvReportBlock>& report_blocks,
                                                int64_t nowMs) {
    // TODO Loss Based Controller Implementation
    NS_LOG_INFO("m_Bitrate_valid_ : " << m_Bitrate_valid_);
    if (!m_Bitrate_valid_){                                                                    
         // First ApplyDelayBasedBitrate called.                                                
         // We need to initialize m_sending_rate_ and min, max configured bitrate.              
                                                                                                
         m_Bitrate_valid_ = true;                                                               
                                                                                                
         SetMinMaxBitrate(kMinBitrateBps, kMaxBitrateBps);                                      
         SetSendBitrate(kInitialBitrateBps);                                                    
     }         
                                                                                  
    OnReceivedRtcpReceiverReportBlocks(report_blocks, nowMs);
	
    if (rttMs > 0 )
        last_round_trip_time_ms_ = rttMs;

   
}

void GccSenderController::OnReceivedRtcpReceiverReportBlocks(const std::vector<ns3::GccRtcpHeader::RecvReportBlock>& report_blocks, int64_t nowMs){
    
    if(report_blocks.empty())
        return;

    int total_packets_lost_delta = 0;
    int total_packets_delta = 0;
    
    for (const ns3::GccRtcpHeader::RecvReportBlock& report_block : report_blocks){
        auto it = last_report_blocks_.find(report_block.m_sourceSsrc);
        if (it != last_report_blocks_.end()) {
            auto number_of_packets = report_block.m_highestSeqNum -
                         it->second.m_highestSeqNum;   
            total_packets_delta += number_of_packets;                               
            auto lost_delta = report_block.m_cumNumLost - it->second.m_cumNumLost;  
            total_packets_lost_delta += lost_delta;                 
        }
        rttMs = nowMs - report_block.m_lastSRTime - report_block.m_SRDelay;
        last_report_blocks_[report_block.m_sourceSsrc] = report_block;                
    }

    if (!total_packets_delta)                                                      
        return;                                                                      
    int packets_received_delta = total_packets_delta - total_packets_lost_delta;   
    // To detect lost packets, at least one packet has to be received. This check  
    // is needed to avoid bandwith detection update in                             
    // VideoSendStreamTest.SuspendBelowMinBitrate                                  
                                                                               
    if (packets_received_delta < 1)                                                
        return;                                                                      
    
    NS_LOG_INFO( ns3::Simulator::Now().ToDouble(ns3::Time::S) << " lost packets : " << total_packets_lost_delta << " , totla packets : " << total_packets_delta);    
 
    UpdatePacketsLost(total_packets_lost_delta, total_packets_lost_delta + packets_received_delta, nowMs);      

}

void GccSenderController::UpdatePacketsLost(int packets_lost, int number_of_packets, int64_t nowMs){
    last_feedback_ms_ = nowMs;                                              
    if (first_report_time_ms_ == -1)                                         
        first_report_time_ms_ = nowMs;                                        
    NS_LOG_INFO("SEND : " << first_report_time_ms_);                                               
    // Check sequence number diff and weight loss report                     
    if (number_of_packets > 0) {                                             
        // Accumulate reports.                                                 
        lost_packets_since_last_loss_update_ += packets_lost;                  
        expected_packets_since_last_loss_update_ += number_of_packets;         
    }                                                                     
    // Don't generate a loss rate until it can be based on enough packets. 
    if (expected_packets_since_last_loss_update_ < kLimitNumPackets)       
        return;                                                              
                                                                         
    has_decreased_since_last_fraction_loss_ = false;                       
    int64_t lost_q8 = lost_packets_since_last_loss_update_ << 8;           
    int64_t expected = expected_packets_since_last_loss_update_;           
    last_fraction_loss_ = std::min<int>(lost_q8 / expected, 255);       

    NS_LOG_INFO( ns3::Simulator::Now().ToDouble(ns3::Time::S) << " GccSenderController::UpdatePacketsLost loss ratio : " << last_fraction_loss_/256.0f); 
                                                                         
    // Reset accumulators.                                                 
                                                                         
    lost_packets_since_last_loss_update_ = 0;                              
    expected_packets_since_last_loss_update_ = 0;                          
    last_packet_report_ms_ = nowMs;                                       
    UpdateEstimate(nowMs);                                                       

}

bool GccSenderController::IsInStartPhase(int64_t nowMs) {
    return first_report_time_ms_ == -1 ||
        nowMs - first_report_time_ms_ < kStartPhaseMs;
}

void GccSenderController::UpdateEstimate(int64_t nowMs){
    uint32_t new_bitrate = current_bitrate_bps_;

    if(last_fraction_loss_ == 0 && IsInStartPhase(nowMs)) {
		NS_LOG_INFO("SENDER new bitrate1 : " << new_bitrate << " " << remb_bitrate_ << " " << current_bitrate_bps_);
        new_bitrate = std::max(remb_bitrate_, new_bitrate);           
		NS_LOG_INFO("SENDER new bitrate2 : " << new_bitrate << " " << remb_bitrate_ << " " << current_bitrate_bps_);
        if (new_bitrate != current_bitrate_bps_) {                    
            min_bitrate_history_.clear();                               
            min_bitrate_history_.push_back(std::make_pair(nowMs, current_bitrate_bps_));          
            CapBitrate(new_bitrate);                
            return;                                                     
        }                                                             
    }
    UpdateMinHistory(nowMs);                                                        
	if (last_packet_report_ms_ == -1) {                                              
		// No feedback received.                                                       
		CapBitrate(current_bitrate_bps_);                          
		return;                                                                        
	}                                                                                
	int64_t time_since_packet_report_ms = nowMs - last_packet_report_ms_;           
	int64_t time_since_feedback_ms = nowMs - last_feedback_ms_;                     
	if (time_since_packet_report_ms < 1.2 * kFeedbackIntervalMs) {                   
		
        float loss = last_fraction_loss_ / 256.0f;                                     
		
        if (current_bitrate_bps_ < kDefaultBitrateThresholdKbps ||                           
				loss <= kDefaultLowLossThreshold) {                                             
			// Loss < 2%: Increase rate by 8% of the min bitrate in the last             
			// kBweIncreaseIntervalMs.                                                   
			// Note that by remembering the bitrate over the last second one can         
			// rampup up one second faster than if only allowed to start ramping         
			// at 8% per second rate now. E.g.:                                          
			//   If sending a constant 100kbps it can rampup immediatly to 108kbps       
			//   whenever a receiver report is received with lower packet loss.          
			//   If instead one would do: current_bitrate_bps_ *= 1.08^(delta time),     
			//   it would take over one second since the lower packet loss to achieve    
			//   108kbps.                     
                                           
			new_bitrate = static_cast<uint32_t>(                                         
					min_bitrate_history_.front().second * 1.08 + 0.5);                       

			new_bitrate += 1000;                                                         
		
        } else if (current_bitrate_bps_ > kDefaultBitrateThresholdKbps) {                    
			if (loss <= kDefaultHighLossThreshold) {                                          
				// Loss between 2% - 10%: Do nothing.                                      
			} else {                                                                     
				// Loss > 10%: Limit the rate decreases to once a kBweDecreaseIntervalMs   
				// + rtt.                                                                  
				if (!has_decreased_since_last_fraction_loss_ &&                            
						(nowMs - time_last_decrease_ms_) >=    //here                               
						(kBweDecreaseIntervalMs + last_round_trip_time_ms_)) {             
					time_last_decrease_ms_ = nowMs;                                         
					// Reduce rate:                                                       
					//   newRate = rate * (1 - 0.5*lossRate);                             
					//   where packetLoss = 256*lossRate;                                 
					new_bitrate = static_cast<uint32_t>(                                  
							(current_bitrate_bps_ *                                           
							 static_cast<double>(512 - last_fraction_loss_)) /                
							512.0);                                                           
					has_decreased_since_last_fraction_loss_ = true;                       
				}                                                                       
			}                                                                         
		}                                                                           
	} 
    /*else if (time_since_feedback_ms >                                           
			kFeedbackTimeoutIntervals * kFeedbackIntervalMs &&             
			(last_timeout_ms_ == -1 ||                                         
			 now_ms - last_timeout_ms_ > kTimeoutIntervalMs)) {                
		if (in_timeout_experiment_) {                                               
			new_bitrate *= 0.8;                                                       
			// Reset accumulators since we've already acted on missing feedback and   
			// shouldn't to act again on these old lost packets.                      
			lost_packets_since_last_loss_update_ = 0;                                 
			expected_packets_since_last_loss_update_ = 0;                             
			last_timeout_ms_ = now_ms;                                                
		}                                                                           
	} */                                                                            
    NS_LOG_INFO( ns3::Simulator::Now().ToDouble(ns3::Time::S) << "GccSenderController::UpdateEstimated::loss based bitrate : " << new_bitrate);

	CapBitrate(new_bitrate);                                      
}

void GccSenderController::UpdateMinHistory(int64_t nowMs) {           
  // Remove old data points from history.                                      
  // Since history precision is in ms, add one so it is able to increase       
  // bitrate if it is off by as little as 0.5ms.                               
    while (!min_bitrate_history_.empty() &&                                      
           nowMs - min_bitrate_history_.front().first + 1 >                     
           kBweIncreaseIntervalMs) {                                         
        min_bitrate_history_.pop_front();                                          
    }                                                                            
                                                                               
  // Typical minimum sliding-window algorithm: Pop values higher than current  
  // bitrate before pushing it.                                                
    while (!min_bitrate_history_.empty() &&                                      
           current_bitrate_bps_ <= min_bitrate_history_.back().second) {         
        min_bitrate_history_.pop_back();                                           
    }                                                                            
                                                                               
    min_bitrate_history_.push_back(std::make_pair(nowMs, current_bitrate_bps_));
}                                                                              


void GccSenderController::CapBitrate(uint32_t bitrate_bps){
    if(remb_bitrate_ > 0 && bitrate_bps > remb_bitrate_){
		bitrate_bps = remb_bitrate_;
    }
    
    if(bitrate_bps > max_bitrate_configured_){
        bitrate_bps = max_bitrate_configured_;
    }

    if(bitrate_bps < min_bitrate_configured_){
        bitrate_bps = min_bitrate_configured_;
    }
    
    current_bitrate_bps_ = bitrate_bps;
    NS_LOG_INFO("RECV current_bitrate_bps " << current_bitrate_bps_);
}



}
