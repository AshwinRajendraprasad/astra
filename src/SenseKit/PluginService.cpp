﻿#include "PluginService.h"
#include "Stream.h"
#include "SenseKitContext.h"
#include "PluginServiceDelegate.h"
#include <SenseKit/Plugins/PluginServiceProxyBase.h>
#include <SenseKit/sensekit_types.h>
#include "CreatePluginProxy.h"
#include "ParameterBin.h"
#include "Logging.h"
#include <cstdio>
#include <memory>

namespace sensekit
{
    PluginServiceProxyBase* PluginService::create_proxy()
    {
        return create_plugin_proxy(this);
    }

    void PluginService::notify_host_event(sensekit_event_id id, const void* data, size_t dataSize)
    {
        m_hostEventSignal.raise(id, data, dataSize);
    }

    sensekit_status_t PluginService::create_stream_set(const char* streamUri, sensekit_streamset_t& streamSet)
    {
        StreamSet& set = m_context.get_setCatalog().get_or_add(streamUri, true);
        streamSet = set.get_handle();

        m_logger.info("creating streamset: %s %x", streamUri, streamSet);

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::destroy_stream_set(sensekit_streamset_t& streamSet)
    {
        StreamSet* actualSet = StreamSet::get_ptr(streamSet);

        m_logger.info("destroying streamset: %s %x", actualSet->get_uri().c_str(), streamSet);
        m_context.get_setCatalog().destroy_set(actualSet);

        streamSet = nullptr;

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::register_stream_added_callback(stream_added_callback_t callback,
                                                                    void* clientTag,
                                                                    CallbackId& callbackId)
    {
        auto thunk = [clientTag, callback](sensekit_streamset_t ss,
                                           sensekit_stream_t s,
                                           sensekit_stream_desc_t d)
            {
                callback(clientTag, ss, s, d);
            };

        callbackId = m_streamAddedSignal += thunk;

        m_context.raise_existing_streams_added(callback, clientTag);

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::register_stream_removing_callback(stream_removing_callback_t callback,
                                                                       void* clientTag,
                                                                       CallbackId& callbackId)
    {
        auto thunk = [clientTag, callback](sensekit_streamset_t ss,
                                           sensekit_stream_t s,
                                           sensekit_stream_desc_t d)
            {
                callback(clientTag, ss, s, d);
            };

        callbackId = m_streamRemovingSignal += thunk;

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::unregister_stream_added_callback(CallbackId callbackId)
    {
        m_streamAddedSignal -= callbackId;

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::unregister_stream_removing_callback(CallbackId callbackId)
    {
        m_streamRemovingSignal -= callbackId;

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::create_stream(sensekit_streamset_t setHandle,
                                                   sensekit_stream_desc_t desc,
                                                   stream_callbacks_t pluginCallbacks,
                                                   sensekit_stream_t& handle)
    {
        // TODO add to specific stream set
        StreamSet* set = StreamSet::get_ptr(setHandle);
        Stream* stream = set->register_stream(desc, pluginCallbacks);
        handle = stream->get_handle();

        m_logger.info("registered stream -- handle %x type: %d", handle, desc.type);

        m_streamAddedSignal.raise(setHandle, handle, desc);

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::destroy_stream(sensekit_stream_t& streamHandle)
    {
        if (streamHandle == nullptr)
            return SENSEKIT_STATUS_INVALID_PARAMETER;

        Stream* stream = Stream::get_ptr(streamHandle);

        assert(stream != nullptr);

        StreamSet* set =
            m_context.get_setCatalog().find_streamset_for_stream(stream);

        assert(set != nullptr);

        const sensekit_stream_desc_t& desc = stream->get_description();

        m_logger.info("destroying stream -- handle: %x type: %d", stream->get_handle(), desc.type);

        m_streamRemovingSignal.raise(set->get_handle(), streamHandle, desc);

        set->destroy_stream(stream);

        streamHandle = nullptr;

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::get_streamset_uri(sensekit_streamset_t setHandle,
                                                       const char*& uri)
    {
        assert(setHandle != nullptr);

        StreamSet* actualSet = StreamSet::get_ptr(setHandle);
        uri = actualSet->get_uri().c_str();

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::create_stream_bin(sensekit_stream_t streamHandle,
                                                       size_t lengthInBytes,
                                                       sensekit_bin_t& binHandle,
                                                       sensekit_frame_t*& binBuffer)
    {
        Stream* actualStream = Stream::get_ptr(streamHandle);
        StreamBin* bin = actualStream->create_bin(lengthInBytes);

        binHandle = bin->get_handle();
        binBuffer = bin->get_backBuffer();

        m_logger.info("creating bin -- handle: %x stream: %x type: %d size: %u",
                      binHandle,
                      streamHandle,
                      actualStream->get_description().type,
                      lengthInBytes);

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::destroy_stream_bin(sensekit_stream_t streamHandle,
                                                        sensekit_bin_t& binHandle,
                                                        sensekit_frame_t*& binBuffer)
    {
        Stream* actualStream = Stream::get_ptr(streamHandle);
        StreamBin* bin = StreamBin::get_ptr(binHandle);

        m_logger.info("destroying bin -- %x stream: %x type: %d size: %u",
                      binHandle,
                      streamHandle,
                      actualStream->get_description().type,
                      bin->bufferSize());

        actualStream->destroy_bin(bin);

        binHandle = nullptr;
        binBuffer = nullptr;

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::bin_has_connections(sensekit_bin_t binHandle, bool& hasConnections)
    {
        StreamBin* bin = StreamBin::get_ptr(binHandle);
        hasConnections = bin->has_clients_connected();

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::cycle_bin_buffers(sensekit_bin_t binHandle,
                                                       sensekit_frame_t*& binBuffer)
    {
        assert(binHandle != nullptr);

        StreamBin* bin = StreamBin::get_ptr(binHandle);
        binBuffer = bin->cycle_buffers();

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::link_connection_to_bin(sensekit_streamconnection_t connection,
                                                            sensekit_bin_t binHandle)
    {
        StreamConnection* underlyingConnection = StreamConnection::get_ptr(connection);
        StreamBin* bin = StreamBin::get_ptr(binHandle);

        Stream* stream = underlyingConnection->get_stream();
        if (binHandle != nullptr)
        {
            m_logger.info("linking connection to bin -- stream: %x type: %d conn: %x bin: %x",
                          stream->get_handle(),
                          stream->get_description().type,
                          connection,
                          bin);
        }
        else
        {
            m_logger.info("unlinking connection to bin -- stream: %x type: %d conn: %x",
                          stream->get_handle(),
                          stream->get_description().type,
                          connection);
        }

        underlyingConnection->set_bin(bin);

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::get_parameter_bin(size_t byteSize,
                                                       sensekit_parameter_bin_t& binHandle,
                                                       sensekit_parameter_data_t& parameterData)
    {
        //TODO pooling
        ParameterBin* parameterBin = new ParameterBin(byteSize);

        binHandle = parameterBin->get_handle();
        parameterData = parameterBin->data();

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::log(const char* channel,
                                         sensekit_log_severity_t logLevel,
                                         const char* format,
                                         va_list args)
    {
        sensekit::log_vargs(channel, logLevel, format, args);
        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::register_host_event_callback(host_event_callback_t callback,
                                                                  void* clientTag,
                                                                  CallbackId& callbackId)
    {
        auto thunk = [clientTag, callback](sensekit_event_id id, const void* data, size_t dataSize)
            {
                callback(clientTag, id, data, dataSize);
            };

        callbackId = m_hostEventSignal += thunk;

        //m_context.raise_existing_device_unavailable(callback, clientTag);

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t PluginService::unregister_host_event_callback(CallbackId callbackId)
    {
        m_hostEventSignal -= callbackId;

        return SENSEKIT_STATUS_SUCCESS;
    }
}
