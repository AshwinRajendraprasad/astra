/* THIS FILE AUTO-GENERATED FROM CreatePluginProxy.h.lpp. DO NOT EDIT. */
#ifndef CREATEPLUGINPROXY_H
#define CREATEPLUGINPROXY_H

#include "PluginService.h"
#include "PluginServiceDelegate.h"
#include <Plugins/PluginServiceProxyBase.h>

namespace sensekit {
PluginServiceProxyBase* create_plugin_proxy(PluginService* service)
    {
        PluginServiceProxyBase* proxy = new PluginServiceProxyBase;

        proxy->register_stream_added_callback = &PluginServiceDelegate::register_stream_added_callback;
        proxy->register_stream_removing_callback = &PluginServiceDelegate::register_stream_removing_callback;
        proxy->unregister_stream_added_callback = &PluginServiceDelegate::unregister_stream_added_callback;
        proxy->unregister_stream_removing_callback = &PluginServiceDelegate::unregister_stream_removing_callback;
        proxy->create_stream_set = &PluginServiceDelegate::create_stream_set;
        proxy->destroy_stream_set = &PluginServiceDelegate::destroy_stream_set;
        proxy->create_stream = &PluginServiceDelegate::create_stream;
        proxy->destroy_stream = &PluginServiceDelegate::destroy_stream;
        proxy->create_stream_bin = &PluginServiceDelegate::create_stream_bin;
        proxy->destroy_stream_bin = &PluginServiceDelegate::destroy_stream_bin;
        proxy->cycle_bin_buffers = &PluginServiceDelegate::cycle_bin_buffers;
        proxy->link_connection_to_bin = &PluginServiceDelegate::link_connection_to_bin;
        proxy->pluginService = service;

        return proxy;
    }
}

#endif /* CREATEPLUGINPROXY_H */