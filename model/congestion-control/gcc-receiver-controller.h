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
 * Gcc Receiver Side Controller (Delay Based Congestion Control) interface for gcc ns3 module.
 *
 * @version 0.1.0
 */

#ifndef GCC_RECV_CONTROLLER_H
#define GCC_RECV_CONTROLLER_H

#include <deque>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <ns3/rate_statistics.h>

namespace rmcat {

class GccRecvController
{
public:
    /** Class constructor */
    GccRecvController();

    /** Class destructor */
    ~GccRecvController();

    /**
     * Set the current bandwidth estimation. This can be useful in test environments
     * to temporarily disrupt the current bandwidth estimation
     *
     * @param [in] newBw Bandwidth estimation to overwrite the current estimation
     */
    // void setCurrentBw(float newBw);

    /**
     * Reset the internal state of the congestion controller
     */
    void reset();


    /**
     * Function for updating delay based controller's bitrate(Ar).
     * This function will process "Overuse Detecting" and "Overuse Estimating" in internet draft.
     */
    void UpdateDelayBasedBitrate(uint64_t nowMs,
                                 uint16_t sequence,
                                 uint64_t txTimestampMs,
                          			 uint64_t rxTimestampMs,
                        				 uint64_t packet_size,
                                 uint8_t ecn);
    /**
     * Simplistic implementation of bandwidth getter. It returns a hard-coded
     * bandwidth value in bits per second
     */
    // float getBandwidth(uint64_t nowMs) const;
    
    /**
     * Get Funtion of estimated_SendingBps_
     */
    uint32_t GetBitrate();

    bool IsOveruse();

private:
    // void updateNetMetrics();
    void logStats(uint64_t nowMs) const;

    void UpdateGroupInfo(uint64_t nowMs, uint16_t sequence, uint64_t txTimestamp, uint64_t rxTimestamp, uint64_t packet_size);
    
    uint32_t estimated_SendingBps_;  /* Sending rate estimated by delay based controller. */

    uint64_t m_lastTimeCalcMs;
    bool m_lastTimeCalcValid;

    uint64_t m_QdelayMs; /**< estimated queuing delay in microseconds */
    uint32_t m_ploss;  /**< packet loss count within configured window */
    float m_plr;       /**< packet loss ratio within packet history window */
    float m_RecvR;     /**< updated receiving rate in bps */

    int n_loss;
    int n_total_pkt;
    int m_lossT;
    int valid_pkt_sequence_;

    int curr_group_num_;
    uint16_t curr_group_sseq_;     /* Current group's first packet sequence*/
    uint64_t curr_group_stime_;    /* Current group's first packet txTimestamp*/
    uint16_t curr_group_eseq_;     /* Current group's last packet sequence.*/
    uint64_t curr_group_etime_arrival_;    /* Current group's last packet rxTimestamp.*/
    uint64_t curr_group_etime_departure_;    /* Current group's last packet txTimestamp.*/

    uint16_t prev_group_sseq_;     /* Previous group's first packet sequence*/
    uint64_t prev_group_stime_;    /* Previous group's first packet txTimestamp.*/
    uint16_t prev_group_eseq_;     /* Previous group's last packet sequence.*/
    uint64_t prev_group_etime_departure_;    /* Previous group's last packet rxTimestamp(arrival time of group's last packet).*/
    uint64_t prev_group_etime_arrival_;      /* Previous group's last packet txTimestamp.*/

    int curr_group_size_;
    int prev_group_size_;
    int group_size_interval_;

    uint16_t prev_pkt_seq_;
    uint64_t prev_pkt_txTime_;
    uint64_t prev_pkt_rxTime_;

    int i_arrival_;
    int i_departure_;
    int i_delay_var_;

    bool m_group_changed_;
    bool m_first_packet_;

	/* For Delay Based Calculate */
	char Hypothesis_;

	/* For Overuse Estimator */
	void UpdateEstimator(int t_delta, double ts_delta, int size_delta, int nowMs);
	double UpdateMinFramePeriod(double ts_delta);
	void UpdateNoiseEstimate(double residual, double ts_delta, bool stable_state);	

	std::deque<double> ts_delta_hist_;
	int num_of_deltas_;
	double slope_;
	double offset_;
	double prev_offset_;
	double E_[2][2];
	double process_noise_[2];
	double avg_noise_;
	double var_noise_;

	/* For Overuse Detect */
    char OveruseDetect(double ts_delta, int nowMs);
	void UpdateThreshold(double modified_offset, int nowMs);

    double k_up_;
    double k_down_;
    double overusing_time_threshold_;
    double threshold_;
    int64_t last_threshold_update_ms_;
    double time_over_using_;
    int overuse_counter_;
	

	/* For Calculate Delay Based Bitrate */
	uint32_t UpdateBitrate(char bw_state, uint64_t incoming_bitrate, int64_t nowMs);
    int GetNearMaxIncreaseRateBps() const;
    uint32_t ChangeBitrate(uint32_t new_bitrate_bps, char bw_state, uint64_t incoming_bitrate, int64_t nowMs);
    uint32_t ClampBitrate(uint32_t new_bitrate_bps, uint32_t incoming_bitrate_bps) const;
    uint32_t MultiplicativeRateIncrease(int64_t nowMs, int64_t lastMs, uint32_t current_bitrate_bps) const;
    uint32_t AdditiveRateIncrease(int64_t nowMs, int64_t lastMs) const;
    void UpdateMaxBitRateEstimate(float incoming_bitrate_kbps);
    void ChangeState(char bw_state, int64_t nowMs);
    void ChangeRegion(char region);

    uint32_t min_configured_bitrate_bps_;   
    uint32_t max_configured_bitrate_bps_;   
    uint32_t current_bitrate_bps_;          
    uint32_t latest_incoming_bitrate_bps_;  
    float avg_max_bitrate_kbps_;            
    float var_max_bitrate_kbps_;            
    char rate_control_state_;   
    char rate_control_region_; 
    int64_t time_last_bitrate_change_;      
    int64_t time_first_incoming_estimate_;  
    bool bitrate_is_initialized_;           
    float beta_;                            
    int64_t rtt_;                           

    webrtc::RateStatistics incoming_bitrate_;
    bool incoming_bitrate_initialized_;
};

}

#endif /* DUMMY_CONTROLLER_H */
