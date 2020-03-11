#pragma once

#include <librealsense2/rs.hpp>
#include "RsRtspClient.h"
#include "software-device.h"
#include "RsSink.h"

const int RTP_QUEUE_MAX_SIZE = 30;

struct Raw_Frame
{
	Raw_Frame(char* buffer, int size,  struct timeval timestamp) :m_metadata((RsMetadataHeader*)buffer), m_buffer(buffer+sizeof(RsMetadataHeader)), m_size(size), m_timestamp(timestamp) {};
	Raw_Frame(const Raw_Frame&);
	Raw_Frame& operator=(const Raw_Frame&);
	~Raw_Frame()  { delete [] m_buffer; };
	
    RsMetadataHeader* m_metadata;
	char* m_buffer;
	unsigned int m_size;
	struct timeval m_timestamp;
};

// TODO: consider use and extend librs c+ stream object
class rs_rtp_stream
{
    public:

        rs_rtp_stream(rs2_video_stream rs_stream, rs2::stream_profile rs_profile)
        {
            frame_data_buff.bpp = rs_stream.bpp;
		    
            //todo: consider move to cpp api for using rs2::stream_profile
            frame_data_buff.profile = rs_profile;
            m_stream_profile = rs_profile;
		    frame_data_buff.stride = rs_stream.bpp * rs_stream.width;
		    pixels_buff.resize(frame_data_buff.stride * rs_stream.height, 0);
            frame_data_buff.pixels = pixels_buff.data();
    		frame_data_buff.deleter = this->frame_deleter;

            m_rs_stream = rs_stream;
        }

        rs2::stream_profile get_stream_profile()
        {
            return m_stream_profile;
        }

        rs2_stream stream_type()
        {
            return m_rs_stream.type;                   
        }

        void insert_frame(Raw_Frame* new_raw_frame)
        {
            if(queue_size()>RTP_QUEUE_MAX_SIZE)
            {
                std::cout << "queue is full. dropping frame for stream id: " << this->m_rs_stream.uid << std::endl;
            }
            else
            {
                std::lock_guard<std::mutex> lock(this->stream_lock);
                frames_queue.push(new_raw_frame);
            }
        }

        std::map<long long int,rs2_extrinsics> extrinsics_map;

        Raw_Frame* extract_frame()
        {
            std::lock_guard<std::mutex> lock(this->stream_lock);
            Raw_Frame* frame = frames_queue.front();
			frames_queue.pop();
            return frame;
        }

        void reset_queue()
        {
            while(!frames_queue.empty())
            {
                frames_queue.pop();
            }
            std::cout << "done clean frames queue: " << m_rs_stream.uid << std::endl;
        }

        int queue_size()
        {
            std::lock_guard<std::mutex> lock(this->stream_lock);
            return frames_queue.size();
        }

        static MemoryPool& get_memory_pool()
        {
            static MemoryPool memory_pool_instance = MemoryPool();
            return memory_pool_instance;
        }

        bool is_enabled;

        rs2_video_stream m_rs_stream;

        rs2_software_video_frame frame_data_buff;

    private:


        
        static void frame_deleter(void* p) { 
                get_memory_pool().returnMem((unsigned char*)p -sizeof(RsFrameHeader));
            }

        rs2::stream_profile m_stream_profile;

        std::mutex stream_lock;

        std::queue<Raw_Frame*> frames_queue;
        // frame data buffers
        
        // pixels data 
        std::vector<uint8_t> pixels_buff;
};
