#pragma once

#include <musikcore/plugin.h>
#include <musikcore/plugin/analyzer.h>
#include <musikcore/plugin/decoder.h>
#include <musikcore/plugin/dsp.h>
#include <musikcore/plugin/encoder.h>
#include <musikcore/plugin/indexer.h>
#include <musikcore/plugin/output.h>
#include <musikcore/plugin/settings.h>
#include <musikcore/plugin/stream.h>
#include <musikcore/plugin/tag.h>

namespace musik { namespace core { namespace plugin {

    namespace detail {
        template<typename Table, size_t N>
        mc_result RegisterTable(const mc_host_api& host, const char (&id)[N], const Table& table) {
            mc_plugin_capability capability = MC_PLUGIN_CAPABILITY_DEFAULT;
            capability.id = { id, N - 1 };
            capability.api = &table;
            capability.api_size = sizeof(table);
            return host.register_plugin_capability(host.host_context, &capability);
        }
    }

    /* use static tables or plugin-owned state; stack tables cannot outlive shutdown. */
    inline mc_result RegisterStream(const mc_host_api& host, const mc_stream_factory& factory) {
        return detail::RegisterTable(host, MC_PLUGIN_CAPABILITY_STREAM, factory);
    }

    inline mc_result RegisterDecoder(const mc_host_api& host, const mc_decoder_factory& factory) {
        return detail::RegisterTable(host, MC_PLUGIN_CAPABILITY_DECODER, factory);
    }

    inline mc_result RegisterOutput(const mc_host_api& host, const mc_output_factory& factory) {
        return detail::RegisterTable(host, MC_PLUGIN_CAPABILITY_OUTPUT, factory);
    }

    inline mc_result RegisterDsp(const mc_host_api& host, const mc_dsp_factory& factory) {
        return detail::RegisterTable(host, MC_PLUGIN_CAPABILITY_DSP, factory);
    }

    inline mc_result RegisterEncoder(const mc_host_api& host, const mc_encoder_factory& factory) {
        return detail::RegisterTable(host, MC_PLUGIN_CAPABILITY_ENCODER, factory);
    }

    inline mc_result RegisterTagReader(const mc_host_api& host, const mc_tag_reader_factory& factory) {
        return detail::RegisterTable(host, MC_PLUGIN_CAPABILITY_TAG_READER, factory);
    }

    inline mc_result RegisterAnalyzer(const mc_host_api& host, const mc_analyzer_factory& factory) {
        return detail::RegisterTable(host, MC_PLUGIN_CAPABILITY_ANALYZER, factory);
    }

    inline mc_result RegisterIndexerSource(const mc_host_api& host, const mc_indexer_source_factory& factory) {
        return detail::RegisterTable(host, MC_PLUGIN_CAPABILITY_INDEXER_SOURCE, factory);
    }

    inline mc_result RegisterConfiguration(const mc_host_api& host, const mc_configuration_api& configuration) {
        return detail::RegisterTable(host, MC_PLUGIN_CAPABILITY_CONFIGURATION, configuration);
    }

    /* the host keeps each table, so a temporary would immediately dangle. */
    mc_result RegisterStream(const mc_host_api&, const mc_stream_factory&&) = delete;
    mc_result RegisterDecoder(const mc_host_api&, const mc_decoder_factory&&) = delete;
    mc_result RegisterOutput(const mc_host_api&, const mc_output_factory&&) = delete;
    mc_result RegisterDsp(const mc_host_api&, const mc_dsp_factory&&) = delete;
    mc_result RegisterEncoder(const mc_host_api&, const mc_encoder_factory&&) = delete;
    mc_result RegisterTagReader(const mc_host_api&, const mc_tag_reader_factory&&) = delete;
    mc_result RegisterAnalyzer(const mc_host_api&, const mc_analyzer_factory&&) = delete;
    mc_result RegisterIndexerSource(const mc_host_api&, const mc_indexer_source_factory&&) = delete;
    mc_result RegisterConfiguration(const mc_host_api&, const mc_configuration_api&&) = delete;

} } }
