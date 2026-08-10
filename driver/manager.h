/****************************************************************************
*
*  Module Name    : manager.h
*  Version        : 
*
*  Abstract       : RAVENNA/AES67 ALSA LKM
*
*  Written by     : Baume Florian, Beguec Frederic
*  Date           : 29/03/2016
*  Modified by    : Baume Florian
*  Date           : 13/01/2017
*  Modification   : C port
*  Known problems : None
*
* Copyright(C) 2017 Merging Technologies
*
* RAVENNA/AES67 ALSA LKM is free software; you can redistribute it and / or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* RAVENNA/AES67 ALSA LKM is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with RAVAENNA ALSA LKM ; if not, see <http://www.gnu.org/licenses/>.
*
****************************************************************************/

#pragma once

#include "MTAL_stdint.h"
//	System Includes
#include "manager_defs.h"

#include "PTP.h"
#include "RTP_streams_manager.h"

#include "../common/MergingRAVENNACommon.h"
#include "../common/MT_ALSA_message_defs.h"

#include "MR_AudioDriverTypes.h"

#include "EtherTubeNetfilter.h"

#include "audio_driver.h"

/// work_struct for the deferred sample-rate apply (see SetSamplingRate / stopIO in manager.c).
#include <linux/workqueue.h>

#ifdef MTTRANSPARENCY_CHECK
    #include "MTTransparencyCheck.h"
#endif

#define MAX_INTERFACE_NAME 64

/// How long a sample-rate change waits for the media clock to RE-LOCK, in MILLISECONDS.
/// Expressed in time because the old 4000-iteration counter was HZ-dependent (~32 s at
/// CONFIG_HZ=250, not the 4 s it looks like). 12 s, not the old nominal 4 s: a real re-lock
/// after a rate change measures ~8 s on imx6 (PTP lock 2->0->1->2), so 4 s would time out on
/// every change. Must stay UNDER Roon's 15 s prepare timeout, which is the whole point.
#define PTP_LOCK_WAIT_MS 12000

/// How long to wait for the PTP reset triggered by StartAudioFrameTICTimer() to become
/// VISIBLE (lock drops) before concluding no re-rate is in flight. The reset shows up within
/// ~26 ms in practice; 500 ms is slack. See WaitForPTPLock() for why sampling the lock level
/// instead of the transition silently corrupts playback rate.
#define PTP_LOCK_DROP_WAIT_MS 500

#ifndef nullptr
    #define nullptr NULL
#endif // nullptr

#include "linux/kernel.h"


//#define MT_TONE_TEST 1
//#define MT_RAMP_TEST 1


struct TManager
{
    bool m_Is_NIC_Active[_MAX_NICS];

    TEtherTubeNetfilter m_EthernetFilter[_MAX_NICS];
    TClock_PTP m_PTP[_MAX_NICS];
    TRTP_streams_manager m_RTP_streams_manager;
    EPTPLockStatus m_lastLockStatus[_MAX_NICS];
    unsigned short m_Active_PTP_NIC_Idx;
    uint32_t m_NumberOfInputs;
    uint32_t m_NumberOfOutputs;
    uint64_t m_RingBufferFrameSize;
    uint32_t m_SampleRate;
    /// Rate whose PTP-lock wait most recently TIMED OUT, or 0 when the last wait succeeded.
    /// The ALSA layer asks for the same rate up to three times per change (hw_params once,
    /// prepare twice), each guarded only by what the peer currently reports — so without
    /// this, one unlockable rate change pays the timeout three times over. See
    /// set_sample_rate().
    uint32_t m_RateWaitTimedOutFor;
    /// Rate that SetSamplingRate() had to refuse because IO was running, 0 when nothing is
    /// pending. Applied from m_RateWork, NOT inline -- see the warning in stopIO().
    uint32_t m_PendingSampleRate;
    /// Runs the deferred apply in PROCESS context. stopIO() is called from the ALSA trigger
    /// callback (atomic), and applying a rate sleeps, so it cannot be done there.
    struct work_struct m_RateWork;
    enum eAudioMode m_AudioMode;

    uint64_t m_TICFrameSizeAt1FS;
    uint32_t m_ui32FrameSize;
    uint32_t m_MaxFrameSize;

    int32_t m_nPlayoutDelay;
    int32_t m_nCaptureDelay;

    bool m_bIsPlaybackIO;
    bool m_bIsRecordingIO;

    volatile bool m_bIsStarted;
    volatile bool m_bIORunning;

    char m_cInterfaceName[MAX_INTERFACE_NAME];


    rtp_audio_stream_ops m_c_callbacks;
    //dispatch_packet_ops m_c_dispatch_callbacks;
    clock_ptp_ops m_c_audio_streamer_clock_PTP_callback;


// ALSA <> Manager communication

    void* m_pALSAChip;                              /// pointer to ALSA chip struct (struct mr_alsa_audio_chip)
    const struct ravenna_mgr_ops *m_alsa_driver_frontend;   /// Manager to ALSA driver (e.g. buffers access and lock)
    struct alsa_ops m_alsa_callbacks;                /// ALSA driver to Manager (e.g. audio setup at runtime)
};


bool init(struct TManager* self, int* errorCode);
void destroy(struct TManager* self);

bool start(struct TManager* self);
bool stop(struct TManager* self);

bool startIO(struct TManager* self, bool is_playback);
bool stopIO(struct TManager* self, bool is_playback);

bool SetInterfaceName(struct TManager* self, const char* cInterfaceName, const int iEthFilterIndex);
bool SetSamplingRate(struct TManager* self, uint32_t SamplingRate);
bool SetDSDSamplingRate(struct TManager* self, uint32_t SamplingRate);
bool SetTICFrameSizeAt1FS(struct TManager* self, uint64_t TICFrameSize);
bool SetMaxTICFrameSize(struct TManager* self, uint64_t max_frameSize);
bool SetNumberOfInputs(struct TManager* self, uint32_t NumberOfChannels);
bool SetNumberOfOutputs(struct TManager* self, uint32_t NumberOfChannels);

TClock_PTP* GetPTP(struct TManager* self, unsigned short ptp_idx);
void Select_PTP_NIC(struct TManager* self);
unsigned short GetSelected_PTP_NIC(struct TManager* self);

bool IsStarted(struct TManager* self);
bool IsIOStarted(struct TManager* self);

// Netfilter
int EtherTubeRxPacket(struct TManager* self, void* packet, int packet_size, const char* ifname, int mac_header);
void EtherTubeHookFct(struct TManager* self, void* hook_fct, void* hook_struct);

// Messaging
void OnNewMessage(struct TManager* self, struct MT_ALSA_msg* msg_rcv);

// Statistics
bool GetHALToTICDelta(struct TManager* self, THALToTICDelta* pHALToTICDelta);
//mutable CMTAL_CriticalSection	m_csStats;
//CMTAL_PerfMonMinMax<int32_t>	m_pmmmHALToTICDelta;

void UpdateFrameSize(struct TManager* self);

void MuteInputBuffer(struct TManager* self);
void MuteOutputBuffer(struct TManager* self);

uint32_t GetTICFrameSizeAt1FS(struct TManager* self);
uint32_t GetMaxTICFrameSize(struct TManager* self);

// Caudio_streamer_clock_PTP_callback
// C++ style
uint32_t GetIPAddress(void* user);// TODO
void AudioFrameTIC(void* user);
// C style
//static void AudioFrameTIC_(void* self) { return ((CManager*)self)->AudioFrameTIC(); }
//static uint32_t GetIPAddress_(void* self) { return ((CManager*)self)->GetIPAddress(); }
// CEtherTubeAdviseSink
EDispatchResult DispatchPacket(struct TManager* self, void* pBuffer, uint32_t packetsize, int mac_header, unsigned char nicId);

//////////////////////////////////////
// Ex-CRTP_audio_stream_callback was defined in RTP_audio_stream.hpp
uint64_t get_global_SAC(void* user);
uint64_t get_global_time(void* user);
void get_global_times(void* user, uint64_t* pui64GlobalSAC, uint64_t* pui64GlobalTime, uint64_t* pui64GlobalPerformanceCounter);
uint32_t get_frame_size(void* user);
void get_audio_engine_sample_format(void* user, enum EAudioEngineSampleFormat* pnSampleFormat);
char get_audio_engine_sample_bytelength(void* user);
void* get_live_in_jitter_buffer(void* user, uint32_t ulChannelId);	// Note: buffer type is retrieved through get_audio_engine_sample_format
void* get_live_out_jitter_buffer(void* user, uint32_t ulChannelId);	// Note: buffer type is retrieved through get_audio_engine_sample_format
uint32_t get_live_in_jitter_buffer_length(void* user);
uint32_t get_live_out_jitter_buffer_length(void* user);
uint32_t get_live_in_jitter_buffer_offset(void* user, const uint64_t ui64CurrentSAC);
uint32_t get_live_out_jitter_buffer_offset(void* user, const uint64_t ui64CurrentSAC);
int update_live_in_audio_data_format(void* user, uint32_t /*ulChannelId*/, char const * /*pszCodec*/);
unsigned char get_live_in_mute_pattern(void* user, uint32_t ulChannelId);
unsigned char get_live_out_mute_pattern(void* user, uint32_t /*ulChannelId*/);


void Init_C_Callbacks(struct TManager* self);
rtp_audio_stream_ops* Get_C_Callbacks(struct TManager* self);
//dispatch_packet_ops* Get_C_Dispatch_Callbacks() {return &m_c_dispatch_callbacks;}


int attach_alsa_driver(void* user, const struct ravenna_mgr_ops *ops, void *alsa_chip_pointer);
void init_alsa_callbacks(struct TManager* self);
int get_input_jitter_buffer_offset(void* user, uint32_t *offset);
int get_output_jitter_buffer_offset(void* user, uint32_t *offset);
int get_min_interrupts_frame_size(void* user, uint32_t *framesize);
int get_max_interrupts_frame_size(void* user, uint32_t *framesize);
int get_interrupts_frame_size(void* user, uint32_t *framesize); // ALSA PCM period size must be a multiple of this framesize
int set_sample_rate(void* user, uint32_t rate);
int get_sample_rate(void* user, uint32_t *rate);
int get_jitter_buffer_sample_bytelength(void* user, char *byte_len);
//int set_nb_inputs(void* user, uint32_t nb_channels);
//int set_nb_outputs(void* user, uint32_t nb_channels);
int get_nb_inputs(void* user, uint32_t *nb_Channels);
int get_nb_outputs(void* user, uint32_t *nb_Channels);
int get_playout_delay(void* user, snd_pcm_sframes_t *delay_in_sample);
int get_capture_delay(void* user, snd_pcm_sframes_t *delay_in_sample);
int start_interrupts(void* user, bool is_playback);
int stop_interrupts(void* user, bool is_playback);
int notify_master_volume_change(void* user, int direction, int32_t value);
int notify_master_switch_change(void* user, int direction, int32_t value);
int get_master_volume_value(void* user, int direction, int32_t* value);
int get_master_switch_value(void* user, int direction, int32_t* value);

// helpers
bool IsDSDRate(const uint32_t sample_rate);
enum eAudioMode GetAudioModeFromRate(const uint32_t sample_rate);

// Debug
#ifdef MTTRANSPARENCY_CHECK
    CMTTransparencyCheck    m_Transparencycheck;
#endif

#if defined(MT_TONE_TEST)
    unsigned long m_tone_test_phase;
#elif defined(MT_RAMP_TEST)
    int32_t m_ramp_test_phase;
#endif // MT_TONE_TEST

