#ifndef STREAMCONNECTION_H
#define STREAMCONNECTION_H

#include "sensekit_types.h"
#include <cassert>
#include "StreamBin.h"

namespace sensekit {

    class StreamBin;
    class Stream;
    class StreamConnection;
    
    class StreamConnection
    {
    public:
        using FrameReadyCallback = std::function<void(StreamConnection*)>;
        StreamConnection(Stream* stream);
        ~StreamConnection();

        void start();
        void stop();

        sensekit_frame_ref_t* lock();
        void unlock();

        void set_bin(StreamBin* bin);
        StreamBin* get_bin() const { return m_bin; }

        Stream* get_stream() const { return m_stream; }

        sensekit_streamconnection_t* get_handle() { return &m_connection; }

        static StreamConnection* get_ptr(sensekit_streamconnection_t* conn)
            { return reinterpret_cast<StreamConnection*>(conn->handle); }

        const sensekit_stream_desc_t& get_description() const;

        CallbackId register_frame_ready_callback(FrameReadyCallback callback);
        void unregister_frame_ready_callback(CallbackId& callbackId);

        void set_parameter(sensekit_parameter_id id,
                           size_t byteLength,
                           sensekit_parameter_data_t* data);

        void get_parameter_size(sensekit_parameter_id id,
                                size_t& byteLength);

        void get_parameter_data(sensekit_parameter_id id,
                                size_t byteLength,
                                sensekit_parameter_data_t* data);

    private:
        void on_bin_front_buffer_ready(StreamBin* bin);

        sensekit_streamconnection_t m_connection;
        sensekit_frame_ref_t m_currentFrame;

        bool m_locked{false};
        bool m_started{false};

        Stream* m_stream{nullptr};
        StreamBin* m_bin{nullptr};
        sensekit_stream_t m_handle{nullptr};

        FrontBufferReadyCallback m_binFrontBufferReadyCallback;
        CallbackId m_binFrontBufferReadyCallbackId;

        Signal<StreamConnection*> m_frameReadySignal;
    };
}

#endif /* STREAMCONNECTION_H */
