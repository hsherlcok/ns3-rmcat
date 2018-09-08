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
 * Gcc Receiver Side Controller (Delay Based Congestion Control) implementation for gcc ns3 module.
 *
 * @version 0.1.0
 */
#include "gcc-receiver-controller.h"
#include <sstream>
#include <cassert>
#include "ns3/log.h"
#include "ns3/simulator.h"

#define BURST_TIME 5

NS_LOG_COMPONENT_DEFINE("GccReceiverController");

namespace rmcat {

	GccRecvController::GccRecvController() :
		estimated_SendingBps_{0},      // Initialized Estimated Sending Bps TODO 

		m_lastTimeCalcValid{false},
		m_ploss{0},
		m_plr{0.f},
		m_RecvR{0.},

		n_loss{0},
		n_total_pkt{0},
		m_lossT{0}, 

		valid_pkt_sequence_{0},

		curr_group_num_{0},
		curr_group_sseq_{0},
		curr_group_stime_{0},
		curr_group_eseq_{0},
		curr_group_etime_arrival_{0},
		curr_group_etime_departure_{0},

		prev_group_sseq_{0},
		prev_group_stime_{0},
		prev_group_eseq_{0},
		prev_group_etime_arrival_{0},
		prev_group_etime_departure_{0},

		curr_group_size_{0},
		prev_group_size_{0},
		group_size_interval_{0},

		prev_pkt_seq_{0},
		prev_pkt_txTime_{0},
		prev_pkt_rxTime_{0},

		i_arrival_{0},
		i_departure_{0},
		i_delay_var_{0},

		m_group_changed_{false},
		m_first_packet_{false},

		num_of_deltas_{0},
		slope_{8.0/512.0},
		offset_{0},
		prev_offset_{0},
		E_{},
		process_noise_{},
		avg_noise_{0.0},
		var_noise_{50},
		ts_delta_hist_{},

		k_up_(0.0075), // default 0.0087 (in webRTC), 0.01(in Gcc Draft)
		k_down_(0.00018), //default 0.039 (in webRTC), 0.00018 (IN Gcc Draft)
		overusing_time_threshold_(150), //default 100 (in webRTC : same as propagation delay), 10 (in Gcc Draft)
		threshold_(12.5),
		last_threshold_update_ms_(-1),
		time_over_using_(-1),
		overuse_counter_(0),
		Hypothesis_('N'),

		min_configured_bitrate_bps_(10000), 
		max_configured_bitrate_bps_(30000000),                                  
		current_bitrate_bps_(max_configured_bitrate_bps_),                      
		latest_incoming_bitrate_bps_(current_bitrate_bps_),                     
		avg_max_bitrate_kbps_(-1.0f),                                           
		var_max_bitrate_kbps_(0.4f),                                            
		rate_control_state_('H'), //Hold mode                                           
		rate_control_region_('M'), //MaxUnkown                                   
		time_last_bitrate_change_(-1),                                          
		time_first_incoming_estimate_(-1),                                      
		bitrate_is_initialized_(false),                                         
		beta_(0.85f),                                     
		rtt_(200), //Initial Rtt can change                                         

		incoming_bitrate_(1000, 8000), //1000 is kBitrateWindowMs which is defined bwe_defines.h in webrtc folder   
		incoming_bitrate_initialized_(false)
		{
			E_[0][0] = 100;           
			E_[1][1] = 1e-1;          
			E_[0][1] = E_[1][0] = 0;  
			process_noise_[0] = 1e-13;
			process_noise_[1] = 1e-3; 

		}

	GccRecvController::~GccRecvController() {
		ts_delta_hist_.clear();
	}

	/*void GccRecvController::setCurrentBw(float newBw) {
	  m_initBw = newBw;
	  }
	  */

	void GccRecvController::reset() {
		m_lastTimeCalcValid = false;

		m_ploss = 0;
		m_plr = 0.f;
		m_RecvR = 0.;

		curr_group_num_ = 0;

	}

	uint32_t GccRecvController::GetBitrate() {
		NS_LOG_INFO("WOWW" << estimated_SendingBps_);
		if(estimated_SendingBps_ == 0){
			estimated_SendingBps_ = current_bitrate_bps_;
		}
		return estimated_SendingBps_;
	}

	bool GccRecvController::IsOveruse(){
		return (Hypothesis_ == 'O');
	}

	// txTimestamp : current received packet's txTimestamp in rtp header.
	// rxTimestamp : current received packet's rxTimestamp. (packet received time in local)
	// sequence : current received packet's sequence.
	void GccRecvController::UpdateGroupInfo(uint64_t nowMs, uint16_t sequence, uint64_t txTimestamp,
			uint64_t rxTimestamp, uint64_t packet_size){
		m_group_changed_ = false;

		if(m_first_packet_) {
			// First Packet... We need to initialize variable.
			curr_group_num_ = 0;
			curr_group_sseq_ = sequence;
			curr_group_stime_ = txTimestamp;

			curr_group_size_ = 0;
			curr_group_size_ += packet_size;

			prev_pkt_seq_ = sequence;
			prev_pkt_txTime_ = txTimestamp;
			prev_pkt_rxTime_ = rxTimestamp;

			m_first_packet_ = false;
			return;
		}

		if (curr_group_stime_ + BURST_TIME < txTimestamp) {
			// Groups are change.
			if(curr_group_num_ == 0) {
				// First group ends... There's no inter variables calculation.
				// Save current group information to prev_group_*.
				prev_group_sseq_ = curr_group_sseq_;
				prev_group_stime_ = curr_group_stime_;
				prev_group_eseq_ = prev_pkt_seq_;
				prev_group_etime_arrival_ = prev_pkt_rxTime_;
				prev_group_etime_departure_ = prev_pkt_txTime_;

				prev_group_size_ = curr_group_size_;
				curr_group_size_ = packet_size;

				// Update new group information.
				curr_group_num_++;
				curr_group_sseq_ = sequence;
				curr_group_stime_ = txTimestamp;
			}
			else {
				/* curr_group_etime_arrival_ = prev_pkt_seq_rxTime_;
				   curr_group_etime_departure_ = prev_pkt_seq_txTime_; */
				i_arrival_ = prev_pkt_rxTime_ - prev_group_etime_arrival_;
				i_departure_ = prev_pkt_txTime_ - prev_group_etime_departure_;

				i_delay_var_ = i_arrival_ - i_departure_;

				// Save Current group information into prev_group_*.
				prev_group_sseq_ = curr_group_sseq_;
				prev_group_stime_ = curr_group_stime_;
				prev_group_eseq_ = prev_pkt_seq_;
				prev_group_etime_arrival_ = prev_pkt_rxTime_;
				prev_group_etime_departure_ = prev_pkt_txTime_;

				group_size_interval_ = prev_group_size_ - curr_group_size_;
				prev_group_size_ = curr_group_size_;
				curr_group_size_ = 0;

				//Update new group information.
				curr_group_num_++;
				curr_group_sseq_ = sequence;
				curr_group_stime_ = txTimestamp;

				m_group_changed_ = true;
			}

			curr_group_size_ += packet_size;

			prev_pkt_seq_ = sequence;
			prev_pkt_txTime_ = txTimestamp;
			prev_pkt_rxTime_ = rxTimestamp;

			return;
		}

		int temp_arrival = prev_pkt_rxTime_ - prev_group_etime_arrival_;
		int temp_departure = prev_pkt_txTime_ - prev_group_etime_departure_;
		int temp_delay_var = temp_arrival - temp_departure;

		if(temp_arrival < BURST_TIME && temp_delay_var < 0) {
			curr_group_size_ += packet_size;

			prev_pkt_seq_ = sequence;
			prev_pkt_txTime_ = txTimestamp;
			prev_pkt_rxTime_ = rxTimestamp;

			return;
		}
		else {
			// Groups are change.
			if(curr_group_num_ == 0) {
				// First group ends... There's no inter variables calculation.
				// Save current group information to prev_group_*.
				prev_group_sseq_ = curr_group_sseq_;
				prev_group_stime_ = curr_group_stime_;
				prev_group_eseq_ = prev_pkt_seq_;
				prev_group_etime_arrival_ = prev_pkt_rxTime_;
				prev_group_etime_departure_ = prev_pkt_txTime_;

				prev_group_size_ = curr_group_size_;
				curr_group_size_ = packet_size;

				// Update new group information.
				curr_group_num_++;
				curr_group_sseq_ = sequence;
				curr_group_stime_ = txTimestamp;
			}
			else {
				/* curr_group_etime_arrival_ = prev_pkt_seq_rxTime_;
				   curr_group_etime_departure_ = prev_pkt_seq_txTime_; */
				i_arrival_ = prev_pkt_rxTime_ - prev_group_etime_arrival_;
				i_departure_ = prev_pkt_txTime_ - prev_group_etime_departure_;

				i_delay_var_ = i_arrival_ - i_departure_;

				// Save Current group information into prev_group_*.
				prev_group_sseq_ = curr_group_sseq_;
				prev_group_stime_ = curr_group_stime_;
				prev_group_eseq_ = prev_pkt_seq_;
				prev_group_etime_arrival_ = prev_pkt_rxTime_;
				prev_group_etime_departure_ = prev_pkt_txTime_;

				group_size_interval_ = prev_group_size_ - curr_group_size_;
				prev_group_size_ = curr_group_size_;
				curr_group_size_ = 0;

				//Update new group information.
				curr_group_num_++;
				curr_group_sseq_ = sequence;
				curr_group_stime_ = txTimestamp;

				m_group_changed_ = true;
			}
		}

		curr_group_size_ += packet_size;

		prev_pkt_seq_ = sequence;
		prev_pkt_txTime_ = txTimestamp;
		prev_pkt_rxTime_ = rxTimestamp;

		return;
	}

	void GccRecvController::UpdateDelayBasedBitrate(uint64_t nowMs,
			uint16_t sequence,
			uint64_t txTimestampMs,
			uint64_t rxTimestampMs,
			uint64_t packet_size, 
			uint8_t ecn){

		UpdateGroupInfo(nowMs, sequence, txTimestampMs, rxTimestampMs, packet_size);

		if(m_group_changed_){
			NS_LOG_INFO ( ns3::Simulator::Now().ToDouble(ns3::Time::S) << "GccReceiverController::UpdateDelayBasedBitrate::GroupInfo : " << curr_group_num_ << " " << i_arrival_ << " " << i_departure_ << " " << i_delay_var_ << " " << group_size_interval_) ;

			m_group_changed_ = false;
			/**
			 * Produce Delay Based Estimation based on follow variables.
			 * 
			 * i_arrival_ : inter arrival time of two adjacency group.
			 * i_departure_ : inter departure time of two adjacency group.
			 * i_delay_var_ : inter delay variance (inter arrival time - inter departure time) of two adjacency group.
			 * group_size_interval_ : group size interval.
			 *
			 */

			// TODO
			uint32_t incoming_bitrate = incoming_bitrate_.Rate(nowMs);
			if(incoming_bitrate){
				incoming_bitrate_initialized_ = true;
			}else if(incoming_bitrate_initialized_){
				incoming_bitrate_.Reset();
				incoming_bitrate_initialized_ = false;
			}
			incoming_bitrate_.Update(packet_size, nowMs);

			NS_LOG_INFO ( ns3::Simulator::Now().ToDouble(ns3::Time::S) << "GccReceiverController::UpdateDelayBasedBitrate Recv Rate : " << incoming_bitrate_.Rate(nowMs)) ;


			UpdateEstimator(i_arrival_, i_departure_, group_size_interval_, nowMs);
			OveruseDetect(i_departure_, nowMs);
			estimated_SendingBps_ = UpdateBitrate( Hypothesis_, incoming_bitrate_.Rate(nowMs), nowMs);

			NS_LOG_INFO ( ns3::Simulator::Now().ToDouble(ns3::Time::S) << "GccReceiverController::UpdateDelayBasedBitrate SendingBps : " << estimated_SendingBps_) ;
		}
	}

	/*
	   float GccRecvController::getBandwidth(uint64_t nowMs) const {

	   return m_initBw;
	   }
	   */

	/*
	   void GccRecvController::updateNetMetrics() {

	   float rrate;
	   bool rrateOK = getCurrentRecvRate(rrate);
	   if (rrateOK) m_RecvR = rrate;

	   uint32_t nLoss;
	   float plr;
	   bool plrOK = getPktLossInfo(nLoss, plr);
	   if (plrOK) {
	   m_ploss = nLoss;
	   m_plr = plr;
	   }
	   }
	   */
	void GccRecvController::logStats(uint64_t nowMs) const {
		/* NEED TO MODIFY
		   std::ostringstream os;
		   os << std::fixed;

		   os  << " algo:dummy : none "
		   << " ts: "     << (nowMs / 1000)
		   << " loglen: none "
		   << " ploss: "  << m_ploss
		   << " plr: "    << m_plr
		   << " rrate: "  << m_RecvR
		   << " srate: none ";
		   logMessage(os.str());
		   */
	}


	/* In "UpdateEstimator", Calculate offset by using KALMAN filter. WE CALL THIS FUNCTION FOR CALCULATE BITRATE */
	void GccRecvController::UpdateEstimator(int t_delta, double ts_delta, int size_delta, int nowMs){
		const double min_frame_period = UpdateMinFramePeriod(ts_delta);
		const double t_ts_delta = i_delay_var_;
		double fs_delta = size_delta;

		++num_of_deltas_;
		if(num_of_deltas_ > 1000){ //1000 is Max value of num_of_deltas
			num_of_deltas_ = 1000;
		}

		//Update Kalman Filter.
		E_[0][0] += process_noise_[0];
		E_[1][1] += process_noise_[1];

		if ((Hypothesis_ == 'O' &&  offset_ < prev_offset_) ||
				(Hypothesis_ == 'U' &&  offset_ > prev_offset_)) {
			E_[1][1] += 10 * process_noise_[1];
		}

		const double h[2] = {fs_delta, 1.0};
		const double Eh[2] = {E_[0][0] * h[0] + E_[0][1] * h[1],
			E_[1][0] * h[0] + E_[1][1] * h[1]};


		const double residual = t_ts_delta - slope_ * h[0] - offset_;
		const bool in_stable_state =  (Hypothesis_ == 'N');
		const double max_residual = 3.0 * sqrt(var_noise_);
		// We try to filter out very late frames. For instance periodic key
		// frames doesn't fit the Gaussian model well.
		if (fabs(residual) < max_residual) {
			UpdateNoiseEstimate(residual, min_frame_period, in_stable_state);
		} else {
			UpdateNoiseEstimate(residual < 0 ? -max_residual : max_residual,
					min_frame_period, in_stable_state);
		}

		const double denom = var_noise_ + h[0] * Eh[0] + h[1] * Eh[1];

		const double K[2] = {Eh[0] / denom, Eh[1] / denom};

		const double IKh[2][2] = {{1.0 - K[0] * h[0], -K[0] * h[1]},
			{-K[1] * h[0], 1.0 - K[1] * h[1]}};
		const double e00 = E_[0][0];
		const double e01 = E_[0][1];

		// Update state.
		E_[0][0] = e00 * IKh[0][0] + E_[1][0] * IKh[0][1];
		E_[0][1] = e01 * IKh[0][0] + E_[1][1] * IKh[0][1];
		E_[1][0] = e00 * IKh[1][0] + E_[1][0] * IKh[1][1];
		E_[1][1] = e01 * IKh[1][0] + E_[1][1] * IKh[1][1];

		NS_LOG_INFO ( ns3::Simulator::Now().ToDouble(ns3::Time::S) << "GccReceiverController::E :  " << E_[0][0] << " " << E_[0][1] << " " << E_[1][0] << " " << E_[1][1] << " h : " << h[0] << " " << h[1] << " t_ts_delta : " << t_ts_delta << " , slope : " << slope_ << " , var_noise : " << var_noise_ << ", K : " << K[0] << " " << K[1] << "offset_bofore : " << offset_ ) ;
		// The covariance matrix must be positive semi-definite.
		bool positive_semi_definite =
			E_[0][0] + E_[1][1] >= 0 &&
			E_[0][0] * E_[1][1] - E_[0][1] * E_[1][0] >= 0 && E_[0][0] >= 0;
		NS_ASSERT(positive_semi_definite);
		slope_ = slope_ + K[0] * residual;
		prev_offset_ = offset_;
		offset_ = offset_ + K[1] * residual;
	}

	/* "UpdateMinFramePeriod" save the history of inter departure time, and return the min value*/
	double GccRecvController::UpdateMinFramePeriod(double ts_delta){

		double min_frame_period = ts_delta;                           
		if (ts_delta_hist_.size() >= 60) {  
			ts_delta_hist_.pop_front();                                 
		}                                                             
		for (const double old_ts_delta : ts_delta_hist_) {            
			min_frame_period = std::min(old_ts_delta, min_frame_period);
		}                                                             
		ts_delta_hist_.push_back(ts_delta);                           
		return min_frame_period;                                      
	}

	/* "UpdateNoiseEstimate" calculate NOISE for Estimator */
	void GccRecvController::UpdateNoiseEstimate(double residual, double ts_delta, bool stable_state){
		if (!stable_state) {
			return;
		}

		// Faster filter during startup to faster adapt to the jitter level
		// of the network. |alpha| is tuned for 30 frames per second, but is scaled
		// according to |ts_delta|.
		double alpha = 0.01;
		if (num_of_deltas_ > 10 * 30) {
			alpha = 0.002;
		}
		// Only update the noise estimate if we're not over-using. |beta| is a
		// function of alpha and the time delta since the previous update.
		const double beta = pow(1 - alpha, ts_delta * 30.0 / 1000.0);
		avg_noise_ = beta * avg_noise_ + (1 - beta) * residual;
		var_noise_ = beta * var_noise_ +
			(1 - beta) * (avg_noise_ - residual) * (avg_noise_ - residual);
		if (var_noise_ < 1) {
			var_noise_ = 1;
		}
	}

	/* "OveruseDetect" change the signal by comparing threshold to offset calculated in Estimator. WE CALL THIS FUNCTION TO CALCULATE BITRATE */
	char GccRecvController::OveruseDetect(double ts_delta, int nowMs){
		if (num_of_deltas_ < 2) {
			return 'N';
		}

		NS_LOG_INFO ( ns3::Simulator::Now().ToDouble(ns3::Time::S) << "GccReceiverController::OveruseDetect Kalmann Values :  " << num_of_deltas_ << " " << offset_ << " ");

		const double T = std::min(num_of_deltas_,60) * offset_; // kMinNumDeltas = 60
		NS_LOG_INFO ( ns3::Simulator::Now().ToDouble(ns3::Time::S) << "GccReceiverController::OveruseDetect T :  " << T << " , threshold_ : " << threshold_) ;


		if (T > threshold_) {
			NS_LOG_INFO("WOW1");
			if (time_over_using_ == -1) {
				// Initialize the timer. Assume that we've been
				// over-using half of the time since the previous
				// sample. 
				time_over_using_ =  ts_delta / 2;
			} else {
				// Increment timer
				time_over_using_ += ts_delta;
			}   
			overuse_counter_++;
			if (time_over_using_ > overusing_time_threshold_ && overuse_counter_ > 1) {
				NS_LOG_INFO("WOW2");
				if (offset_ >= prev_offset_) {
					NS_LOG_INFO("WOW3");
					time_over_using_ = 0;
					overuse_counter_ = 0;
					Hypothesis_ = 'O'; //Overusing
				}
			}
		} else if (T < -threshold_) {
			time_over_using_ = -1;
			overuse_counter_ = 0;
			Hypothesis_ = 'U'; //Underusing
		} else {
			time_over_using_ = -1;
			overuse_counter_ = 0;
			Hypothesis_ = 'N';
		}

		UpdateThreshold(T, nowMs);
		NS_LOG_INFO ( ns3::Simulator::Now().ToDouble(ns3::Time::S) << "GccReceiverController::OveruseDetect hypo : " << Hypothesis_) ;


		return Hypothesis_;
	}

	/* "UpdateThreshold" update threshold, and the output will be dynamic value */
	void GccRecvController::UpdateThreshold(double modified_offset, int nowMs){
		if (last_threshold_update_ms_ == -1)
			last_threshold_update_ms_ = nowMs;

		if (fabs(modified_offset) > threshold_ + 15.0) { //kMaxAdaptOffsetMs = 15.0
			// Avoid adapting the threshold to big latency spikes, caused e.g.,
			// by a sudden capacity drop.
			last_threshold_update_ms_ = nowMs;
			return;
		}

		const double k = fabs(modified_offset) < threshold_ ? k_down_ : k_up_;
		const int64_t kMaxTimeDeltaMs = 100;
		int64_t time_delta_ms = std::min(nowMs - last_threshold_update_ms_, kMaxTimeDeltaMs);
		threshold_ += k * (fabs(modified_offset) - threshold_) * time_delta_ms;
		// avoid using SafeClamp, change >> threshold_ = rtc::SafeClamp(threshold_, 6.f, 600.f); << to under line.
		threshold_ = threshold_ < 6.f ? 6.f : threshold_ > 600.f ? 600.f : threshold_;
		last_threshold_update_ms_ = nowMs;

	}

	uint32_t GccRecvController::UpdateBitrate(char bw_state, uint64_t incoming_bitrate, int64_t nowMs) {   

		// Set the initial bit rate value to what we're receiving the first half       
		// second.                                                                     
		// TODO(bugs.webrtc.org/9379): The comment above doesn't match to the code.    

		if (!bitrate_is_initialized_) {                                                
			const int64_t kInitializationTimeMs = 5000;                                  
			if (time_first_incoming_estimate_ < 0) {                                     
				if (incoming_bitrate)                                               
					time_first_incoming_estimate_ = nowMs;                                  
			} else if (nowMs - time_first_incoming_estimate_ > kInitializationTimeMs && 
					incoming_bitrate) {                                        
				current_bitrate_bps_ = incoming_bitrate;                           
				bitrate_is_initialized_ = true;                                            
			}                                                                            
		}                                                                              

		current_bitrate_bps_ = ChangeBitrate(current_bitrate_bps_, bw_state, incoming_bitrate, nowMs);    
		return current_bitrate_bps_;                                                   
	}                                                                                

	int GccRecvController::GetNearMaxIncreaseRateBps() const {                         
		double bits_per_frame = static_cast<double>(current_bitrate_bps_) / 30.0;      
		double packets_per_frame = std::ceil(bits_per_frame / (8.0 * 1200.0));         
		double avg_packet_size_bits = bits_per_frame / packets_per_frame;              

		// Approximate the over-use estimator delay to 100 ms.                         
	//	const int64_t response_time = (rtt_ + 100) * 2;  // Or this value "rtt_ + 100" ... ;  
		const int64_t response_time = (300 + 100) * 2;  // Or this value "rtt_ + 100" ... ;  
		const double kMinIncreaseRateBps = 4000; // default 4000 (in webRTC) OR 1000 (in Gcc Draft)                                
		return static_cast<int>(std::max(                                             
					kMinIncreaseRateBps, (avg_packet_size_bits * 1000) / response_time));      
	}                                                                                

	uint32_t GccRecvController::ChangeBitrate(uint32_t new_bitrate_bps, char bw_state, uint64_t incoming_bitrate, 						 int64_t nowMs) {                         
		uint32_t incoming_bitrate_bps;
		if(incoming_bitrate) incoming_bitrate_bps = incoming_bitrate;
		else incoming_bitrate_bps = latest_incoming_bitrate_bps_;                                                               
		if (incoming_bitrate)                                                     
			latest_incoming_bitrate_bps_ = incoming_bitrate;                       

		// An over-use should always trigger us to reduce the bitrate, even though      
		// we have not yet established our first estimate. By acting on the over-use,   
		// we will end up with a valid estimate.                                        
		if (!bitrate_is_initialized_ && bw_state != 'O')                             
			return current_bitrate_bps_;                                                  

		ChangeState(bw_state, nowMs);                                                     
		// Calculated here because it's used in multiple places.                        
		const float incoming_bitrate_kbps = incoming_bitrate_bps / 1000.0f;             
		// Calculate the max bit rate std dev given the normalized                      
		// variance and the current incoming bit rate.                                  
		const float std_max_bit_rate =                                                  
			sqrt(var_max_bitrate_kbps_ * avg_max_bitrate_kbps_);                        
		NS_LOG_INFO("bw_state : " << bw_state << " "  << rate_control_state_);
        switch (rate_control_state_) {                                                  
			case 'H': //Hold mode                                                                
				break;                                                                      

			case 'I': //Increase mode                                                             
				if (avg_max_bitrate_kbps_ >= 0 &&                                           
						incoming_bitrate_kbps >                                                 
						avg_max_bitrate_kbps_ + 3 * std_max_bit_rate) {                     
					ChangeRegion('M');                                              
					avg_max_bitrate_kbps_ = -1.0;                                             
				}                                                                           
				if (rate_control_region_ == 'N') {                                   
					uint32_t additive_increase_bps =                                          
						AdditiveRateIncrease(nowMs, time_last_bitrate_change_);              
					new_bitrate_bps += additive_increase_bps;                                 
				} else {                                                                    
					uint32_t multiplicative_increase_bps = MultiplicativeRateIncrease(        
							nowMs, time_last_bitrate_change_, new_bitrate_bps);                  
					new_bitrate_bps += multiplicative_increase_bps;                           
				}                                                                      
				time_last_bitrate_change_ = nowMs;                                         
				break;                                                                      

			case 'D': //Decrease mode                                                             
				// Set bit rate to something slightly lower than max                        
				// to get rid of any self-induced delay.                                    
				NS_LOG_INFO("WOW4");
				new_bitrate_bps =                                                           
					static_cast<uint32_t>(beta_ * incoming_bitrate_bps + 0.5);              
				if (new_bitrate_bps > current_bitrate_bps_) {                               
					// Avoid increasing the rate when over-using.                             
					if (rate_control_region_ != 'M') {                              
						new_bitrate_bps = static_cast<uint32_t>(                                
								beta_ * avg_max_bitrate_kbps_ * 1000 + 0.5f);
						NS_LOG_INFO("wow6 : " << avg_max_bitrate_kbps_ ) ;                       
					}                                                                         
					NS_LOG_INFO(  "wow5 : " << new_bitrate_bps << " " << current_bitrate_bps_);
					new_bitrate_bps = std::min(new_bitrate_bps, current_bitrate_bps_);        
				}                                                                           
				ChangeRegion('N');

				if (incoming_bitrate_kbps < avg_max_bitrate_kbps_ - 3 * std_max_bit_rate) {   
					avg_max_bitrate_kbps_ = -1.0f;                                             
				}                                                                            

				bitrate_is_initialized_ = true;                                              
				UpdateMaxBitRateEstimate(incoming_bitrate_kbps);                             
				// Stay on hold until the pipes are cleared.                                 
				rate_control_state_ = 'H';                                               
				time_last_bitrate_change_ = nowMs;                                          
				break;                                                                       

			default:                                                                       
				assert(false);                                                               
		}                                                                                
		return ClampBitrate(new_bitrate_bps, incoming_bitrate_bps);                      
	}                                                                                       

	uint32_t GccRecvController::ClampBitrate(uint32_t new_bitrate_bps, uint32_t incoming_bitrate_bps) const { 

		// Don't change the bit rate if the send side is too far off.               
		// We allow a bit more lag at very low rates to not too easily get stuck if 
		// the encoder produces uneven outputs.                                     
		const uint32_t max_bitrate_bps =                                            
			static_cast<uint32_t>(1.5f * incoming_bitrate_bps) + 10000;             
		if (new_bitrate_bps > current_bitrate_bps_ &&                               
				new_bitrate_bps > max_bitrate_bps) {                                    
			new_bitrate_bps = std::max(current_bitrate_bps_, max_bitrate_bps);        
		}                                                                           
		new_bitrate_bps = std::max(new_bitrate_bps, min_configured_bitrate_bps_);   
		return new_bitrate_bps;                                                     
	}                                                                             

	uint32_t GccRecvController::MultiplicativeRateIncrease(int64_t nowMs, int64_t lastMs, 
			uint32_t current_bitrate_bps) const {                
		double alpha = 1.08;                                                             
		if (lastMs > -1) {                                                              
			auto time_since_last_update_ms = std::min(nowMs - lastMs, (int64_t)1000);            
			alpha = pow(alpha, time_since_last_update_ms / 1000.0);                        
		}                                                                                
		uint32_t multiplicative_increase_bps =                                           
			std::max(current_bitrate_bps * (alpha - 1.0), 1000.0);                       
		return multiplicative_increase_bps;                                              
	}                                                                                  

	uint32_t GccRecvController::AdditiveRateIncrease(int64_t nowMs, int64_t lastMs) const {    
		return static_cast<uint32_t>((nowMs - lastMs) * GetNearMaxIncreaseRateBps()/1000); 
	}                                                                                  

	void GccRecvController::UpdateMaxBitRateEstimate(float incoming_bitrate_kbps) {      
		const float alpha = 0.05f;                                                       
		if (avg_max_bitrate_kbps_ == -1.0f) {                                            
			avg_max_bitrate_kbps_ = incoming_bitrate_kbps;                                 
		} else {                                                                         
			avg_max_bitrate_kbps_ =                                                        
				(1 - alpha) * avg_max_bitrate_kbps_ + alpha * incoming_bitrate_kbps;       
		}                                                                                
		// Estimate the max bit rate variance and normalize the variance                 
		// with the average max bit rate.                                                
		const float norm = std::max(avg_max_bitrate_kbps_, 1.0f);                        
		var_max_bitrate_kbps_ =                                                          
			(1 - alpha) * var_max_bitrate_kbps_ +                                        
			alpha * (avg_max_bitrate_kbps_ - incoming_bitrate_kbps) *                    
			(avg_max_bitrate_kbps_ - incoming_bitrate_kbps) / norm;                  
		// 0.4 ~= 14 kbit/s at 500 kbit/s                                                
		if (var_max_bitrate_kbps_ < 0.4f) {                                              
			var_max_bitrate_kbps_ = 0.4f;                                                  
		}                                                                                
		// 2.5f ~= 35 kbit/s at 500 kbit/s                                               
		if (var_max_bitrate_kbps_ > 2.5f) {                                              
			var_max_bitrate_kbps_ = 2.5f;                                                  
		}                                                                                
	}                                                                                  

	void GccRecvController::ChangeState (char bw_state, int64_t nowMs) {             
		switch (bw_state) {                                     
			case 'N': //normal -> if state is hold, change to Increase mode                 
				if (rate_control_state_ == 'H') {                     
					time_last_bitrate_change_ = nowMs;                     
					rate_control_state_ = 'I';                      
				}                                                         
				break;                                                    
			case 'O': //overusing -> if state is not decrease mode, change to Decrease mode      
				if (rate_control_state_ != 'D') {                 
					rate_control_state_ = 'D';                      
				}                                                         
				break;                                                    
			case 'U': //underusing -> change to Hold mode                        
				rate_control_state_ = 'H';                            
				break;                                                    
			default:                                                    
				assert(false);                                            
		}                                                             
	}                                                               

	void GccRecvController::ChangeRegion(char region) {  
		rate_control_region_ = region;                                
	}            

}
