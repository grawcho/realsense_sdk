// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016 Intel Corporation. All Rights Reserved.

#pragma once
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include "compression/decoder.h"
#include "include/file_types.h"
#include "status.h"
#include "disk_read_interface.h"
#include "include/file.h"

namespace rs
{
    namespace playback
    {
        class disk_read_base : public disk_read_interface
        {
            struct active_stream_info
            {
                core::file_types::stream_info   m_stream_info;
                std::vector<uint32_t>           m_image_indices;
                uint32_t                        m_prefetched_samples_count;
            };

        public:
            disk_read_base(const char *file_path);
            virtual ~disk_read_base(void);
            virtual core::status init() override;
            virtual void reset() override;
            virtual void resume() override;
            virtual void pause() override;
            virtual void enable_stream(rs_stream stream, bool state) override;
            virtual bool is_motion_tracking_enabled() override { return m_is_motion_tracking_enabled; }
            virtual void enable_motions_callback(bool state) override;
            virtual void set_realtime(bool realtime) override;
            virtual std::map<rs_stream, std::shared_ptr<core::file_types::frame_sample>> set_frame_by_index(uint32_t index, rs_stream stream_type) override;
            virtual std::map<rs_stream, std::shared_ptr<core::file_types::frame_sample>> set_frame_by_time_stamp(uint64_t ts) override;
            virtual bool query_realtime() override { return m_realtime; }
            virtual bool is_stream_profile_available(rs_stream stream, int width, int height, rs_format format, int framerate) override;
            virtual uint32_t query_number_of_frames(rs_stream stream_type) override;
            virtual int32_t query_coordinate_system() override { return m_file_header.coordinate_system; }
            virtual const std::map<rs_camera_info, std::string>& get_camera_info() override { return m_camera_info; }
            virtual std::map<rs_stream, core::file_types::stream_info> get_streams_infos() override {return m_streams_infos; }
            virtual rs_motion_intrinsics get_motion_intrinsics() { return m_motion_intrinsics; }
            virtual std::map<rs_option, double> get_properties() override { return m_properties; }
            virtual std::vector<rs_capabilities> get_capabilities() { return m_capabilities; }
            virtual playback::capture_mode query_capture_mode() override { return m_file_header.capture_mode; }
            virtual file_info query_file_info() override ;
            virtual uint64_t query_run_time() override;
            virtual void set_callback(std::function<void(std::shared_ptr<core::file_types::sample>)> handler) { m_sample_callback = handler;}
            virtual void set_callback(std::function<void()> handler) { m_eof_callback = handler; }
            virtual void set_total_frame_drop_count(double value) override;
            virtual void update_frame_drop_count(rs_stream stream, uint32_t frame_drop) override;
            virtual void update_imu_drop_count(uint32_t drop_count)override;

        protected:
            virtual rs::core::status read_headers() = 0;
            virtual void index_next_samples(uint32_t number_of_samples) = 0;
            virtual int32_t size_of_pitches(void) = 0;
            virtual std::shared_ptr<core::file_types::frame_sample> read_image_buffer(std::shared_ptr<rs::core::file_types::frame_sample> &frame);
            void read_thread();
            core::file_types::version query_sdk_version();
            core::file_types::version query_librealsense_version();
            core::status get_image_offset(rs_stream stream, int64_t &offset);
            void notify_available_samples();
            void prefetch_sample();
            bool read_next_sample();
            void update_time_base();
            std::map<rs_stream, std::shared_ptr<core::file_types::frame_sample>> find_nearest_frames(uint32_t sample_index, rs_stream stream);
            bool all_samples_bufferd();
            void init_decoder();
            virtual uint32_t read_frame_metadata(const std::shared_ptr<core::file_types::frame_sample>& frame, unsigned long num_bytes_to_read) = 0;
            int64_t calc_sleep_time(std::shared_ptr<core::file_types::sample> sample);

            playback::capture_mode get_capture_mode();

            static const int                                                NUMBER_OF_SAMPLES_TO_INDEX = 1;

            //if IMU and video streams are enabled no more than 4 images will be bufferd per stream
            static const int                                                NUMBER_OF_REQUIRED_PREFETCHED_SAMPLES = 20;

            std::string                                                     m_file_path;
            //file pointers
            std::unique_ptr<core::file>                                     m_file_indexing;//use only for samples indexing
            std::unique_ptr<core::file>                                     m_file_data_read;//use both for file header read and image data read

            bool                                                            m_pause;
            bool                                                            m_realtime;
            bool                                                            m_is_index_complete;

            std::mutex                                                      m_mutex;
            std::thread                                                     m_thread;

            std::shared_ptr<core::compression::decoder>                     m_decoder;
            std::vector<uint8_t>                                            m_encoded_data;

            std::chrono::high_resolution_clock::time_point                  m_base_sys_time;
            uint64_t                                                        m_base_ts;

            //file static info
            core::file_types::sw_info                                       m_sw_info;
            core::file_types::file_header                                   m_file_header;
            std::map<rs_option, double>                                     m_properties;
            std::vector<rs_capabilities>                                    m_capabilities;
            std::map<rs_stream, core::file_types::stream_info>              m_streams_infos;
            rs_motion_intrinsics                                            m_motion_intrinsics;
            std::map<rs_stream, active_stream_info>                         m_active_streams_info;
            std::map<rs_camera_info, std::string>                           m_camera_info;
            bool                                                            m_is_motion_tracking_enabled;

            //sticky variables, calculated once in objects lifetime
            std::map<rs_stream, std::vector<uint32_t>>                      m_image_indices; // index in m_samples_descriptors
            std::queue<std::shared_ptr<core::file_types::sample>>           m_prefetched_samples;
            std::vector<std::shared_ptr<core::file_types::sample>>          m_samples_desc; // growing vector of all samples descriptors in order of capture
            uint32_t                                                        m_samples_desc_index; // points to the nexr indexed sample, which wasn't prefetched yet

            std::function<void(std::shared_ptr<core::file_types::sample>)>  m_sample_callback;
            std::function<void()>                                           m_eof_callback;

            std::map<rs_stream,uint32_t>                                    m_frame_drop_count;
            uint64_t                                                        m_motion_drop_count;
        };
    }
}
