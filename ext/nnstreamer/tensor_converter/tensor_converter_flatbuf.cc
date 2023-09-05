/* SPDX-License-Identifier: LGPL-2.1-only */
/**
 * GStreamer / NNStreamer tensor_converter subplugin, "Flatbuffer"
 * Copyright (C) 2020 Gichan Jang <gichan2.jang@samsung.com>
 */
/**

* @file        tensor_converter_flatbuf.cc
* @date        14 May 2020
* @brief       NNStreamer tensor-converter subplugin, "flatbuffer",
*              which converts flatbuufers byte stream to tensors.
* @see         https://github.com/nnstreamer/nnstreamer
* @author      Gichan Jang <gichan2.jang@samsung.com>
* @bug         No known bugs except for NYI items
*
*/

/**
 * Install flatbuffers
 * We assume that you use Ubuntu linux distribution.
 * You may simply download binary packages from PPA
 *
 * $ sudo apt-add-repository ppa:nnstreamer
 * $ sudo apt update
 * $ sudo apt install libflatbuffers libflatbuffers-dev flatbuffers-compiler
 */

#include <fstream>
#include <glib.h>
#include <gst/gstinfo.h>
#include <iostream>
#include <nnstreamer_generated.h> /* Generated by `flatc`. */
#include <nnstreamer_log.h>
#include <nnstreamer_plugin_api.h>
#include <nnstreamer_plugin_api_converter.h>
#include <nnstreamer_util.h>
#include <typeinfo>
#include "../extra/nnstreamer_flatbuf.h"
#include "tensor_converter_util.h"

namespace nnstreamer
{
namespace flatbuf
{
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
void init_fbc (void) __attribute__ ((constructor));
void fini_fbc (void) __attribute__ ((destructor));
#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @brief tensor converter plugin's NNStreamerExternalConverter callback */
static GstCaps *
fbc_query_caps (const GstTensorsConfig *config)
{
  UNUSED (config);
  return gst_caps_from_string (GST_FLATBUF_TENSOR_CAP_DEFAULT);
}

/** @brief tensor converter plugin's NNStreamerExternalConverter callback
 *  @todo : Consider multi frames, return Bufferlist and
 *          remove frame size and the number of frames
 */
static GstBuffer *
fbc_convert (GstBuffer *in_buf, GstTensorsConfig *config, void *priv_data)
{
  const Tensors *tensors;
  const flatbuffers::Vector<flatbuffers::Offset<Tensor>> *tensor;
  const flatbuffers::Vector<unsigned char> *tensor_data;
  frame_rate fr;
  GstBuffer *out_buf = NULL;
  GstMemory *in_mem, *out_mem;
  GstMapInfo in_info;
  guint mem_size;
  GstTensorInfo *_info;

  UNUSED (priv_data);

  if (!in_buf || !config) {
    ml_loge ("NULL parameter is passed to tensor_converter::flatbuf");
    return NULL;
  }

  in_mem = gst_buffer_peek_memory (in_buf, 0);
  if (!gst_memory_map (in_mem, &in_info, GST_MAP_READ)) {
    nns_loge ("Cannot map input memory / tensor_converter::flatbuf");
    return NULL;
  }

  tensors = GetTensors (in_info.data);
  g_assert (tensors);

  config->info.num_tensors = tensors->num_tensor ();
  config->info.format = (tensor_format) tensors->format ();

  if (tensors->num_tensor () > NNS_TENSOR_SIZE_LIMIT) {
    nns_loge ("The number of tensors is limited to %d", NNS_TENSOR_SIZE_LIMIT);
    goto done;
  }
  config->rate_n = tensors->fr ()->rate_n ();
  config->rate_d = tensors->fr ()->rate_d ();

  tensor = tensors->tensor ();
  out_buf = gst_buffer_new ();

  for (guint i = 0; i < config->info.num_tensors; i++) {
    gsize offset;
    std::string _name = tensor->Get (i)->name ()->str ();
    const gchar *name = _name.c_str ();

    _info = gst_tensors_info_get_nth_info (&config->info, i);

    g_free (_info->name);
    _info->name = (name && strlen (name) > 0) ? g_strdup (name) : NULL;
    _info->type = (tensor_type) tensor->Get (i)->type ();
    tensor_data = tensor->Get (i)->data ();

    for (guint j = 0; j < NNS_TENSOR_RANK_LIMIT; j++) {
      _info->dimension[j] = tensor->Get (i)->dimension ()->Get (j);
    }
    mem_size = VectorLength (tensor_data);

    offset = tensor_data->data () - in_info.data;

    out_mem = gst_memory_share (in_mem, offset, mem_size);

    gst_buffer_append_memory (out_buf, out_mem);
  }

  /** copy timestamps */
  gst_buffer_copy_into (
      out_buf, in_buf, (GstBufferCopyFlags) GST_BUFFER_COPY_METADATA, 0, -1);
done:
  gst_memory_unmap (in_mem, &in_info);

  return out_buf;
}

static const gchar converter_subplugin_flatbuf[] = "flatbuf";

/** @brief flatbuffer tensor converter sub-plugin NNStreamerExternalConverter instance */
static NNStreamerExternalConverter flatBuf = { .name = converter_subplugin_flatbuf,
  .convert = fbc_convert,
  .get_out_config = tcu_get_out_config,
  .query_caps = fbc_query_caps,
  .open = NULL,
  .close = NULL };

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
/** @brief Initialize this object for tensor converter sub-plugin */
void
init_fbc (void)
{
  registerExternalConverter (&flatBuf);
}

/** @brief Destruct this object for tensor converter sub-plugin */
void
fini_fbc (void)
{
  unregisterExternalConverter (flatBuf.name);
}
#ifdef __cplusplus
}
#endif /* __cplusplus */

}; /* Namespace flatbuf */
}; /* Namespace nnstreamer */
