#include "SenseKitContext.h"
#include "Core/shared_library.h"
#include "Core/libs.h"
#include "StreamReader.h"
#include "StreamConnection.h"
#include "StreamServiceDelegate.h"
#include <Plugins/StreamServiceProxyBase.h>
#include <Plugins/PluginServiceProxyBase.h>
#include <SenseKitAPI.h>
#include "CreateStreamProxy.h"
#include "sensekit_internal.h"

namespace sensekit {

    sensekit_status_t SenseKitContext::initialize()
    {
        if (m_initialized)
            return SENSEKIT_STATUS_SUCCESS;

        m_pluginServiceProxy = m_pluginService.create_proxy();
        m_streamServiceProxy = create_stream_proxy(this);

        //TODO: OMG ERROR HANDLING
        LibHandle libHandle = nullptr;

        for(auto lib : get_libs())
        {
            os_load_library((PLUGIN_DIRECTORY + lib).c_str(), libHandle);

            PluginFuncs pluginFuncs;
            os_get_proc_address(libHandle, SK_STRINGIFY(sensekit_plugin_initialize), (FarProc&)pluginFuncs.initialize);
            os_get_proc_address(libHandle, SK_STRINGIFY(sensekit_plugin_terminate), (FarProc&)pluginFuncs.terminate);
            os_get_proc_address(libHandle, SK_STRINGIFY(sensekit_plugin_update), (FarProc&)pluginFuncs.update);

            if (pluginFuncs.isValid())
            {
                pluginFuncs.initialize(m_pluginServiceProxy);
                m_pluginList.push_back(pluginFuncs);
            }
            else
            {
                os_free_library(libHandle);
            }
        }

        sensekit_api_set_proxy(get_streamServiceProxy());

        m_initialized = true;

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::terminate()
    {
        if (!m_initialized)
            return SENSEKIT_STATUS_SUCCESS;

        m_readers.clear();

        for(auto pluginFuncs : m_pluginList)
        {
            pluginFuncs.terminate();
        }

        if (m_pluginServiceProxy)
            delete m_pluginServiceProxy;

        if (m_streamServiceProxy)
            delete m_streamServiceProxy;

        m_initialized = false;

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::streamset_open(const char* uri, sensekit_streamset_t& streamSet)
    {
        streamSet = get_rootSet().get_handle();

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::streamset_close(sensekit_streamset_t& streamSet)
    {
        streamSet = nullptr;

        return SENSEKIT_STATUS_SUCCESS;
    }

    char* SenseKitContext::get_status_string(sensekit_status_t status)
    {
        //TODO
        return nullptr;
    }

    sensekit_status_t SenseKitContext::reader_create(sensekit_streamset_t streamSet,
                                                     sensekit_reader_t& reader)
    {
        assert(streamSet != nullptr);

        StreamSet* actualSet = StreamSet::get_ptr(streamSet);
        std::unique_ptr<StreamReader> actualReader(new StreamReader(*actualSet));

        reader = actualReader->get_handle();

        m_readers.push_back(std::move(actualReader));

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::reader_destroy(sensekit_reader_t& reader)
    {
        assert(reader != nullptr);

        StreamReader* actualReader = StreamReader::get_ptr(reader);

        auto it = std::find_if(m_readers.begin(),
                               m_readers.end(),
                               [&actualReader] (ReaderPtr& element)
                               -> bool
                               {
                                   return actualReader == element.get();
                               });

        if (it != m_readers.end())
        {
            m_readers.erase(it);
        }

        reader = nullptr;

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::reader_get_stream(sensekit_reader_t reader,
                                                         sensekit_stream_type_t type,
                                                         sensekit_stream_subtype_t subType,
                                                         sensekit_streamconnection_t& connection)
    {
        assert(reader != nullptr);

        StreamReader* actualReader = StreamReader::get_ptr(reader);

        sensekit_stream_desc_t desc;
        desc.type = type;
        desc.subType = subType;

        connection = actualReader->get_stream(desc)->get_handle();

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::stream_get_description(sensekit_streamconnection_t connection,
                                                              sensekit_stream_desc_t* description)
    {
        *description = StreamConnection::get_ptr(connection)->get_description();

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::stream_start(sensekit_streamconnection_t connection)
    {
        assert(connection != nullptr);
        assert(connection->handle != nullptr);

        StreamConnection* actualConnection = StreamConnection::get_ptr(connection);

        actualConnection->start();

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::stream_stop(sensekit_streamconnection_t connection)
    {
        assert(connection != nullptr);
        assert(connection->handle != nullptr);

        StreamConnection* actualConnection = StreamConnection::get_ptr(connection);

        actualConnection->stop();

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::reader_open_frame(sensekit_reader_t reader,
                                                         int timeoutMillis,
                                                         sensekit_reader_frame_t& frame)
    {
        assert(reader != nullptr);

        StreamReader* actualReader = StreamReader::get_ptr(reader);
        sensekit_status_t rc = actualReader->lock(timeoutMillis);

        if (rc == SENSEKIT_STATUS_SUCCESS)
        {
            frame = reader;
        }
        else
        {
            frame = nullptr;
        }
        return rc;
    }

    sensekit_status_t SenseKitContext::reader_close_frame(sensekit_reader_frame_t& frame)
    {
        assert(frame != nullptr);

        StreamReader* actualReader = StreamReader::from_frame(frame);
        actualReader->unlock();

        frame = nullptr;

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::reader_register_frame_ready_callback(sensekit_reader_t reader,
                                                                            FrameReadyCallback callback,
                                                                            sensekit_reader_callback_id_t& callbackId)
    {
        assert(reader != nullptr);
        callbackId = nullptr;

        StreamReader* actualReader = StreamReader::get_ptr(reader);

        CallbackId cbId = actualReader->register_frame_ready_callback(callback);

        sensekit_reader_callback_id_raw_t* cb = new sensekit_reader_callback_id_raw_t;
        callbackId = cb;

        cb->reader = reader;
        cb->callbackId = cbId;

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::reader_unregister_frame_ready_callback(sensekit_reader_callback_id_t& callbackId)
    {
        sensekit_reader_callback_id_raw_t* cb = callbackId;
        assert(cb != nullptr);
        assert(cb->reader != nullptr);

        CallbackId cbId = cb->callbackId;
        StreamReader* actualReader = StreamReader::get_ptr(cb->reader);

        delete cb;
        callbackId = nullptr;
        actualReader->unregister_frame_ready_callback(cbId);

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::reader_get_frame(sensekit_reader_frame_t frame,
                                                        sensekit_stream_type_t type,
                                                        sensekit_stream_subtype_t subType,
                                                        sensekit_frame_ref_t*& frameRef)
    {
        assert(frame != nullptr);

        StreamReader* actualReader = StreamReader::from_frame(frame);

        sensekit_stream_desc_t desc;
        desc.type = type;
        desc.subType = subType;

        frameRef = actualReader->get_subframe(desc);

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::temp_update()
    {
        for(auto plinfo : m_pluginList)
        {
            if (plinfo.update)
                plinfo.update();
        }

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::stream_set_parameter(sensekit_streamconnection_t connection,
                                                            sensekit_parameter_id parameterId,
                                                            size_t byteLength,
                                                            sensekit_parameter_data_t* data)
    {
        assert(connection != nullptr);
        assert(connection->handle != nullptr);

        StreamConnection* actualConnection = StreamConnection::get_ptr(connection);
        actualConnection->set_parameter(parameterId, byteLength, data);

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::stream_get_parameter_size(sensekit_streamconnection_t connection,
                                                                 sensekit_parameter_id parameterId,
                                                                 size_t& byteLength)
    {
        assert(connection != nullptr);
        assert(connection->handle != nullptr);

        StreamConnection* actualConnection = StreamConnection::get_ptr(connection);

        actualConnection->get_parameter_size(parameterId, byteLength);

        return SENSEKIT_STATUS_SUCCESS;
    }

    sensekit_status_t SenseKitContext::stream_get_parameter_data(sensekit_streamconnection_t connection,
                                                                 sensekit_parameter_id parameterId,
                                                                 size_t byteLength,
                                                                 sensekit_parameter_data_t* data)
    {
        assert(connection != nullptr);
        assert(connection->handle != nullptr);

        StreamConnection* actualConnection = StreamConnection::get_ptr(connection);
        actualConnection->get_parameter_data(parameterId, byteLength, data);

        return SENSEKIT_STATUS_SUCCESS;
    }
}
