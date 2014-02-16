// This file has been automatically generated by "kdu_hyperdoc"
// Do not edit manually.
// Copyright 2001, David Taubman, The University of New South Wales (UNSW)
#ifndef KDU_AUX_H

// Public header files used by "kdu_aux.dll" (or "kdu_aux.h")
#include "jp2.h"
#include "jpb.h"
#include "jpx.h"
#include "kdu_cache.h"
#include "kdu_client.h"
#include "kdu_client_window.h"
#include "kdu_clientx.h"
#include "kdu_file_io.h"
#include "kdu_video_io.h"
#include "mj2.h"
#include "kdu_tiff.h"
#include "kdu_serve.h"
#include "kdu_servex.h"
#include "kdu_region_compositor.h"
#include "kdu_region_decompressor.h"
#include "kdu_stripe_compressor.h"
#include "kdu_stripe_decompressor.h"
#include "kdu_arch.h"
#include "kdu_block_coding.h"
#include "kdu_compressed.h"
#include "kdu_messaging.h"
#include "kdu_params.h"
#include "kdu_roi_processing.h"
#include "kdu_sample_processing.h"
#include "kdu_threads.h"
#include "kdu_ubiquitous.h"
#include "kdu_utils.h"

/* ============================================ */
// Special classes for use in binding callbacks
/* ============================================ */

//---------------------------------------------------
// Delegator for class: kdu_cache
class _aux_delegator__kdu_cache {
  public:
    virtual ~_aux_delegator__kdu_cache() { return; }
    virtual void acquire_lock() { return; }
    virtual void release_lock() { return; }
  };

//---------------------------------------------------
// Derived version of class: kdu_cache
class _aux_extended__kdu_cache : public kdu_cache {
  public:
    _aux_delegator__kdu_cache * _delegator;
  public:
    KDU_AUX_EXPORT _aux_extended__kdu_cache();
    virtual void  acquire_lock()
      { _delegator->acquire_lock(); }
    virtual void  release_lock()
      { _delegator->release_lock(); }
  };

//---------------------------------------------------
// Delegator for class: kdu_client_notifier
class _aux_delegator__kdu_client_notifier {
  public:
    virtual ~_aux_delegator__kdu_client_notifier() { return; }
    virtual void notify() { return; }
  };

//---------------------------------------------------
// Derived version of class: kdu_client_notifier
class _aux_extended__kdu_client_notifier : public kdu_client_notifier {
  public:
    _aux_delegator__kdu_client_notifier * _delegator;
  public:
    KDU_AUX_EXPORT _aux_extended__kdu_client_notifier();
    virtual void  notify()
      { _delegator->notify(); }
  };

//---------------------------------------------------
// Delegator for class: kdu_compressed_source_nonnative
class _aux_delegator__kdu_compressed_source_nonnative {
  public:
    virtual ~_aux_delegator__kdu_compressed_source_nonnative() { return; }
    virtual int get_capabilities() { return (int) 0; }
    virtual bool seek(kdu_long offset) { return (bool) 0; }
    virtual kdu_long get_pos() { return (kdu_long) 0; }
    virtual bool set_tileheader_scope(int tnum, int num_tiles) { return (bool) 0; }
    virtual bool set_precinct_scope(kdu_long unique_id) { return (bool) 0; }
    virtual int post_read(int num_bytes) { return (int) 0; }
  };

//---------------------------------------------------
// Derived version of class: kdu_compressed_source_nonnative
class _aux_extended__kdu_compressed_source_nonnative : public kdu_compressed_source_nonnative {
  public:
    _aux_delegator__kdu_compressed_source_nonnative * _delegator;
  public:
    KDU_AUX_EXPORT _aux_extended__kdu_compressed_source_nonnative();
    virtual int  get_capabilities()
      { return _delegator->get_capabilities(); }
    virtual bool  seek(kdu_long offset)
      { return _delegator->seek(offset); }
    virtual kdu_long  get_pos()
      { return _delegator->get_pos(); }
    virtual bool  set_tileheader_scope(int tnum, int num_tiles)
      { return _delegator->set_tileheader_scope(tnum,num_tiles); }
    virtual bool  set_precinct_scope(kdu_long unique_id)
      { return _delegator->set_precinct_scope(unique_id); }
    virtual int  post_read(int num_bytes)
      { return _delegator->post_read(num_bytes); }
  };

//---------------------------------------------------
// Delegator for class: kdu_compressed_target_nonnative
class _aux_delegator__kdu_compressed_target_nonnative {
  public:
    virtual ~_aux_delegator__kdu_compressed_target_nonnative() { return; }
    virtual int get_capabilities() { return (int) 0; }
    virtual void start_tileheader(int tnum, int num_tiles) { return; }
    virtual void end_tileheader(int tnum) { return; }
    virtual void start_precinct(kdu_long unique_id) { return; }
    virtual void post_end_precinct(int num_packets) { return; }
    virtual bool start_rewrite(kdu_long backtrack) { return (bool) 0; }
    virtual bool end_rewrite() { return (bool) 0; }
    virtual void set_target_size(kdu_long num_bytes) { return; }
    virtual bool post_write(int num_bytes) { return (bool) 0; }
  };

//---------------------------------------------------
// Derived version of class: kdu_compressed_target_nonnative
class _aux_extended__kdu_compressed_target_nonnative : public kdu_compressed_target_nonnative {
  public:
    _aux_delegator__kdu_compressed_target_nonnative * _delegator;
  public:
    KDU_AUX_EXPORT _aux_extended__kdu_compressed_target_nonnative();
    virtual int  get_capabilities()
      { return _delegator->get_capabilities(); }
    virtual void  start_tileheader(int tnum, int num_tiles)
      { _delegator->start_tileheader(tnum,num_tiles); }
    virtual void  end_tileheader(int tnum)
      { _delegator->end_tileheader(tnum); }
    virtual void  start_precinct(kdu_long unique_id)
      { _delegator->start_precinct(unique_id); }
    virtual void  post_end_precinct(int num_packets)
      { _delegator->post_end_precinct(num_packets); }
    virtual bool  start_rewrite(kdu_long backtrack)
      { return _delegator->start_rewrite(backtrack); }
    virtual bool  end_rewrite()
      { return _delegator->end_rewrite(); }
    virtual void  set_target_size(kdu_long num_bytes)
      { _delegator->set_target_size(num_bytes); }
    virtual bool  post_write(int num_bytes)
      { return _delegator->post_write(num_bytes); }
  };

//---------------------------------------------------
// Delegator for class: kdu_message
class _aux_delegator__kdu_message {
  public:
    virtual ~_aux_delegator__kdu_message() { return; }
    virtual void put_text(const char * string) { return; }
    virtual void flush(bool end_of_message) { return; }
    virtual void start_message() { return; }
  };

//---------------------------------------------------
// Derived version of class: kdu_message
class _aux_extended__kdu_message : public kdu_message {
  public:
    _aux_delegator__kdu_message * _delegator;
  public:
    KDU_AUX_EXPORT _aux_extended__kdu_message();
    virtual void  put_text(const char * string)
      { _delegator->put_text(string); }
    virtual void  flush(bool end_of_message)
      { _delegator->flush(end_of_message); }
    virtual void  start_message()
      { _delegator->start_message(); }
  };

//---------------------------------------------------
// Delegator for class: kdu_region_compositor
class _aux_delegator__kdu_region_compositor {
  public:
    virtual ~_aux_delegator__kdu_region_compositor() { return; }
    virtual bool custom_paint_overlay(kdu_compositor_buf *buffer, kdu_dims buffer_region, kdu_dims bounding_region, jpx_metanode node, kdu_overlay_params *painting_params, kdu_coords image_offset, kdu_coords subsampling, bool transpose, bool vflip, bool hflip, kdu_coords expansion_numerator, kdu_coords expansion_denominator, kdu_coords compositing_offset) { return (bool) 0; }
    virtual kdu_compositor_buf *allocate_buffer(kdu_coords min_size, kdu_coords &actual_size, bool read_access_required) { return NULL; }
    virtual void delete_buffer(kdu_compositor_buf *buf) { return; }
  };

//---------------------------------------------------
// Derived version of class: kdu_region_compositor
class _aux_extended__kdu_region_compositor : public kdu_region_compositor {
  public:
    _aux_delegator__kdu_region_compositor * _delegator;
  public:
    KDU_AUX_EXPORT _aux_extended__kdu_region_compositor(kdu_thread_env *env=NULL, kdu_thread_queue *env_queue=NULL);
    KDU_AUX_EXPORT _aux_extended__kdu_region_compositor(kdu_compressed_source *source, int persistent_cache_threshold=256000);
    KDU_AUX_EXPORT _aux_extended__kdu_region_compositor(jpx_source *source, int persistent_cache_threshold=256000);
    KDU_AUX_EXPORT _aux_extended__kdu_region_compositor(mj2_source *source, int persistent_cache_threshold=256000);
    virtual bool  custom_paint_overlay(kdu_compositor_buf *buffer, kdu_dims buffer_region, kdu_dims bounding_region, jpx_metanode node, kdu_overlay_params *painting_params, kdu_coords image_offset, kdu_coords subsampling, bool transpose, bool vflip, bool hflip, kdu_coords expansion_numerator, kdu_coords expansion_denominator, kdu_coords compositing_offset)
      { return _delegator->custom_paint_overlay(buffer,buffer_region,bounding_region,node,painting_params,image_offset,subsampling,transpose,vflip,hflip,expansion_numerator,expansion_denominator,compositing_offset); }
    virtual kdu_compositor_buf * allocate_buffer(kdu_coords min_size, kdu_coords &actual_size, bool read_access_required)
      { return _delegator->allocate_buffer(min_size,actual_size,read_access_required); }
    virtual void  delete_buffer(kdu_compositor_buf *buf)
      { _delegator->delete_buffer(buf); }
  };

//---------------------------------------------------
// Delegator for class: kdu_thread_safe_message
class _aux_delegator__kdu_thread_safe_message {
  public:
    virtual ~_aux_delegator__kdu_thread_safe_message() { return; }
    virtual void flush(bool end_of_message) { return; }
    virtual void start_message() { return; }
  };

//---------------------------------------------------
// Derived version of class: kdu_thread_safe_message
class _aux_extended__kdu_thread_safe_message : public kdu_thread_safe_message {
  public:
    _aux_delegator__kdu_thread_safe_message * _delegator;
  public:
    KDU_AUX_EXPORT _aux_extended__kdu_thread_safe_message();
    virtual void  flush(bool end_of_message)
      { _delegator->flush(end_of_message); }
    virtual void  start_message()
      { _delegator->start_message(); }
  };

#endif // KDU_AUX_H
