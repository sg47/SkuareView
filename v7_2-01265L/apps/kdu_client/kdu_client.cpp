/*****************************************************************************/
// File: kdu_client.cpp [scope = APPS/KDU_CLIENT]
// Version: Kakadu, V7.2
// Author: David Taubman
// Last Revised: 17 January, 2013
/*****************************************************************************/
// Copyright 2001, David Taubman, The University of New South Wales (UNSW)
// The copyright owner is Unisearch Ltd, Australia (commercial arm of UNSW)
// Neither this copyright statement, nor the licensing details below
// may be removed from this file or dissociated from its contents.
/*****************************************************************************/
// Licensee: International Centre For Radio Astronomy Research, Uni of WA
// License number: 01265
// The licensee has been granted a UNIVERSITY LIBRARY license to the
// contents of this source file.  A brief summary of this license appears
// below.  This summary is not to be relied upon in preference to the full
// text of the license agreement, accepted at purchase of the license.
// 1. The License is for University libraries which already own a copy of
//    the book, "JPEG2000: Image compression fundamentals, standards and
//    practice," (Taubman and Marcellin) published by Kluwer Academic
//    Publishers.
// 2. The Licensee has the right to distribute copies of the Kakadu software
//    to currently enrolled students and employed staff members of the
//    University, subject to their agreement not to further distribute the
//    software or make it available to unlicensed parties.
// 3. Subject to Clause 2, the enrolled students and employed staff members
//    of the University have the right to install and use the Kakadu software
//    and to develop Applications for their own use, in their capacity as
//    students or staff members of the University.  This right continues
//    only for the duration of enrollment or employment of the students or
//    staff members, as appropriate.
// 4. The enrolled students and employed staff members of the University have the
//    right to Deploy Applications built using the Kakadu software, provided
//    that such Deployment does not result in any direct or indirect financial
//    return to the students and staff members, the Licensee or any other
//    Third Party which further supplies or otherwise uses such Applications.
// 5. The Licensee, its students and staff members have the right to distribute
//    Reusable Code (including source code and dynamically or statically linked
//    libraries) to a Third Party, provided the Third Party possesses a license
//    to use the Kakadu software, and provided such distribution does not
//    result in any direct or indirect financial return to the Licensee,
//    students or staff members.  This right continues only for the
//    duration of enrollment or employment of the students or staff members,
//    as appropriate.
/******************************************************************************
Description:
  Implements a compressed data source, derived from "kdu_cache" which
interacts with a server using the protocol proposed by UNSW to the JPIP
(JPEG2000 Interactive Imaging Protocol) working group of ISO/IEC JTC1/SC29/WG1
******************************************************************************/

#include <assert.h>
#include "client_local.h"
#include "kdu_messaging.h"
#include "kdu_utils.h"

/* Note Carefully:
      If you want to be able to use the "kdu_text_extractor" tool to
   extract text from calls to `kdu_error' and `kdu_warning' so that it
   can be separately registered (possibly in a variety of different
   languages), you should carefully preserve the form of the definitions
   below, starting from #ifdef KDU_CUSTOM_TEXT and extending to the
   definitions of KDU_WARNING_DEV and KDU_ERROR_DEV.  All of these
   definitions are expected by the current, reasonably inflexible
   implementation of "kdu_text_extractor".
      The only things you should change when these definitions are ported to
   different source files are the strings found inside the `kdu_error'
   and `kdu_warning' constructors.  These strings may be arbitrarily
   defined, as far as "kdu_text_extractor" is concerned, except that they
   must not occupy more than one line of text.
*/
#ifdef KDU_CUSTOM_TEXT
#  define KDU_ERROR(_name,_id) \
     kdu_error _name("E(kdu_client.cpp)",_id);
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("W(kdu_client.cpp)",_id);
#  define KDU_TXT(_string) "<#>" // Special replacement pattern
#else // !KDU_CUSTOM_TEXT
#  define KDU_ERROR(_name,_id) \
     kdu_error _name("Error in Kakadu Client:\n");
#  define KDU_WARNING(_name,_id) \
     kdu_warning _name("Warning in Kakadu Client:\n");
#  define KDU_TXT(_string) _string
#endif // !KDU_CUSTOM_TEXT

#define KDU_ERROR_DEV(_name,_id) KDU_ERROR(_name,_id)
 // Use the above version for errors which are of interest only to developers
#define KDU_WARNING_DEV(_name,_id) KDU_WARNING(_name,_id)
 // Use the above version for warnings which are of interest only to developers


/* ========================================================================= */
/*                             Internal Functions                            */
/* ========================================================================= */

/*****************************************************************************/
/* STATIC                       make_new_string                              */
/*****************************************************************************/

static char *make_new_string(const char *src, int max_copy_chars=-1)
  /* This function returns a newly allocated string whose deletion is the
     caller's responsibility.  If `max_copy_chars' is non-negative, the
     new string consists of at most that number of characters.  A
     null-terminator is automatically appended.  The function generates
     an error if a ridiculously long string is to be copied. */
{
  const char *cp;
  int len, max_len=max_copy_chars;
  if ((max_len < 0) || (max_len > (1<<16))) // A pretty massive string anyway
    max_len = (1 << 16);
  for (len=0, cp=src; *cp != '\0'; cp++, len++)
    if (len == max_len)
      {
        if (max_len != max_copy_chars)
          { KDU_ERROR(e,0x13030902); e <<
            KDU_TXT("Attempting to make an internal copy of a string "
                    "(probably a network supplied name) which is ridiculously "
                    "long (more than 65K characters).  The copy is being "
                    "aborted to avoid potential exploitation by malicious "
                    "network agents.");
          }
        break;
      }
  char *result = new char[len+1];
  memcpy(result,src,(size_t) len);
  result[len] = '\0';
  return result;
}

/*****************************************************************************/
/* STATIC               check_and_extract_port_suffix                        */
/*****************************************************************************/

static void
  check_and_extract_port_suffix(char *server, kdu_uint16 &port)
  /* This function looks for a ":<port>" suffix within the `server'
     string.  If one is found, it then verifies that the suffix is
     not being mistaken for part of an IPv6 literal.  Finally, the function
     verifies that bracketed IP literals are immediately followed by the
     port suffix if there is one.  If this last test fails, `server' is not a
     well formed server/proxy address so an appropriate error message is
     generated through `kdu_error'.  Otherwise, any port suffix is parsed
     and removed from the string, writing the result to the `port' argument.
     The `port' argument is left untouched in the event that no port suffix
     is found. */
{
  char *cp = (char *) strrchr(server,':');
  if ((cp != NULL) && (server[0] == '['))
    { // See if we are mistaking part of an IPv6 address for a port suffix
      const char *delim_p = strchr(server,']');
      if ((delim_p != NULL) && (delim_p > cp))
        cp = NULL;
      else if ((delim_p != NULL) && (delim_p != (cp-1)))
        { KDU_ERROR(e,0x25051001); e <<
          KDU_TXT("Illegal server/proxy address -- bracketed portion of "
                  "address") << ", \"" << server << "\", " <<
          KDU_TXT("suggests an IP literal, which should be followed "
                  "immediately by any \":<port>\" suffix, "
                  "in call to `kdu_client::connect' (or possibly in a "
                  "JPIP-cnew response header).");
        }
    }
        
  int port_val;
  if ((cp != NULL) && (cp > server) && (sscanf(cp+1,"%d",&port_val) == 1))
    { 
      if ((port_val <= 0) || (port_val >= (1<<16)))
        { KDU_ERROR(e,0x06030902); e <<
          KDU_TXT("Illegal port number found in server/proxy address suffix")
          << ", \"" << server << "\", " <<
          KDU_TXT("in call to `kdu_client::connect' (or possibly in a "
                  "JPIP-cnew response header).");
        }
      port = (kdu_uint16) port_val;
      *cp = '\0';
    }
}

/*****************************************************************************/
/* STATIC                    resolve_server_address                          */
/*****************************************************************************/

static void resolve_server_address(const char *server_name_or_ip,
                                   kdcs_sockaddr &address)
  /* Generates an error if the `server_name_or_ip' string cannot be converted
     into an IP address, written to `address'.  Upon return, `address' holds
     a valid internet address, with the port set to 80 -- the caller may
     change this later of course.  You should release any mutex lock while
     calling this function, or else it may suspend the application.  You
     then need to make sure that it is safe to do this -- i.e., make sure
     that the application thread is not able to damage the context in
     which the function was called.
       This function performs hex-hex decoding of `server_name_or_ip' if
     required, using a separately allocated buffer. */
{
  if (!address.init(server_name_or_ip,
                    KDCS_ADDR_FLAG_BRACKETED_LITERALS |
                    KDCS_ADDR_FLAG_ESCAPED_NAMES |
                    KDCS_ADDR_FLAG_NEED_PORT))
    { 
      KDU_ERROR(e,1); e <<
      KDU_TXT("Unable to resolve host address")
      << ", \"" << server_name_or_ip << "\".";
    }
  address.set_port(80);
}

/*****************************************************************************/
/* STATIC                    write_cache_descriptor                          */
/*****************************************************************************/

static void write_cache_descriptor(int cs_idx, bool &cs_started,
                                   const char *bin_class, kdu_long bin_id,
                                   int available_bytes,
                                   bool is_complete,
                                   kdcs_message_block &block)
  /* Utility function used by `signal_model_corrections' to write a
     cache descriptor for a single code-stream.  If `cs_started' is false,
     the code-stream qualifier (`cs_idx') is written first, followed by
     the descriptor itself.  In this case, `cs_started' is set to true
     so that the code-stream qualifier will not need to be written multiple
     times.  If `bin_id' is negative, no data-bin ID is actually written;
     this is appropriate if the `bin_class' string is "Hm". */
{
  assert((cs_idx >= 0) && (available_bytes >= 0));
  if (!cs_started)
    {
      cs_started = true;
      block << "[" << cs_idx << "],";
    }
  
  char buf[20];
  char *start = buf+20;
  kdu_long tmp;
  *(--start) = '\0';
  if (bin_id >= 0)
    {
      while (start > buf)
        {
          tmp = bin_id / 10;
          *(--start) = (char)('0' + (int)(bin_id - tmp*10));
          bin_id = tmp;
          if (bin_id == 0)
            break;
        }
      assert(bin_id == 0);
    }
  block << bin_class << start;
  if (!is_complete)
    block << ":" << available_bytes;
  block << ",";
}

/*****************************************************************************/
/* STATIC                  find_disparity_compensation                       */
/*****************************************************************************/

static kdu_long
  find_disparity_compensation(kdu_long horizon, kdu_long current_disparity,
                              kdu_long outstanding_request_duration,
                              kdu_long outstanding_disparity_compensation)
  /* This is an important utility function that manages a control loop
     whose objective is to ensure that the server's responses to timed
     requests keep up with the client's expectations of when those
     requests should complete.  Timed requests may be served by converting
     the target duration of each request into a byte limit, based on
     current estimates of the return data rate from the server.  Of course,
     these estimates might be wrong, but the data rate estimates are
     themselves updated based on data received from the server, so we do
     not expect any long term bias to exist.  In this case, without any
     compensation for observed disparity between the actual and expected
     completion times for requests, we would expect the disparity to
     resemble a classic "random walk".
        The returned disparity compensation value represents the amount of
     extra time that should notionally be allocated to the request to help
     close the disparity which currently exists.  The `horizon' argument
     represents a lower bound on the target duration of the request (prior
     to adding the disparity compensation) -- normally, `horizon' is
     identical to the target duration of the request, but if the target
     duration is large enough that a byte limited request might exceed the
     maximum number of extra bytes that our flow regulator wants to allow
     onto the channel, the `horizon' value may be somewhat smaller than
     the final target request duration -- basically, this allows the
     disparity compensation to be computed once only, without entering an
     iterative loop.
        `outstanding_request_duration' represents the total target duration of
     requests that are currently outstanding -- i.e., they have been issued
     but they have not yet completed to the point where they affect
     the `current_disparity' value.  This value helps to provide an indication
     of the lag that might be experienced between the point at which we
     introduce disparity compensation and the point at which we can assess
     its effect.  Similarly, `outstanding_disparity_compensation' represents
     the total amount of disparity compensation that has been added to
     this same set of outstanding requests.  All things being equal, we
     expect `current_disparity' to change by this amount in the future if
     this function returns 0. */
{
  kdu_long gap = current_disparity + outstanding_disparity_compensation;
  kdu_long window = horizon + outstanding_request_duration;
  double compensation_fraction = -0.5 * ((double) gap) / ((double) window);
    // This is the fraction of the target request duration that we expect
    // devote to disparity compensation, with a view to closing roughly
    // half the gap over the ensuing `window' microseconds.
  if (compensation_fraction > 0.25)
    compensation_fraction = 0.25; // Avoid growing request lengths too much
  if (compensation_fraction < -0.9)
    compensation_fraction = -0.9; // Can shrink request lengths aggressively to
                                  // address a temporary network dropout.
  kdu_long result = (kdu_long)(0.5 + compensation_fraction * (double) horizon);
  if (result < (8-horizon))
    { // Avoid ridiculously small numbers 
      result = 8-horizon;
      result = (result > 0)?0:result;
    }
  return result;
}

/*****************************************************************************/
/* STATIC                  convert_to_internal_timebase                      */
/*****************************************************************************/

static kdu_long
  convert_to_internal_timebase(kdu_long external_usecs,
                               kdu_long &cum_internal_usecs,
                               kdu_long &cum_external_usecs,
                               kdu_long sync_span_internal,
                               kdu_long sync_span_external)
  /* This function is used to correct preferred service times passed across
     the `kdu_client::post_window' function for differences between the
     application (external) timebase and the internal timebase used to
     interpret preferred service times.  If the internal and external
     clocks are running at exactly the same rate, the function will
     return `external_usecs' as-is, without any adjustments.  If the
     internal clock is running faster by a factor alpha then the function
     should return alpha * `external_usecs'.  The clock running rates are
     judged based upon the ratio between the number of internally and
     externally measured microseconds, as provided by the
     `sync_span_internal' and `sync_span_external' arguments, being
     careful not to assume too much until we have a decent amount of
     time on the board.  The `cum_external_usecs' value keeps track of
     the cumulative sum of the `external_usecs' values, while
     `cum_internal_usecs' keeps track of the cumulative sum of values
     returned by this function.  Ultimately, the function strives to make
     the ratio between `cum_internal_usecs' and `cum_external_usecs'
     equal to that between `sync_span_internal' and `sync_span_external'. */
{
  kdu_long internal_usecs = external_usecs;
  cum_external_usecs += external_usecs;
  if ((sync_span_internal > 500000) && (sync_span_external > 500000))
    { 
      double alpha = ((double)sync_span_internal)/((double)sync_span_external);
      if (alpha < 0.8)
        alpha = 0.8;
      else if (alpha > 1.25)
        alpha = 1.25;
      kdu_long cum_internal_tgt, max_internal_usecs, min_internal_usecs;
      cum_internal_tgt = (kdu_long)(0.5 + alpha * (double) cum_external_usecs);
      max_internal_usecs = (kdu_long)(0.5+1.5*alpha * (double) external_usecs);
      min_internal_usecs = (kdu_long)(0.5+0.7*alpha * (double) external_usecs);
      internal_usecs = cum_internal_tgt - cum_internal_usecs;
      if (internal_usecs > max_internal_usecs)
        internal_usecs = max_internal_usecs;
      else if (internal_usecs < min_internal_usecs)
        internal_usecs = min_internal_usecs;
    }
  cum_internal_usecs += internal_usecs;
  return internal_usecs;
}

/*****************************************************************************/
/* STATIC                  convert_to_external_timebase                      */
/*****************************************************************************/

static kdu_long
  convert_to_external_timebase(kdu_long internal_usecs,
                               kdu_long &cum_internal_usecs,
                               kdu_long &cum_external_usecs,
                               kdu_long sync_span_internal,
                               kdu_long sync_span_external)
  /* Does the opposite of `convert_to_internal_timebase'.  This function
     subtracts the internal and (derived) external number of service
     microseconds from the `cum_internal_usecs' and `cum_external_usecs'
     values, respectively, rather than adding them.  The function is used
     by `kdu_client::trim_timed_requests' to convert the amount of trimmed
     service time back to the external time base and keep all relevant
     state variables up to date. */
{
  kdu_long external_usecs = internal_usecs;
  cum_internal_usecs -= internal_usecs;
  assert(cum_internal_usecs >= 0);
  if ((sync_span_internal > 500000) && (sync_span_external > 500000))
    { 
      double alpha = ((double)sync_span_internal)/((double)sync_span_external);
      if (alpha < 0.8)
        alpha = 0.8;
      else if (alpha > 1.25)
        alpha = 1.25;
      kdu_long cum_external_tgt, max_external_usecs, min_external_usecs;
      cum_external_tgt = (kdu_long)(0.5 + ((double)cum_internal_usecs)/alpha);      
      min_external_usecs=(kdu_long)(0.5+((double)internal_usecs)/(1.5*alpha));
      max_external_usecs=(kdu_long)(0.5+((double)internal_usecs)/(0.7*alpha));
      external_usecs = cum_external_usecs - cum_external_tgt;
      if (external_usecs > max_external_usecs)
        external_usecs = max_external_usecs;
      if (external_usecs < min_external_usecs)
        external_usecs = min_external_usecs;
    }
  cum_external_usecs -= external_usecs;
  if (cum_external_usecs < 0)
    { 
      external_usecs += cum_external_usecs;
      cum_external_usecs = 0;
    }
  return external_usecs;
}


/* ========================================================================= */
/*                            kdu_client_translator                          */
/* ========================================================================= */

/*****************************************************************************/
/*                kdu_client_translator::kdu_client_translator               */
/*****************************************************************************/

kdu_client_translator::kdu_client_translator()
{
  return; // Need this function for foreign language bindings
}

/* ========================================================================= */
/*                             kdc_flow_regulator                            */
/* ========================================================================= */

#define KDC_FLOW_REGULATOR_RENORM_USECS  ((kdu_long) 1000000) /* 1 second */
#define KDC_FLOW_REGULATOR_F pow(3,0.000001)
#define KDC_FLOW_REGULATOR_MU 0.1
#define KDC_FLOW_REGULATOR_MAX_USECS 5000000 /* 5 seconds */
#define KDC_FLOW_REGULATOR_MIN_USECS  500000 /* 0.5 seconds */

/*****************************************************************************/
/*                     kdc_flow_regulator::chunk_received                    */
/*****************************************************************************/

void
  kdc_flow_regulator::chunk_received(int chunk_length,
                                     kdu_long request_issue_time,
                                     kdu_long chunk_received_time,
                                     bool first_in_request,
                                     bool have_more_requests,
                                     bool byte_limited,
                                     kdu_long request_rtt)
{
  // See if the inter-chunk gap includes a pause condition
  bool was_paused = this->potential_pause && first_in_request;
  this->potential_pause = !have_more_requests; // For next time
  
  // Start by updating the channel rate estimation machinery
  kdu_long arrival_gap = 0;  
  if (cumulative_chunk_bytes == 0)
    { // Initialize cumulative bytes and usecs conservatively using initial
      // rate guess.
      cumulative_chunk_usecs = chunk_received_time - request_issue_time;
      if (cumulative_chunk_usecs < 100000)
        cumulative_chunk_usecs = 100000; // Initial bias of at least 0.1seconds
      cumulative_chunk_bytes = 1 + (kdu_long)
        (0.5 + estimated_rate * (double) cumulative_chunk_usecs);
      if (cumulative_chunk_bytes < (kdu_long) chunk_length)
        cumulative_chunk_bytes = chunk_length;
    }
  else if (!was_paused)
    { 
      arrival_gap = chunk_received_time - this->last_chunk_time;
      cumulative_chunk_bytes += chunk_length;
      cumulative_chunk_usecs += arrival_gap;
    }
  if ((cumulative_chunk_usecs > 0) && (cumulative_chunk_bytes > 0))
    { 
      estimated_rate = (((double) cumulative_chunk_bytes) /
                        ((double) cumulative_chunk_usecs));
      while (cumulative_chunk_usecs > KDC_FLOW_REGULATOR_RENORM_USECS)
        { 
          cumulative_chunk_usecs -= (cumulative_chunk_usecs >> 2);
          cumulative_chunk_bytes -= (cumulative_chunk_bytes >> 2);
        }
    }
  this->last_chunk_time = chunk_received_time;
  
  if (first_in_request &&
      (request_issue_time < last_request_time)) // else chunks misordered
    { 
      if (!byte_limited)
        last_request_time = 0;
      
      // See if we can update the Lmax machinery
      if ((last_request_time > 0) && !was_paused)
        { // Otherwise, previous request was either not byte limited or else
          // it was separated from the current request by a "pause" event,
          // meaning that the request became available only after its
          // predecessor completed.  For inter-request times to be meaningful,
          // we need both requests to be byte limited and the second request
          // needs to be issued or available for issuing before all response data
          // from the first request arrived.
          if (disjoint_requests)
            { // In this case, we apply a simple algorithm that aims for a
              // request of Lmax bytes to have a total duration that is
              // 5 times the inter-request gap.
              double target_Lmax = 5.0 * estimated_rate * (double) arrival_gap;
              double update_factor = 0.000003*((double) arrival_gap); 
              update_factor = (update_factor > 0.5)?0.5:update_factor;
              current_Lmax_value += update_factor*(target_Lmax -
                                                   current_Lmax_value);
            }
          else
            { // Evaluate request gaps and apply differential request gap
              // algorithm described in detail in the notes following the
              // definition of `kdc_flow_regulator' in "client_local.h".
              assert(last_request_time > 0); // See outer "if" statement
              kdu_long request_gap = request_issue_time - last_request_time;
              if (this->last_request_gap >= 0)
                { // We can find delta_gap values and use them to adapt Lmax
                  assert(last_arrival_gap >= 0);
                  double delta_request_gap =
                    (double)(request_gap - last_request_gap);
                  double delta_arrival_gap =
                    (double)(arrival_gap - last_arrival_gap);
                  int delta_initial_bytes =
                    chunk_length - last_initial_chunk_bytes;
                  delta_arrival_gap -= delta_initial_bytes / estimated_rate;
                  if (delta_request_gap < 0.0)
                    { 
                      delta_request_gap = -delta_request_gap;
                      delta_arrival_gap = -delta_arrival_gap;
                    }
                  // Limit the ratio between the arrival and request gaps to
                  // the range -1.0 to 2.0 -- in the absence of timing jitter,
                  // we expect the range to actually be 0.0 to 1.0.
                  if (delta_arrival_gap > (2.0*delta_request_gap))
                    delta_arrival_gap = 2.0*delta_request_gap;
                  if (delta_arrival_gap < -delta_request_gap)
                    delta_arrival_gap = -delta_request_gap;
                  double update_factor =
                    pow(KDC_FLOW_REGULATOR_F, delta_arrival_gap -
                        KDC_FLOW_REGULATOR_MU * delta_request_gap);
                  current_Lmax_value *= update_factor;
                }
              this->last_request_gap = request_gap;
              this->last_arrival_gap = arrival_gap;
              this->last_initial_chunk_bytes = chunk_length;
            }
        }
      this->last_request_time = request_issue_time;
    }
  
  // Perform bounds checks on Lmax
  kdu_long lower_bound = KDC_FLOW_REGULATOR_MIN_USECS;
  kdu_long upper_bound = KDC_FLOW_REGULATOR_MAX_USECS;
  if (request_rtt > lower_bound)
    lower_bound = request_rtt;
  request_rtt += (request_rtt >> 1);
  if (request_rtt > upper_bound)
    upper_bound = request_rtt;
  
  if (current_Lmax_value < (estimated_rate * (double) lower_bound))
    current_Lmax_value = (estimated_rate * (double) lower_bound);
  else if (current_Lmax_value > (estimated_rate * (double) upper_bound))
    current_Lmax_value = (estimated_rate * (double) upper_bound);
}


/* ========================================================================= */
/*                                 kdc_request                               */
/* ========================================================================= */

/*****************************************************************************/
/*                    kdc_request::set_response_terminated                   */
/*****************************************************************************/

void kdc_request::set_response_terminated(kdu_long current_time)
{
  response_terminated = true;
  if ((target_end_time > 0) && (queue != NULL) && (queue->cid != NULL))
    queue->cid->reconcile_timed_request(this,current_time);
}

/*****************************************************************************/
/*                        kdc_request::add_dependency                        */
/*****************************************************************************/

void kdc_request::add_dependency(const kdc_request *dep)
{
  kdc_request_dependency *rdp;
  for (rdp=dependencies; rdp != NULL; rdp=rdp->next)
    if (rdp->queue == dep->queue)
      { // Replace existing dependency
        rdp->qid = dep->qid;
        return;
      }
  rdp = queue->client->alloc_dependency();
  rdp->next = dependencies;
  dependencies = rdp;
  rdp->queue = dep->queue;
  rdp->qid = dep->qid;
}

/*****************************************************************************/
/*                        kdc_request::remove_dependency                     */
/*****************************************************************************/

void kdc_request::remove_dependency(const kdc_request *dep,
                                    const kdc_request *alt_dep)
{ 
  kdc_request_dependency *rdp, *prev=NULL;
  for (rdp=dependencies; rdp != NULL; prev=rdp, rdp=rdp->next)
    if ((rdp->queue == dep->queue) && (rdp->qid == dep->qid))
      { // Found a match
        if (alt_dep != NULL)
          { rdp->queue = alt_dep->queue;  rdp->qid = alt_dep->qid; }
        else
          { 
            if (prev == NULL)
              dependencies = rdp->next;
            else
              prev->next = rdp->next;
            rdp->next = NULL; // So we recycle just one record
            queue->client->recycle_dependencies(rdp);
          }
        break;
      }
}


/* ========================================================================= */
/*                                 kdc_primary                               */
/* ========================================================================= */

/*****************************************************************************/
/*                     kdc_primary::set_last_active_request                  */
/*****************************************************************************/

void kdc_primary::set_last_active_request(kdc_request *req)
{
  req->primary_next_request = NULL;
  if (last_active_request == NULL)
    first_active_request = last_active_request = req;
  else
    last_active_request = last_active_request->primary_next_request = req;
  req->is_primary_active_request = true;
}

/*****************************************************************************/
/*                     kdc_primary::remove_active_request                    */
/*****************************************************************************/

void kdc_primary::remove_active_request(kdc_request *req)
{
  assert(req->is_primary_active_request);
  kdc_request *scan, *prev=NULL;
  for (scan=first_active_request; scan != NULL;
       prev=scan, scan=scan->primary_next_request)
    if (scan == req)
      { 
        if (prev == NULL)
          first_active_request = req->primary_next_request;
        else
          prev->primary_next_request = req->primary_next_request;
        if (req == last_active_request)
          { 
            last_active_request = prev;
            assert(req->primary_next_request == NULL);
          }
        else
          assert(req->primary_next_request != NULL);
        req->primary_next_request = NULL;
        req->is_primary_active_request = false;
        break;
      }
  assert(scan != NULL);
  kdc_cid *cid = req->queue->cid;
  if (cid->is_released)
    return; // No special processing required in this case
  
  // See if we need to close the TCP channel
  if ((!is_persistent) && (first_active_request == NULL))
    {
      if (channel != NULL)
        channel->close();
    }
  
  // Take care of primary connection details/assignment for a newly created CID
  if (cid->newly_assigned_by_server)
    cid->assign_ongoing_primary_channel();
}

/*****************************************************************************/
/*                         kdc_primary::service_channel                      */
/*****************************************************************************/

void kdc_primary::service_channel(kdcs_channel_monitor *monitor,
                                  kdcs_channel *channel, int cond_flags)
{
  if (is_released)
    return; // Nothing to serve; channel has already been released.
  kdu_long current_time;
  client->acquire_management_lock(current_time);
  try {
    if (cond_flags & KDCS_CONDITION_READ)
      {
        waiting_to_read = false; // The condition has been cancelled by the
          // monitor before calling this function.  To reinstate it, we
          // need to attempt a read in one of the functions below and
          // have it fail to get all the way through.
        while (read_reply(current_time) || read_body_chunk(current_time));
      }
    if ((active_requester != NULL) && (send_block.get_remaining_bytes() > 0))
      {
        if ((cond_flags & KDCS_CONDITION_ERROR) && !channel_connected)
          {
            KDU_ERROR(e,0x24030901); e <<
            KDU_TXT("Primary channel connection failed!");
          }
        else if (channel_timeout_set && (cond_flags & KDCS_CONDITION_WAKEUP))
          {
            channel_timeout_set = false;
            KDU_ERROR(e,0x19030901); e <<
            KDU_TXT("Primary channel connection attempt timed out!");
          }
        else if (((cond_flags&KDCS_CONDITION_CONNECT) && !channel_connected) ||
                 ((cond_flags & KDCS_CONDITION_WRITE) && channel_connected))
          send_active_request(current_time);
      }
  }
  catch (...)
  {
    client->acquire_management_lock(current_time);
       // In case exception when unlocked
    if ((next == NULL) && (client->primary_channels == this))
      client->final_status = "Connection closed unexpectedly.";
    signal_status("Connection closed unexpectedly.");
    client->release_primary_channel(this);
  }
  client->release_management_lock();
}

/*****************************************************************************/
/*                        kdc_primary::resolve_address                       */
/*****************************************************************************/

void kdc_primary::resolve_address(kdu_long &current_time)
{
  assert(channel == NULL);
  assert(client->management_lock_acquired);
  if (!immediate_address.is_valid())
    { 
      signal_status("Resolving host name ...");
      client->release_management_lock();
      resolve_server_address(immediate_server,immediate_address);
      client->acquire_management_lock(current_time);
      assert(immediate_address.is_valid());
      signal_status("Host name resolved.");
    }
  immediate_address.set_port(immediate_port);
    
  // Now run a check to see if there is another unused TCP channel
  // with the same address and `keep_alive'=true.
  kdc_primary *scan;
  for (scan=client->primary_channels; scan != NULL; scan=scan->next)
    if (scan->keep_alive && (scan != this) &&
        ((scan->num_http_aux_cids+scan->num_http_only_cids) == 0))        
      {
        assert(scan->channel != NULL);
        if (scan->immediate_address == this->immediate_address)
          {
            this->channel = scan->channel;
            this->channel_connected = scan->channel_connected;
            this->channel_reconnect_allowed = this->channel_connected;
            scan->channel = NULL;
            scan->channel_connected = false;
            this->channel->set_channel_servicer(this);
          }
        client->release_primary_channel(scan);
        break;
      }
  if (channel == NULL)
    {
      channel = new kdcs_tcp_channel(client->monitor,true);
      channel_connected = channel_reconnect_allowed = false;
    }
}

/*****************************************************************************/
/*                       kdc_primary::send_active_request                    */
/*****************************************************************************/

void kdc_primary::send_active_request(kdu_long &current_time)
{
  if ((active_requester == NULL) || !send_block.get_remaining_bytes())
    return;
  if (channel == NULL)
    resolve_address(current_time);
  kdc_request *req = NULL;
  bool delivered = false;
  while (!delivered)
    {
      assert(channel != NULL);
      try {
        if (!channel_connected)
          {
            channel_reconnect_allowed = false;
            signal_status("Forming primary connection...");
            channel_connected = channel->connect(immediate_address,this);
            if (!channel->is_active())
              { KDU_ERROR(e,12); e <<
                KDU_TXT("Unable to complete primary request channel "
                        "connection.");
              }
            if (!channel_connected)
              { // Need to wait for connection to complete or a timeout
                if (!channel_timeout_set)
                  {
                    kdu_long timeout_usecs =
                      client->primary_connection_timeout_usecs +
                      client->timer->get_ellapsed_microseconds();
                    channel->schedule_wakeup(timeout_usecs,
                                             timeout_usecs+10000);
                    channel_timeout_set = true;
                  }
                return;
              }
            channel->schedule_wakeup(-1,-1); // Cancel wakeup
            channel_timeout_set = false;
            signal_status("Connected.");
          }
        if (active_requester->last_start_time_usecs < 0)
          {
            active_requester->last_start_time_usecs = current_time;
            if (active_requester->queue_start_time_usecs < 0)
              active_requester->queue_start_time_usecs = current_time;
            if (client->last_start_time_usecs < 0)
              client->last_start_time_usecs = current_time;
            if (client->client_start_time_usecs < 0)
              client->client_start_time_usecs = current_time;
          }
        if (!channel->write_block(send_block))
          return; // Need to wait for the request transmission to complete
        req = active_requester->first_unreplied;
        if (req != NULL)
          { // Can't imagine why the request would not still exist, but just
            // in case.
            while (req->next != active_requester->first_unrequested)
              req = req->next; // Advance to last unreplied request.  This must
                               // be the one whose request we have just issued.
            assert(req->last_event_time < 0);
            req->last_event_time = req->request_issue_time = current_time;
            active_requester->cid->num_active_requests++;
            active_requester->cid->last_request_time = current_time;
          }
        delivered = true;
      }
      catch (kdu_exception exc) { // Something wrong; close channel, maybe retry
        client->acquire_management_lock(current_time);
               // In case exception occurred when unlocked
        channel->close();
        channel_connected = false;
        if (!channel_reconnect_allowed)
          throw exc; // Caller will release the channel
      }
      catch (...) {
        client->acquire_management_lock(current_time);
        channel->close();
        channel_connected = false;
        throw;
      }
    }
  
  // If we get here, we have succeeded, at least in sending the request
  active_requester->cid->last_request_had_byte_limit = (req->byte_limit > 0);
  
  if (client->non_interactive)
    active_requester->signal_status("Non-interactive request in progress...");
  else if (active_requester->close_when_idle)
    active_requester->signal_status("Issuing channel-close request...");
  else
    active_requester->signal_status("Interactive transfer...");
  assert(delivered);
  send_block.restart();
  if (client->is_stateless || !is_persistent)
    req->unblock_primary_upon_comms_complete = true;
  else if (active_requester->just_started)
    req->unblock_primary_upon_reply = true;
  else
    active_requester = NULL; // Unblock primary channel immediately
  if (!waiting_to_read)
    { // We need to attempt to receive the reply (almost certainly not ready
      // yet) so as to inform the channel monitor that we are interested in
      // receiving data.
      assert(!in_http_body);
      read_reply(current_time);
      while (read_body_chunk(current_time));
        // The above code is necessary to cover the extremely unlikely case
        // that the reply and any response data might be available on the
        // channel immediately after we pushed in the request.  This could
        // just possibly happen if the process/thread were suspended for a
        // considerable period of time right after the request was delivered.
    }
}

/*****************************************************************************/
/*                           kdc_primary::read_reply                         */
/*****************************************************************************/

bool kdc_primary::read_reply(kdu_long &current_time)
{
  // First see if a reply is expected.
  if (in_http_body || (first_active_request == NULL))
    return false;
  kdc_request_queue *queue = first_active_request->queue;
  
  // Now read a non-empty reply paragraph if possible
  const char *reply = "";
  kdc_request *req = NULL;
  while (req == NULL)
    {
      if ((reply = channel->read_paragraph()) != NULL)
        { 
          int par_len = (int) strlen(reply);
          queue->received_bytes += par_len;
          client->total_received_bytes += par_len;
          req = queue->process_reply(reply,current_time);
        }
      else
        {
          waiting_to_read = true;
          return false;
        }
    }
  assert(req == first_active_request);
  assert(req->reply_received);

  // Parse response data headers
  kdc_cid *cid = queue->cid;
  if (!cid->uses_aux_channel)
    {
      assert(chunk_length == 0);
      const char *header;
      if ((header = kdcs_caseless_search(reply,"\nContent-type:")) != NULL)
        {
          bool have_jpp_stream = false;
          while (*header == ' ') header++;
          if (kdcs_has_caseless_prefix(header,"image/jpp-stream"))
            {
              header += strlen("image/jpp-stream");
              if ((*header == ' ') || (*header == '\n') ||
                  (*header == ';')) // Ignore any parameter values
                have_jpp_stream = true;
            }
          if (!have_jpp_stream)
            { KDU_ERROR(e,36); e <<
              KDU_TXT("Server response has an unacceptable "
                      "content type.  Complete server response is:\n\n")
              << reply;
            }
        }
      if ((header = kdcs_caseless_search(reply,"\nContent-length:")) != NULL)
        {
          while (*header == ' ') header++;
          if ((!sscanf(header,"%d",&chunk_length)) || (chunk_length<0))
            { KDU_ERROR(e,37); e <<
              KDU_TXT("Malformed \"Content-length\" header "
                      "in HTTP response message.  Complete server response "
                      "is:\n\n") << reply;
            }
          chunked_transfer = false;
          in_http_body = (chunk_length > 0);
        }
      else if ((header = kdcs_caseless_search(reply,
                                              "\nTransfer-encoding:")) != NULL)
        {
          while (*header == ' ') header++;
          if (kdcs_has_caseless_prefix(header,"chunked"))
            chunked_transfer = in_http_body = true;
          else
            { KDU_ERROR(e,0x12030901); e <<
              KDU_TXT("Cannot understand \"Transfer-encoding\" header in "
                      "HTTP response message.  Expect chunked transfer "
                      "encoding, or a \"Content-length\" header.  "
                      "Complete server response is:\n\n") << reply;
            }
        }
      if (in_http_body)
        {
          total_chunk_bytes = 0;
          recv_block.restart();
        }
    }
   
  // Finally, adjust links, pointers and other state variable in accordance
  // with the fact that the reply has been read.
  this->channel_reconnect_allowed = true; // One more reconnect attempt allowed
  if (req->unblock_primary_upon_reply)
    { // Release primary channel for other requests to be issued
      assert(queue == active_requester);
      active_requester = NULL;
      req->unblock_primary_upon_reply = false;
    }
  if (!cid->uses_aux_channel)
    { // HTTP-only communications
      if (!in_http_body)
        { // All communication for this request is now complete
          req->set_response_terminated(current_time);
          assert(req->communication_complete());
          queue->request_comms_completed(req); // This call indirectly invokes
                                               // `remove_active_request'
          req = NULL; // So we don't accidentally reference it
        }
    }
  else
    { 
      assert(!in_http_body);
      remove_active_request(req);
      if (req->communication_complete())
        queue->request_comms_completed(req); // Unlikely, but possible for all
                // response data to have arrived before the reply is received.
    }
   
  return true;
}

/*****************************************************************************/
/*                        kdc_primary::read_body_chunk                       */
/*****************************************************************************/

bool kdc_primary::read_body_chunk(kdu_long &current_time)
{
  if (!in_http_body)
    return false;

  // Start by tentatively finding the queue and request to which
  // the response data that we may be waiting for belongs.  It is possible
  // that these will be NULL, or inappropriate, in the event that
  // `waiting_for_chunk_terminator_after_eor' is true.  That flag is set if
  // an earlier data chunk already contained the EOR message so that
  // communications for the request were noted as complete (possibly leaving
  // the `primary' channel without any active CID).  We need to do this
  // because dependencies processing (needed if there are other channels that
  // use unreliable transports) may result in a request which is marked as
  // `response_terminated' being marked as complete at any time, possibly
  // from a different context.  For this reason, we cannot afford to wait
  // until the chunk terminator text arrives from the server if the message
  // is already marked as `response_terminated'.  In view of the above, the
  // queue and request we find below will not actually be touched if the
  // `waiting_for_chunk_terminator_after_eor' flag is set.
  
  kdc_request *req = first_active_request;
  kdc_request_queue *queue = (req==NULL)?NULL:req->queue;
  assert(waiting_for_chunk_terminator_after_eor ||
         ((req != NULL) && req->reply_received));
  
  if (chunk_length == 0)
    {
      assert(chunked_transfer);
      const char *text = "";
      while ((*text == '\0') || (*text == '\n'))
        {
          if ((text = channel->read_line(false)) == NULL)
            { // Channel not ready for reading
              waiting_to_read = true;
              if (!waiting_for_chunk_terminator_after_eor)
                queue->cid->alert_app_if_new_data();
              return false;
            }
          int header_len = (int) strlen(text);
          if (!waiting_for_chunk_terminator_after_eor)
            queue->received_bytes += header_len;
          client->total_received_bytes += header_len;
        }
      if ((sscanf(text,"%x",&chunk_length) == 0) || (chunk_length < 0))
        { KDU_ERROR(e,38);  e <<
          KDU_TXT("Expected non-negative hex-encoded chunk length on "
                  "line:\n\n") << text;
        }
    }
  
  if (waiting_for_chunk_terminator_after_eor)
    { 
      if (chunk_length != 0)
        { KDU_ERROR(e,0x20070901); e <<
          KDU_TXT("Server response contains an HTTP body with a "
                  "non-terminal EOR message!  EOR messages may appear only "
                  "at the end of a response to any given request.");
        }
      in_http_body = waiting_for_chunk_terminator_after_eor = false;
      return true;
    }

  // If we get here, we must have valid `req' and `queue' pointers.
  assert((req != NULL) && (queue != NULL));
  kdc_cid *cid = queue->cid;
  if (chunk_length == 0)
    { 
      in_http_body = false;
    }
  else
    { // There is chunk data to read
      if (!channel->read_block(chunk_length,recv_block))
        {
          waiting_to_read = true;
          cid->alert_app_if_new_data();
          return false;
        }
      bool is_first_chunk = false;
      kdu_long chunk_start_time = req->last_event_time;
      if (!req->chunk_received)
        { 
          is_first_chunk = true;
          req->chunk_received = true;
          assert(req->reply_received);
          if (chunk_start_time >= current_time)
            chunk_start_time = current_time-1;
          queue->received_first_request_chunk(req,chunk_start_time,
                                              current_time);
        }
      assert(req->last_event_time >= 0);
      req->received_service_time += current_time - chunk_start_time;
      if (req->received_service_time <= 0)
        req->received_service_time = 1; // Must be +ve after chunk received
      total_chunk_bytes += chunk_length;
      queue->received_bytes += chunk_length;
      client->total_received_bytes += chunk_length;
      cid->process_return_data(recv_block,req,current_time);
      
      cid->flow_regulator.chunk_received(chunk_length,req->request_issue_time,
                                         current_time,is_first_chunk,
                                         cid->check_for_more_requests(req),
                                         (req->byte_limit > 0),
                                         cid->request_rtt);
  
      chunk_length = 0;
      if (!chunked_transfer)
        in_http_body = false;
    }
  req->last_event_time = current_time;
  if (!in_http_body)
    req->set_response_terminated(current_time); // Should have been done when
      // EOR message was read; just a precaution for non-compliant servers.
  
  if (!req->communication_complete())
    return true; // More response data still to come
  
  // Perform actions required upon completion of the request's communications
  cid->alert_app_if_new_data();
  if (in_http_body)
    { 
      waiting_for_chunk_terminator_after_eor = true;
      queue->received_bytes += 1; // Expected length of the chunk terminator
                  // line, "0", that we will not be able to assign to the
                  // `queue' when it is read, because the queue might no
                  // longer appear as the active one for this channel.
    }
  
  queue->request_comms_completed(req);
  if (recv_block.get_remaining_bytes() != 0)
    { KDU_ERROR(e,34); e <<
      KDU_TXT("HTTP response body terminated before sufficient "
              "compressed data was received to correctly parse all server "
              "messages!");
    }
  
  total_chunk_bytes = 0;
  return true;
}

/*****************************************************************************/
/*                         kdc_primary::signal_status                        */
/*****************************************************************************/

void kdc_primary::signal_status(const char *text)
{
  kdc_request_queue *queue;
  for (queue=client->request_queues; queue != NULL; queue=queue->next)
    if (queue->cid->primary_channel == this)
      queue->status_string = text;
  client->signal_status();
}


/* ========================================================================= */
/*                                  kdc_cid                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                      kdc_cid::set_last_active_receiver                    */
/*****************************************************************************/

void kdc_cid::set_last_active_receiver(kdc_request *req)
{
  if (last_active_receiver == NULL)
    first_active_receiver = last_active_receiver = req;
  else
    last_active_receiver =
      last_active_receiver->cid_next_receiver = req;
  req->cid_next_receiver = NULL;
  req->is_cid_active_receiver = true;
}

/*****************************************************************************/
/*                       kdc_cid::remove_active_receiver                     */
/*****************************************************************************/

void kdc_cid::remove_active_receiver(kdc_request *req)
{
  assert(req->is_cid_active_receiver);
  kdc_request *scan, *prev=NULL;
  for (scan=first_active_receiver; scan != NULL;
       prev=scan, scan=scan->cid_next_receiver)
    if (scan == req)
      { 
        if (prev == NULL)
          first_active_receiver = req->cid_next_receiver;
        else
          prev->cid_next_receiver = req->cid_next_receiver;
        if (req == last_active_receiver)
          { 
            last_active_receiver = prev;
            assert(req->cid_next_receiver == NULL);
          }
        req->cid_next_receiver = NULL;
        req->is_cid_active_receiver = false;
        break;
      }
  assert(scan != NULL);
}

/*****************************************************************************/
/*                  kdc_cid::calculate_num_outstanding_bytes                 */
/*****************************************************************************/

int kdc_cid::calculate_num_outstanding_bytes()
{
  int delta, result=0;
  kdc_request *scan;
  for (scan=first_active_receiver; scan != NULL; scan=scan->cid_next_receiver)
    if ((scan->byte_limit > 0) &&
        ((delta = (scan->byte_limit - scan->received_message_bytes)) > 0))
      result += delta;
  return result;
}

/*****************************************************************************/
/*                       kdc_cid::find_gaps_to_abandon                       */
/*****************************************************************************/

kdc_chunk_gap *kdc_cid::find_gaps_to_abandon(kdu_long current_time,
                                             bool abandon_all,
                                             kdc_chunk_gap *head)
{
  if (!aux_channel_is_udp)
    return head;
  
  bool abandoned_chunk_free_request = false;
  kdu_long thresh1 = current_time - (kdu_long)(KDC_ABANDON_FACTOR*request_rtt);
  kdu_long thresh2 = thresh1 - (kdu_long)(KDC_ABANDON_FACTOR*request_rtt);
  kdc_chunk_gap *tail=head;
  if (tail != NULL)
    while (tail->next != NULL)
      tail = tail->next;
  
  bool done = false;
  while (!done)
    { 
      kdc_request *req;
      done = true; // Until proven otherwise
      for (req=first_active_receiver; req != NULL; req=req->cid_next_receiver)
        if ((req->chunk_gaps != NULL) &&
            (abandon_all ||
             (req->last_event_time <
              ((req->chunk_received)?thresh1:thresh2))))
          break; // Found a request which needs abandoning
      if (req != NULL)
        { 
          done = false;
          if (!req->chunk_received)
            abandoned_chunk_free_request = true;
              
          // Include all of this request's missing chunks in the list
          if (tail == NULL)
            head = tail = req->chunk_gaps;
          else
            tail = tail->next = req->chunk_gaps;
          req->chunk_gaps = NULL;
          while (tail->next != NULL)
            tail = tail->next;
            
          // Clean up this request
          req->untrusted = true;
          req->set_response_terminated(current_time);
          req->queue->request_comms_completed(req,true);
            // Forces requests dependent on this one to be marked untrusted
        }
    }
  
  if (abandoned_chunk_free_request)
    { 
      request_rtt <<= 1;
      if (request_rtt > KDC_MAX_REQUEST_RTT)
        request_rtt = KDC_MAX_REQUEST_RTT;
    }

  return head;
}

/*****************************************************************************/
/*                       kdc_cid::find_next_requester                        */
/*****************************************************************************/

kdc_request_queue *
  kdc_cid::find_next_requester(kdu_long current_time,
                               bool synthesize_new_request)
{
  if (primary_channel->active_requester != NULL)
    return NULL;
  if (num_active_requests > 1)
    { 
      kdu_long min_gap = (kdu_long)
        ((request_rtt * (KDC_ABANDON_FACTOR+1) * num_active_requests) /
         (KDC_WINDOW_TARGET * KDC_WINDOW_TARGET));
      if ((last_request_time + min_gap) > current_time)
        return NULL;
    }
  if (last_request_had_byte_limit)
    { 
      int num_outstanding_bytes = calculate_num_outstanding_bytes();
      if (num_outstanding_bytes > flow_regulator.get_Omax_request_threshold())
        return NULL;
    }
  
  kdc_request_queue *start = last_requester;
  if ((start == NULL) || ((start = start->next) == NULL))
    start = client->request_queues;

  // Scan request queues using this CID to discover:
  // a) the number of queues
  // b) the number of queues with requests to send;
  // c) the number of queues that are in timed request mode;
  // d) the number of queues that are neither in timed request mode nor
  //    have any requests to send;
  // e) the total amount of excessive lag that must be redistributed to
  //    request queues with requests to send -- this is the total number
  //    of microseconds that need to be added to the t_q values of idle
  //    request queues in timed request mode so that t_q >= tC-RTT;
  // f) the queue, if any, that should be the next requester; and
  // g) an appropriate value to use for the target duration of any request
  //    that may need to be converted to a synthesized timed request.
  int num_queues = 0;
  int num_queues_with_requests = 0;
  int num_queues_in_timed_request_mode = 0;
  int num_regular_empty_queues = 0;
  kdu_long lag_to_compensate = 0;
  kdu_long synth_target_duration = 250000; // Reasonable default value
  kdc_request_queue *best_queue=NULL, *queue, *next_queue;
  kdc_request *req;
  for (queue=start; queue != NULL; queue=(next_queue==start)?NULL:next_queue)
    { 
      next_queue = (queue->next==NULL)?client->request_queues:queue->next;
      if (queue->cid != this)
        continue;
      if ((req = queue->first_unrequested) != NULL)
        { // The queue probably is in a position to be next requester, but
          // we have to check more carefully, possibly returning `req' to NULL
          if (req->preemptive && (!last_request_had_byte_limit) &&
              (primary_channel->last_active_request != NULL) &&
              (primary_channel->last_active_request->queue != queue) &&
              !primary_channel->last_active_request->reply_received)
            { // We are not doing timed or byte limited requests, but we are
              // interleaving requests from multiple queues onto this CID.
              // In this case, sending a request from this queue right now
              // may pre-empt an exising request from another queue that
              // has not yet received its reply.
              return NULL;
            }
          else if ((req->posted_service_time > 0) &&
                   (req->nominal_start_time != queue->next_nominal_start_time))
            { // Note: when a timed request is posted to a queue that is not
              // already in timed request mode, its `next_nominal_start_time'
              // member is set immediately; once the CID itself enters
              // timed request modes (`last_target_end_time' becomes
              // non-negative), all queues using the CID have their
              // `next_nominal_start_time' initialized to the same value.
              // With this in mind, we can always compare a timed request's
              // `nominal_start_time' member with `next_nominal_start_time'.
              assert(queue->next_nominal_start_time >= 0);
              queue->fix_timed_request_discrepancies();
              req = queue->first_unrequested; // Requests may have been removed
            }
        }
      num_queues++;
      if (req != NULL)
        { 
          num_queues_with_requests++;
          if (req->posted_service_time > 0)
            { 
              assert(queue->next_nominal_start_time >= 0);
              assert(req->nominal_start_time==queue->next_nominal_start_time);
              num_queues_in_timed_request_mode++;
              if ((queue->last_noted_target_duration > 0) &&
                  (queue->last_noted_target_duration < synth_target_duration))
                synth_target_duration = queue->last_noted_target_duration;
            }
          if (best_queue == NULL)
            best_queue = queue;
          else if ((queue->next_nominal_start_time >= 0) &&
                   ((best_queue->next_nominal_start_time < 0) ||
                    (best_queue->next_nominal_start_time >
                     queue->next_nominal_start_time)))
            best_queue = queue;
        }
      else
        { 
          if (queue->next_posted_start_time >= 0)
            { // Queue is in timed request mode with no requests to send
              assert(queue->next_nominal_start_time >= 0);
              num_queues_in_timed_request_mode++;
              if ((queue->last_noted_target_duration > 0) &&
                  (queue->last_noted_target_duration < synth_target_duration))
                synth_target_duration = queue->last_noted_target_duration;
            }
          else
            num_regular_empty_queues++;
          if (this->last_target_end_time > 0)
            { // See if this queue has excessive lag
              assert(queue->next_nominal_start_time >= 0);
              kdu_long lag =
                last_target_end_time - queue->next_nominal_start_time;
              lag -= (queue->next_posted_start_time >= 0)?request_rtt:0;
              if (lag > 0)
                lag_to_compensate += lag;
            }
        }
    }
  
  if ((num_queues_in_timed_request_mode == 0) && (last_target_end_time >= 0))
    reset_request_timing();

  if (best_queue != NULL)
    { 
      assert(num_queues_with_requests > 0);
      assert(this->last_idle_time < 0); // A request has been posted since the
        // CID last became idle, so idle time compensation should have been
        // performed within the call to `kdu_client::post_window'.
      
      if (lag_to_compensate > 0)
        { // Perform lag compensation
          assert(this->last_target_end_time > 0);
          int queues_left = num_queues_with_requests;
          kdu_long adjustment_left = lag_to_compensate;
          for (queue=start; queue != NULL;
               queue=(next_queue==start)?NULL:next_queue)
            { 
              next_queue =
                (queue->next==NULL)?client->request_queues:queue->next;
              if (queue->cid != this)
                continue;
              assert(queue->next_nominal_start_time >= 0);
              if ((req = queue->first_unrequested) == NULL)
                { // May be one of the queues losing stored up service time
                  kdu_long lag =
                    last_target_end_time - queue->next_nominal_start_time;
                  lag -= (queue->next_posted_start_time >= 0)?request_rtt:0;
                  if (lag > 0)
                    { // Adjust lag
                      lag_to_compensate -= lag; // For consistency check
                      queue->next_nominal_start_time += lag;
                    }
                }
              else
                { // This is one of the queues that is gaining service time
                  assert((queues_left > 0) && (adjustment_left >= 0));
                  kdu_long  adj = adjustment_left / queues_left;
                  queue->next_nominal_start_time -= adj;
                  queues_left--;
                  adjustment_left -= adj;
                  if (req->posted_service_time > 0)
                    { 
                      queue->fix_timed_request_discrepancies();
                      assert(queue->first_unrequested != NULL);
                    }
                }
            }
          assert(lag_to_compensate == 0);
        }
      
      queue = best_queue;
      if (num_queues_in_timed_request_mode > 0)
        { // If necessary, move the CID into timed request mode; then fill
          // in timing parameters for the request itself.
          req = queue->first_unrequested;
          assert(req != NULL);
          assert(queue->next_nominal_start_time >= 0);
          if (req->posted_service_time > 0)
            { 
              assert(req->nominal_start_time==queue->next_nominal_start_time);
              int divisor = num_queues - num_regular_empty_queues;
                  // This is the N' value discussed in the notes following the
                  // definition of `kdc_request_queue'
              assert(divisor > 0);
              req->target_duration = 1 +
                ((req->posted_service_time-1)/divisor);
            }
          else
            { // Need to synthesize request timing
              assert(synth_target_duration > 0);
              req->nominal_start_time = queue->next_nominal_start_time;
              req->target_duration = synth_target_duration;
            }
          if (last_target_end_time < 0)
            initialize_request_timing(req->nominal_start_time);
        }
      return queue;
    }
    
  // If we get here, there are no queues with any available requests.  However,
  // we may need to synthesize a request so that we can use it to signal
  // abandonment of UDP data chunks which have been deemed overdue
  // by a recent call to `find_gaps_to_abandon'.
  if (synthesize_new_request && (num_queues_in_timed_request_mode == 0))
    { // If any queue is still in timed request mode, there will be more
      // requests, or else the queue will eventually fall out of this mode,
      // allowing a request to be synthesized later on.
      last_idle_time = -1; // In case CID was marked as idle
      for (queue=client->request_queues; queue != NULL; queue=queue->next)
        if ((queue->cid == this) && ((req = queue->request_tail) != NULL))
          { 
            assert(queue->first_unrequested == NULL);
            kdc_request *dup = queue->duplicate_request(req,true);
            if (dup != NULL)
              { 
                dup->preemptive = true;
                assert(queue->first_unrequested == dup);
                return queue;
              }
          }
    }

  // If we get here, no queue has any request to send
  if (first_active_receiver == NULL)
    { // No request to send and the CID is now idle
      flow_regulator.note_idle(); // This call should not normally make a
            // difference, because the flow regulator receives information
            // about whether there are any ensuing requests, each time a
            // chunk of data is received for any given request.  However,
            // this information might have been misleading if all of these
            // later requests turned out to be redundant based on the
            // EOR message and communication completion from earlier requests.
            // This call makes sure that the flow controller is aware of the
            // fact that communication with the server over this CID is
            // currently idle.
      if (this->last_idle_time < 0)
        this->last_idle_time = current_time;
    }

  return NULL;
}

/*****************************************************************************/
/*                       kdc_cid::process_return_data                        */
/*****************************************************************************/

void
  kdc_cid::process_return_data(kdcs_message_block &block, kdc_request *req,
                               kdu_long current_time)
{
  const kdu_byte *data_start = block.peek_block();
  const kdu_byte *data = data_start;
  int data_bytes = block.get_remaining_bytes();
  bool eor_found = false;
  while ((data_bytes > 0) && !eor_found)
    {     
      // Process the next message
      kdu_byte byte = *(data++); data_bytes--;
      int class_id=0, eor_reason_code=-1;
      int range_from=0, range_length=0, aux_val=0;
      kdu_long bin_id = 0;
      kdu_long stream_id = 0;
      bool is_final = ((byte & 0x10) != 0);
      if (byte == 0)
        { // EOR message;
          if (data_bytes == 0)
            return; // Cannot process this message yet
          eor_reason_code = *(data++); data_bytes--;
        }
      else
        { // Extract class-id, bin-id, stream-id and range-offset
          switch ((byte & 0x7F) >> 5) {
            case 0:
            {
              KDU_ERROR(e,41); e <<
              KDU_TXT("Illegal message header encountered "
                      "in response message sent by server.");
            }
            case 1:
              class_id = last_msg_class_id; stream_id = last_msg_stream_id;
              break;
            case 2:
              class_id = -1; stream_id = last_msg_stream_id;
              break;
            case 3:
              class_id = -1; stream_id = -1;
              break;
          }
          bin_id = (kdu_long)(byte & 0x0F);
          while (byte & 0x80)
            {
              if (data_bytes == 0)
                return; // Cannot process this message yet
              byte = *(data++); data_bytes--;
              bin_id = (bin_id << 7) | (kdu_long)(byte & 0x7F);
            }
          
          if (class_id < 0)
            { // Read class code
              class_id = 0;
              do {
                if (data_bytes == 0)
                  return; // Cannot process this message yet
                byte = *(data++); data_bytes--;
                class_id = (class_id << 7) | (int)(byte & 0x7F);
              } while (byte & 0x80);
            }
          
          if (stream_id < 0)
            { // Read codestream ID
              stream_id = 0;
              do {
                if (data_bytes == 0)
                  return; // Cannot process this message yet
                byte = *(data++); data_bytes--;
                stream_id = (stream_id << 7) | (int)(byte & 0x7F);
              } while (byte & 0x80);
            }
          
          do {
            if (data_bytes == 0)
              return; // Cannot process this message yet
            byte = *(data++); data_bytes--;
            range_from = (range_from << 7) | (int)(byte & 0x7F);
          } while (byte & 0x80);
        }
      
      // Read range length
      do {
        if (data_bytes == 0)
          return; // Cannot process this message yet
        byte = *(data++); data_bytes--;
        range_length = (range_length << 7) | (int)(byte & 0x7F);
      } while (byte & 0x80);
      
      if (class_id & 1)
        { // Discard auxiliary VBAS for extended class code
          do {
            if (data_bytes == 0)
              return; // Cannot process this message yet
            byte = *(data++); data_bytes--;
            aux_val = (aux_val << 7) | (int)(byte & 0x7F);
          } while (byte & 0x80);
        }
      
      if ((range_from < 0) || (range_length < 0) ||
          (bin_id < 0) || (stream_id < 0) ||
          (((class_id>>1) == KDU_MAIN_HEADER_DATABIN) && (bin_id != 0)))
        {
          KDU_ERROR(e,42); e <<
          KDU_TXT("Received a JPIP stream message containing an "
                  "illegal header or one which contains a ridiculously large "
                  "parameter.");
        }
      
      if (data_bytes < range_length)
        return; // Cannot process this message yet

      // At this point, we have a complete message.
      if (eor_reason_code >= 0)
        { // End of response message
          eor_found = true;
          data += range_length;
          data_bytes -= range_length; // Skip over any message body for now
          if (req != NULL)
            { 
              if (eor_reason_code == JPIP_EOR_IMAGE_DONE)
                req->image_done = req->window_completed = true;
              else if (eor_reason_code == JPIP_EOR_WINDOW_DONE)
                req->window_completed = true;
              else if (eor_reason_code == JPIP_EOR_BYTE_LIMIT_REACHED)
                req->byte_limit_reached = true;
              else if (eor_reason_code == JPIP_EOR_QUALITY_LIMIT_REACHED)
                req->quality_limit_reached = true;
              else if (eor_reason_code == JPIP_EOR_SESSION_LIMIT_REACHED)
                req->session_limit_reached = true;
              req->set_response_terminated(current_time); // Must call this
                                // last, after the above flags have been set
            }
        }
      else
        { // Regular data-bin class
          last_msg_class_id = class_id;
          last_msg_stream_id = stream_id;
          int cls = class_id >> 1;
          client->add_to_databin(cls,stream_id,bin_id,data,range_from,
                                 range_length,is_final);
          data += range_length;
          data_bytes -= range_length;
          have_new_data_since_last_alert = true;
          if (req != NULL)
            { 
              req->received_body_bytes += range_length;
              req->received_message_bytes += (int)(data-data_start);
            }
        }
      block.read_raw((int)(data-data_start)); // Advance over completed bytes.
      data_start = data;
    }
}

/*****************************************************************************/
/*                  kdc_cid::assign_ongoing_primary_channel                  */
/*****************************************************************************/

void kdc_cid::assign_ongoing_primary_channel()
{
  kdc_primary *primary = this->primary_channel;
  assert((primary != NULL) && this->newly_assigned_by_server &&
         (this->channel_id != NULL));
  newly_assigned_by_server = false;
  
  kdc_primary *new_primary = NULL;
  server_address.set_port(request_port);
  if ((primary->num_http_aux_cids + primary->num_http_only_cids) == 1)
    { // We are the only CID using the current primary communication channel,
      // but we may need to change its address details.
      if (!(primary->using_proxy ||
            (primary->immediate_address == this->server_address)))
        { // Change of address
          assert(server_address.is_valid());
          if (primary->channel != NULL)
            delete primary->channel;
          primary->channel = NULL;
          primary->immediate_address = server_address;
          primary->immediate_port = request_port;
          delete[] primary->immediate_server;
          primary->immediate_server = NULL;
          primary->immediate_server = make_new_string(server);
          primary->channel_connected = false;
          primary->channel_reconnect_allowed = false;
          primary->is_persistent = true; // Until proven otherwise
        }
    }
  else if (uses_aux_channel)
    { // We allow multiple HTTP-TCP/UDP CID's share a single primary channel
      assert(server_address.is_valid());
      if ((primary->num_http_only_cids != 0) ||
          (primary->immediate_address != server_address))
        { // Need to create a new primary channel
          new_primary = client->add_primary_channel(server,request_port,false);
          new_primary->immediate_address = server_address;
        }
    }
  else if (primary->using_proxy)
    { // Every HTTP-only CID gets its own primary channel, even if talking
      // via a proxy.
      new_primary = client->add_primary_channel(primary->immediate_server,
                                                primary->immediate_port,true);
      new_primary->immediate_address = primary->immediate_address;
    }
  else
    { // Direct HTTP-only connection
      new_primary = client->add_primary_channel(server,request_port,false);
      new_primary->immediate_address = server_address;
    }

  if (new_primary != NULL)
    { // Swap over the primary channels
      if (uses_aux_channel)
        {
          assert(primary->num_http_aux_cids > 0);
          primary->num_http_aux_cids--;
          new_primary->num_http_aux_cids++;
        }
      else
        {
          assert(primary->num_http_only_cids > 0);
          primary->num_http_only_cids--;
          new_primary->num_http_only_cids++;
        }
      this->primary_channel = primary = new_primary;
    }
  assert(primary == this->primary_channel);
}

/*****************************************************************************/
/*                    kdc_cid::initialize_request_timing                     */
/*****************************************************************************/

void kdc_cid::initialize_request_timing(kdu_long start_time)
{
  assert(last_target_end_time < 0);
  assert(start_time >= 0);
  last_target_end_time = start_time;
  waiting_to_sync_nominal_request_timing = true;
  target_end_time_disparity = 0; // Just in case
  outstanding_target_duration = 0; // Just in case
  outstanding_disparity_compensation = 0; // Just in case
  kdc_request_queue *queue;
  for (queue=client->request_queues; queue != NULL; queue=queue->next)
    if (queue->cid == this)
      { 
        assert((queue->next_nominal_start_time < 0) ||
               (queue->next_nominal_start_time == start_time));
        queue->next_nominal_start_time = start_time;
      }
}

/*****************************************************************************/
/*                       kdc_cid::reset_request_timing                       */
/*****************************************************************************/

void kdc_cid::reset_request_timing()
{
  if (last_target_end_time < 0)
    return; // Nothing to do
  last_target_end_time = -1;
  target_end_time_disparity = 0;
  outstanding_target_duration = outstanding_disparity_compensation = 0;
  waiting_to_sync_nominal_request_timing = false;
  kdc_request_queue *queue;
  for (queue=client->request_queues; queue != NULL; queue=queue->next)
    if (queue->cid == this)
      { 
        assert(queue->next_posted_start_time < 0);
        queue->next_nominal_start_time = -1;
      }
}

/*****************************************************************************/
/*                      kdc_cid::adjust_request_timing                       */
/*****************************************************************************/

void kdc_cid::adjust_request_timing(kdc_request *req, kdu_long duration)
{
  assert((duration > 0) && (last_target_end_time >= 0));
  last_target_end_time += duration;
  
  // Find the value N' defined in the notes at the end of the definition
  // of `kdc_request_queue'.
  int num_queues = 0; // Total queues associated with this CID
  int num_regular_empty_queues = 0; // Non-timed queues with no requests
  kdu_long tq_sum=0;
  kdc_request_queue *queue;
  for (queue=client->request_queues; queue != NULL; queue=queue->next)
    if (queue->cid == this)
      { 
        num_queues++;
        assert(queue->next_nominal_start_time >= 0);
        if ((queue->first_unrequested == NULL) &&
            (queue->next_posted_start_time < 0))
          { 
            num_regular_empty_queues++;
            queue->next_nominal_start_time += duration;
          }
        tq_sum += queue->next_nominal_start_time;
      }
  kdu_long N_prime = num_queues - num_regular_empty_queues;
  assert(N_prime > 0); // N' must include at least the main queue
  kdc_request_queue *main_queue = req->queue;
  kdu_long main_increment = duration * N_prime;
  main_queue->next_nominal_start_time += main_increment;
  req->target_duration = duration;
  assert(req->next_copy == NULL);
  if (req->posted_service_time <= 0)
    { // Always duplicate the request for non-timed queues, so that further
      // requests will be sent to the server until everything is complete.
      assert(main_queue->next_posted_start_time < 0);
      main_queue->duplicate_request(req);
    }
  else if (req->posted_service_time > (main_increment+num_queues))
    { // Duplicate the request, adding a new one to accommodate the
      // unused service time; this only happens if the original
      // target duration was too long to accommodate within a single
      // request.
      kdc_request *nrq = main_queue->duplicate_request(req);
      nrq->posted_service_time = req->posted_service_time-main_increment;
      nrq->nominal_start_time = req->nominal_start_time+main_increment;
      req->posted_service_time = main_increment;
    }
  tq_sum += main_increment;
  assert(tq_sum == num_queues*last_target_end_time);
    // Validate the fundamental equation
}

/*****************************************************************************/
/*                kdc_cid::adjust_timing_after_queue_removed                 */
/*****************************************************************************/

void kdc_cid::adjust_timing_after_queue_removed()
{
  if ((last_target_end_time < 0) || (num_request_queues < 1))
    return;
  kdu_long disparity_comp = 0;
  kdc_request *req;
  for (req=first_active_receiver; req != NULL; req=req->cid_next_receiver)
    if (req->target_end_time >= 0)
      disparity_comp += req->disparity_compensation;
  this->outstanding_disparity_compensation = disparity_comp;
  
  int num_queues=0;
  kdu_long cum_nominal_start = 0;
  kdc_request_queue *queue;
  for (queue=client->request_queues; queue != NULL; queue=queue->next)
    if (queue->cid == this)
      { 
        assert(queue->next_nominal_start_time >= 0);
        cum_nominal_start += queue->next_nominal_start_time;
        num_queues++;
      }
  assert(num_queues == this->num_request_queues);
  kdu_long delta_t = num_queues * last_target_end_time - cum_nominal_start;
  for (queue=client->request_queues; queue != NULL; queue=queue->next)
    if (queue->cid == this)
      { 
        assert(num_queues > 0);
        kdu_long incr = delta_t / num_queues;
        delta_t -= incr;
        num_queues--;
        queue->next_nominal_start_time += incr;
      }
}

/*****************************************************************************/
/*                  kdc_cid::sync_nominal_request_timing                     */
/*****************************************************************************/

void kdc_cid::sync_nominal_request_timing(kdu_long delta_usecs)
{
  assert(waiting_to_sync_nominal_request_timing);
  waiting_to_sync_nominal_request_timing = false;
  last_target_end_time += delta_usecs;
  kdc_request *req;
  for (req=first_active_receiver; req != NULL; req=req->cid_next_receiver)
    if (req->nominal_start_time >= 0)
      { 
        req->nominal_start_time += delta_usecs;
        req->target_end_time += delta_usecs;
      }
  kdc_request_queue *queue;
  for (queue=client->request_queues; queue != NULL; queue=queue->next)
    if (queue->cid == this)
      { 
        assert(queue->next_nominal_start_time >= 0);
        queue->next_nominal_start_time += delta_usecs;
        if (queue->next_posted_start_time >= 0)
          queue->next_posted_start_time += delta_usecs;
        for (req=queue->first_unrequested; req != NULL; req=req->next)
          { 
            assert(req->target_end_time < 0);
            if (req->nominal_start_time < 0)
              break;
            req->nominal_start_time += delta_usecs;
          }
      }
}

/*****************************************************************************/
/*                          kdc_cid::wake_from_idle                          */
/*****************************************************************************/

void kdc_cid::wake_from_idle(kdu_long current_time)
{
  if (last_idle_time < 0)
    return;
  if (current_time < 0)
    current_time = client->timer->get_ellapsed_microseconds();
  kdu_long lost_service_time = (current_time - last_idle_time) + request_rtt;
  assert(lost_service_time >= 0);
  last_idle_time = -1;
  if (last_target_end_time < 0)
    return;
  last_target_end_time += lost_service_time;
  kdc_request_queue *queue;
  for (queue=client->request_queues; queue != NULL; queue=queue->next)
    if (queue->cid == this)
      { 
        assert(queue->next_nominal_start_time >= 0);
        queue->next_nominal_start_time += lost_service_time;
      }
}

/*****************************************************************************/
/*                     kdc_cid::reconcile_timed_request                      */
/*****************************************************************************/

void
  kdc_cid::reconcile_timed_request(kdc_request *req, kdu_long actual_end_time)
{
  if (waiting_to_sync_nominal_request_timing ||
      (last_target_end_time < 0) || (req->target_end_time <= 0))
    return;
  assert(req->byte_limit > 0);
  if (req->window_completed || req->quality_limit_reached)
    { // Compensate for the fact that the request may have been completed
      // before reaching the byte limit.
      int unused_bytes = req->byte_limit - req->received_message_bytes;
      kdu_long unused_usecs = 0;
      if (unused_bytes > 0)
        unused_usecs = flow_regulator.estimate_usecs_for_bytes(unused_bytes);
      if (unused_usecs > 0)
        { 
          if (unused_usecs > last_target_end_time)
            unused_usecs = last_target_end_time; // So it stays >= 0
          for (kdc_request *scn=req; scn != NULL; scn=scn->cid_next_receiver)
            if (scn->target_end_time >= 0)
              scn->target_end_time -= unused_usecs;
          last_target_end_time -= unused_usecs;
          kdc_request_queue *queue;
          for (queue=client->request_queues; queue != NULL; queue=queue->next)
            if (queue->cid == this)
              { 
                assert(queue->next_nominal_start_time >= 0);
                queue->next_nominal_start_time -= unused_usecs;
              }
        }
    }
  
  target_end_time_disparity = actual_end_time - req->target_end_time;
  outstanding_target_duration = last_target_end_time - req->target_end_time;
  outstanding_disparity_compensation -= req->disparity_compensation;
  req->disparity_compensation = 0; // So we don't count this again
  req->target_end_time = -1;
}

/*****************************************************************************/
/*                          kdc_cid::service_channel                         */
/*****************************************************************************/

void kdc_cid::service_channel(kdcs_channel_monitor *monitor,
                              kdcs_channel *channel, int cond_flags)
{
  if (is_released || !uses_aux_channel)
    return; // Nothing to serve; channel has already been released.

  kdu_long current_time;
  client->acquire_management_lock(current_time);
  try {
    if (!aux_channel_connected)
      {
        if (cond_flags & KDCS_CONDITION_ERROR)
          {
            KDU_ERROR(e,0x24030902); e <<
            KDU_TXT("Auxiliary return channel connection attempt failed!");
          }
        else
          connect_aux_channel(current_time);
      }
    while (aux_channel_connected && read_aux_chunk(current_time));
    this->alert_app_if_new_data();
  }
  catch (...) {
    client->acquire_management_lock(current_time);
        // In case exception occurred when unlocked
    const char *explanation = "Connection closed unexpectedly.";
    if (first_active_receiver == NULL)
      explanation = "Server closed idle connection.";
    if ((next == NULL) && (client->cids == this) && !channel_close_requested)
      client->final_status = explanation;
    signal_status(explanation);
    client->release_cid(this);
  }
  client->release_management_lock();
}

/*****************************************************************************/
/*                        kdc_cid::connect_aux_channel                       */
/*****************************************************************************/

bool kdc_cid::connect_aux_channel(kdu_long &current_time)
{
  if (aux_channel_connected)
    return true;
  server_address.set_port(return_port);
  signal_status("Forming auxiliary connection...");
  if (aux_tcp_channel != NULL)
    { 
      if (aux_tcp_channel->connect(server_address,this))
        aux_channel_connected = true;
      if (!aux_tcp_channel->is_active())
        { KDU_ERROR(e,13); e <<
          KDU_TXT("Unable to connect auxiliary TCP channel to server.");
        }
      if (aux_channel_connected)
        { // Connection succeeded
          aux_tcp_channel->schedule_wakeup(-1,-1); // Cancel wakeup
          
          // Temporarily borrow `aux_recv_block' to send connection header
          aux_recv_block.restart();
          aux_recv_block << channel_id << "\r\n\r\n";
          aux_tcp_channel->write_block(aux_recv_block);
          
          if (aux_min_usecs_per_byte > 0.0)
            aux_recv_gate = current_time;
        }
      else
        { // Need to wait for connection to complete or a timeout
          if (aux_connect_deadline == 0)
            { 
              kdu_long timeout = client->aux_connection_timeout_usecs;
              aux_connect_deadline = current_time + timeout;
              aux_tcp_channel->schedule_wakeup(aux_connect_deadline,
                                               aux_connect_deadline+100000);
            }
        }
    }
  else if (aux_udp_channel != NULL)
    { 
      bool failed = !aux_udp_channel->connect(server_address,this);
      if (!failed)
        { 
          // See if the connection is complete by now
          int msg_len = 0;
          aux_udp_channel->recv_msg(msg_len,-8); // Just does MSG_PEEK
          if (msg_len > 0)
            aux_channel_connected = true;
          else
            { // Temporarily borrow `aux_recv_block' to send the connection msg
              aux_recv_block.restart();
              size_t string_len = strlen(channel_id);
              kdu_byte hdr[4] =
                { 0xFF,0xFF,(kdu_byte)(string_len>>8),(kdu_byte)string_len };
              aux_recv_block.write_raw(hdr,4);
              aux_recv_block << channel_id;
              msg_len = aux_recv_block.get_remaining_bytes();
              kdcs_sockaddr local_addr, peer_addr;
              aux_udp_channel->send_msg(aux_recv_block.peek_block(),msg_len);
            }
        }
      if (failed || !aux_udp_channel->is_active())
        { KDU_ERROR(e,0x04081001); e <<
          KDU_TXT("Unable to connect auxiliary UDP channel to server.");
        }
      if (aux_channel_connected)
        { 
          aux_udp_channel->schedule_wakeup(-1,-1); // Cancel any wakeup
          if (aux_min_usecs_per_byte > 0.0)
            aux_recv_gate = current_time;
        }
      else
        { // Need to wait for the server to send something before we know we
          // are connected.  Set a hard deadline if not already set, but also
          // schedule a (typically earlier) return to this function for the
          // purpose of retransmitting the connection establishment message
          if (aux_connect_deadline == 0)
            aux_connect_deadline = current_time+5000000; // Give it 5 seconds
          kdu_long retry_time = current_time+200000; // Retry every 200ms
          aux_udp_channel->schedule_wakeup(retry_time,retry_time+100000);
        }
    }
  else
    assert(0);

  if (!aux_channel_connected)
    { 
      if (current_time >= aux_connect_deadline)
        { KDU_ERROR(e,0x19030902); e <<
          KDU_TXT("Auxiliary return channel connection attempt timed out!");
        }
      return false;
    }
  
  aux_connect_deadline = 0;
  signal_status("Receiving data ...");
  aux_recv_block.restart();
  have_unsent_ack = false;
  tcp_chunk_length = 0;
  return true;
}

/*****************************************************************************/
/*                           kdc_cid::read_udp_chunk                         */
/*****************************************************************************/

bool kdc_cid::read_udp_chunk(kdu_long &current_time)
{
  if (!aux_channel_connected)
    return false;

  if (current_time < aux_recv_gate)
    { // Receiving rate is being throttled
      aux_udp_channel->schedule_wakeup(aux_recv_gate,aux_recv_gate+5000);
      return false;
    }
  
  if (have_unsent_ack)
    { // Attend to this first
      if (!aux_udp_channel->send_msg(ack_buf,8))
        return false;
      have_unsent_ack = false;
    }
  
  int dgram_length=0;
  kdu_byte *dgram = aux_udp_channel->recv_msg(dgram_length,4096);
  if (dgram == NULL)
    return false;
  if (dgram_length < 8)
    { KDU_ERROR(e,0x29071001); e <<
      KDU_TXT("Illegal datagram length found in server return data "
              "sent on the auxiliary UDP channel.  Datagrams "
              "must include the 8-byte chunk preamble, so they cannot "
              "be smaller than 8 bytes in length.  Got a datagram with "
              "length ") << dgram_length << ".";
    }
  
  if (aux_per_byte_loss_probability > 0.0)
    { // Simulate packet loss
      int rand_thresh = (int)
        (aux_per_byte_loss_probability*dgram_length*RAND_MAX);
      if (rand() < rand_thresh)
        { // Discard this datagram
          if (aux_min_usecs_per_byte > 0.0)
            aux_recv_gate += (kdu_long)(dgram_length * aux_min_usecs_per_byte);
          return true; // So we can get called again
        }
    }
  
  ack_buf[0] = ack_buf[1] = 0; // Aways clear the "control" field for now
  for (int i=2; i < 8; i++)
    ack_buf[i] = dgram[i];
  kdu_uint16 qid16 = dgram[2];  qid16=(qid16<<8)+dgram[3];
  
  have_unsent_ack = true;
  kdc_request *req = first_active_receiver;
  for (; req != NULL; req=req->cid_next_receiver)
    if ((qid16 == (kdu_uint16) req->qid) && !req->response_terminated)
      break;
  bool is_first_chunk = false;
  if (req != NULL)
    { 
      kdu_long chunk_start_time = req->last_event_time;
      if (!req->chunk_received)
        { 
          is_first_chunk = true;
          req->chunk_received = true;
          chunk_start_time = current_time -
            flow_regulator.estimate_usecs_for_bytes(dgram_length);
          if (chunk_start_time < req->request_issue_time)
            chunk_start_time = req->request_issue_time;
          if (chunk_start_time >= current_time)
            chunk_start_time = current_time-1;
          req->queue->received_first_request_chunk(req,chunk_start_time,
                                                   current_time);
          if (req->reply_received)
            { 
              assert(req->request_issue_time >= 0);
              update_request_rtt(current_time - req->request_issue_time);
            }
        }
      req->received_service_time += current_time - chunk_start_time;
    }

  // Note that `req' might be NULL here if all requests have already been
  // fully processed (or abandoned).  Nevertheless, there is no harm in
  // processing the data chunk and absorbing its contributions into the cache.
  total_aux_chunk_bytes += dgram_length;
  client->total_received_bytes += dgram_length;
  if (dgram_length > 8)
    { 
      aux_recv_block.write_raw(dgram+8,dgram_length-8);
      process_return_data(aux_recv_block,req, // `req' is allowed to be NULL
                          current_time); 
      if (aux_recv_block.get_remaining_bytes() != 0)
        { KDU_ERROR(e,0x02081001); e <<
          KDU_TXT("Illegal data chunk received from server over auxiliary "
                  "UDP channel.  UDP data chunks must contain a whole "
                  "number of JPIP messages, all of which must belong to a "
                  "single request.");
        }
    }
  
  if (req != NULL)
    flow_regulator.chunk_received(dgram_length,req->request_issue_time,
                                  current_time,is_first_chunk,
                                  check_for_more_requests(req),
                                  (req->byte_limit > 0),request_rtt);

  if (req != NULL)
    { // Augment queue's byte count and adjust the `chunk_gaps' list
      req->last_event_time = current_time;
      req->queue->received_bytes += dgram_length;
      
      int seq = dgram[5]; seq = (seq<<8)+dgram[6]; seq = (seq<<8)+dgram[7];
      assert(req->chunk_gaps != NULL);
      kdc_chunk_gap *gap, *prev_gap=NULL;
      for (gap=req->chunk_gaps; gap != NULL; prev_gap=gap, gap=gap->next)
        { 
          if (seq < gap->seq_from)
            break; // This is a repeated data chunk
          if (gap->seq_to < 0)
            { // Chunk belongs to final open-ended gap
              assert(gap->next == NULL);
              if (req->response_terminated)
                gap->seq_to = seq; // This chunk must have the EOR message
            }
          else if (seq > gap->seq_to)
            continue;
          
          // If we get here, `seq' belongs to gap
          if (gap->seq_from == gap->seq_to)
            { // Singleton gap can be removed
              assert(gap->seq_from == seq);
              if (prev_gap == NULL)
                req->chunk_gaps = gap->next;
              else
                prev_gap->next = gap->next;
              gap->next = NULL; // So we only recycle `gap'
              client->recycle_chunk_gaps(gap);
            }
          else if (seq == gap->seq_from)
            gap->seq_from++;
          else if (seq == gap->seq_to)
            gap->seq_to--;
          else
            { 
              kdc_chunk_gap *new_gap = client->alloc_chunk_gap();
              new_gap->qid = req->qid;
              new_gap->seq_from = seq+1;
              new_gap->seq_to = gap->seq_to;
              new_gap->next = gap->next;
              gap->seq_to = seq-1;
              gap->next = new_gap;
            }
          break;
        }
    }

  if ((req != NULL) && req->communication_complete())
    { // May be able to retire this request and/or other requests which
      // depend upon this request's data
      req->queue->request_comms_completed(req);
      req = NULL; // Because the request might have been removed altogether
    }

  assert(have_unsent_ack && (dgram_length >= 8));
  if (aux_min_usecs_per_byte > 0.0)
    { 
      aux_recv_gate += (kdu_long)(dgram_length * aux_min_usecs_per_byte);
      if (aux_recv_gate < (current_time-100000))
        aux_recv_gate = current_time-100000; // Don't get more than 0.1s behind
    }

  return true; // Next call to this function will send the acknowledgement
}

/*****************************************************************************/
/*                           kdc_cid::read_tcp_chunk                         */
/*****************************************************************************/

bool kdc_cid::read_tcp_chunk(kdu_long &current_time)
{
  if (!aux_channel_connected)
    return false;

  if (current_time < aux_recv_gate)
    { // Receiving rate is being throttled
      aux_tcp_channel->schedule_wakeup(aux_recv_gate,aux_recv_gate+5000);
      return false;
    }  

  if (have_unsent_ack && (tcp_chunk_length == 0))
    { // Note: the acknowledgement message should not be sent until the
      // `tcp_chunk_length' value has been returned to 0; otherwise, we
      // are still waiting to retrieve all the bytes of the chunk.
      if (!aux_tcp_channel->write_raw(ack_buf,8))
        return false;
      have_unsent_ack = false;
    }
    
  kdu_byte *raw;
  if (tcp_chunk_length == 0)
    { 
      raw = aux_tcp_channel->read_raw(8);
      if (raw == NULL)
        return false;
      tcp_chunk_length = (int) raw[0];
      tcp_chunk_length = (tcp_chunk_length << 8) + (int) raw[1];
      if (tcp_chunk_length < 8)
        { KDU_ERROR(e,39); e <<
          KDU_TXT("Illegal chunk length found in server return data "
                  "sent on the auxiliary TCP channel.  Chunk lengths "
                  "must include the length of the 8-byte chunk preamble, "
                  "which contains the chunk length value itself.  This "
                  "means that the length may not be less than 8.  Got a "
                  "value of ") << tcp_chunk_length << ".";
        }
      for (int i=0; i < 8; i++)
        ack_buf[i] = raw[i];
      have_unsent_ack = true;
      total_aux_chunk_bytes += tcp_chunk_length;
    }
  
  assert((tcp_chunk_length >= 8) && have_unsent_ack);
  if (tcp_chunk_length > 8)
    { 
      raw = aux_tcp_channel->read_raw(tcp_chunk_length-8);
      if (raw == NULL)
        return false;

      aux_recv_block.write_raw(raw,tcp_chunk_length-8);
      int parsed_bytes, unparsed_bytes = aux_recv_block.get_remaining_bytes();
      bool need_to_attribute_chunk_header = true;
      kdc_request *req = NULL;
      while (unparsed_bytes > 0)
        { 
          if (req == NULL)
            { // Find the request to which the next message belongs.
              for (req=first_active_receiver; req != NULL;
                   req=req->cid_next_receiver)
                if (!req->response_terminated)
                  break;
            }
          if (req == NULL)
            { KDU_ERROR(e,0x14030901); e <<
              KDU_TXT("Server's response data seems to be getting ahead of "
                      "receiver's requests!!!  All outstanding response data "
                      "for issued requests on an HTTP-TCP/UDP JPIP channel "
                      "have been received over the auxiliary channel, yet "
                      "there is still more data available!");
            }
          bool is_first_chunk = false;
          kdu_long chunk_start_time = req->last_event_time;
          if (!req->chunk_received)
            { 
              is_first_chunk = true;
              req->chunk_received = true;
              chunk_start_time = current_time -
                flow_regulator.estimate_usecs_for_bytes(tcp_chunk_length);
              if (chunk_start_time < req->request_issue_time)
                chunk_start_time = req->request_issue_time;
              if (chunk_start_time >= current_time)
                chunk_start_time = current_time-1;
              req->queue->received_first_request_chunk(req,chunk_start_time,
                                                       current_time);
              if (req->reply_received)
                { 
                  assert(req->request_issue_time >= 0);
                  update_request_rtt(current_time - req->request_issue_time);
                }
            }
          req->received_service_time += current_time - chunk_start_time;
          req->last_event_time = current_time;
          if (need_to_attribute_chunk_header)
            { // First queue with outstanding response data pays for header
              req->queue->received_bytes += 8;
              need_to_attribute_chunk_header = false;
            }
          process_return_data(aux_recv_block,req,current_time);
          parsed_bytes = unparsed_bytes - aux_recv_block.get_remaining_bytes();
          if (parsed_bytes == 0)
            break; // Need to wait for more chunks to arrive
          unparsed_bytes -= parsed_bytes;
          req->queue->received_bytes += parsed_bytes;
          
          flow_regulator.chunk_received(parsed_bytes,req->request_issue_time,
                                        current_time,is_first_chunk,
                                        check_for_more_requests(req),
                                        (req->byte_limit > 0),request_rtt);
          
          if (req->response_terminated)
            { 
              if (req->communication_complete())
                req->queue->request_comms_completed(req);
              req = NULL;   // Force re-evaluation of the request and queue
            }
        }
    }
  
  if (aux_min_usecs_per_byte > 0.0)
    { 
      aux_recv_gate += (kdu_long)(tcp_chunk_length*aux_min_usecs_per_byte);
      if (aux_recv_gate < (current_time-100000))
        aux_recv_gate = current_time-100000; // Don't get more than 0.1s behind
    }
  
  client->total_received_bytes += tcp_chunk_length;
  tcp_chunk_length = 0;
  return true; // The acknowledgement message will be sent in the next call
}

/*****************************************************************************/
/*                           kdc_cid::signal_status                          */
/*****************************************************************************/

void kdc_cid::signal_status(const char *text)
{
  kdc_request_queue *queue;
  for (queue=client->request_queues; queue != NULL; queue=queue->next)
    if (queue->cid == this)
      queue->status_string = text;
  client->signal_status();
}

/*****************************************************************************/
/*                      kdc_cid::check_for_more_requests                     */
/*****************************************************************************/

bool kdc_cid::check_for_more_requests(const kdc_request *req)
{ 
  if ((last_active_receiver != NULL) && (last_active_receiver != req))
    return true; // Simple and also most common case
  for (kdc_request_queue *qscan=client->request_queues;
       qscan != NULL; qscan=qscan->next)
    if ((qscan->first_unrequested != NULL) && (qscan->cid == this))
      return true;
  return false;
}


/* ========================================================================= */
/*                              kdc_request_queue                            */
/* ========================================================================= */

/*****************************************************************************/
/*                        kdc_request_queue::add_request                     */
/*****************************************************************************/

kdc_request *kdc_request_queue::add_request(kdu_long current_time)
{
  kdc_request *qp = client->alloc_request();
  qp->init(this,client->session_untrusted);
  if (request_tail == NULL)
    request_head = request_tail = qp;
  else
    request_tail = request_tail->next = qp;
  if (first_incomplete == NULL)
    first_incomplete = qp;
  if (first_unreplied == NULL)
    first_unreplied = qp;
  if (first_unrequested == NULL)
    first_unrequested = qp;
  is_idle = false;
  if ((cid != NULL) && (cid->last_idle_time >= 0))
    cid->wake_from_idle(current_time);
  return qp;
}

/*****************************************************************************/
/*                    kdc_request_queue::duplicate_request                   */
/*****************************************************************************/

kdc_request *
  kdc_request_queue::duplicate_request(kdc_request *req, bool force_dup)
{
  if (!force_dup)
    { 
      if (close_when_idle)
        return NULL; // Don't duplicate requests if we are closing
    }
  assert(req->next_copy == NULL);
  if ((req->queue != this) || (request_tail == NULL))
    { 
      assert(0);
      return NULL;
    }
  
  kdc_request *qp = client->alloc_request();
  qp->init(this,client->session_untrusted);
  qp->custom_id = req->custom_id;
  qp->original_window.copy_from(req->original_window);
  qp->window.copy_from(req->window);
  qp->preemptive = req->preemptive;
  qp->new_elements = false;
  qp->is_copy = true;
  qp->next = req->next;
  req->next = qp;
  if (request_tail == req)
    request_tail = qp;
  if (first_incomplete == qp->next)
    first_incomplete = qp;
  if (first_unreplied == qp->next)
    first_unreplied = qp;
  if (first_unrequested == qp->next)
    first_unrequested = qp;
  req->next_copy = qp;
  qp->copy_src = req;
  is_idle = false;
  return qp;
}

/*****************************************************************************/
/*                      kdc_request_queue::remove_request                    */
/*****************************************************************************/

void kdc_request_queue::remove_request(kdc_request *req)
{
  assert(req->queue == this);
  
  // Start by making sure the request is no longer on a primary channel's
  // list of active requesters or a CID's list of active receivers -- should
  // not be the case normally, but perhaps we are backing out of some error
  // condition.
  if (req->is_primary_active_request)
    { 
      assert(cid != NULL);
      kdc_primary *primary = cid->primary_channel;
      assert(primary != NULL);
      primary->remove_active_request(req);
    }
  if (req->is_cid_active_receiver)
    { 
      assert(cid != NULL);
      cid->remove_active_receiver(req);
    }
    
  // Now adjust the queue pointers so that they do not reference this
  // request.
  kdc_request *scan, *prev=NULL;
  for (scan=request_head; scan != NULL; prev=scan, scan=scan->next)
    { 
      if (scan == req)
        { 
          if (prev == NULL)
            request_head = req->next;
          else
            prev->next = req->next;
          if (req == request_tail)
            { 
              request_tail = prev;
              assert((prev == NULL) || (prev->next == NULL));
            }
          if (req == first_unrequested)
            first_unrequested = req->next;
          if (req == first_unreplied)
            first_unreplied = req->next;
          if (req == first_incomplete)
            first_incomplete = req->next;
          break;
        }
    }
  assert(scan != NULL);
  
  // Next unlink the request from any copies of it or from any request that
  // was copied to create this one.
  if (req->next_copy != NULL)
    { 
      assert(req->next_copy->copy_src == req);
      if (req->next_copy->received_service_time < req->received_service_time)
        req->next_copy->received_service_time = req->received_service_time;
      req->next_copy->copy_src = req->copy_src;
    }
  if (req->copy_src != NULL)
    { 
      assert(req->copy_src->next_copy == req);
      if ((req->next_copy == NULL) &&
          (req->copy_src->received_service_time < req->received_service_time))
        req->copy_src->received_service_time = req->received_service_time;
          // Make sure service time can never appear to decrease when queried
          // by `kdu_client::get_window_info'.
      req->copy_src->next_copy = req->next_copy;
    }
  req->copy_src = req->next_copy = NULL;
  
  // Finally, we can recycle the storage.
  client->recycle_request(req);
}

/*****************************************************************************/
/*                kdc_request_queue::request_comms_completed                 */
/*****************************************************************************/

void kdc_request_queue::request_comms_completed(kdc_request *req,
                                                bool force_untrusted)
{
  assert(req->communication_complete());
  assert(req->queue == this);
  if (force_untrusted)
    req->untrusted = true;
  kdc_primary *primary = (cid==NULL)?NULL:cid->primary_channel;
  if (req->is_primary_active_request)
    { // In case this was not done when the reply was received -- this is true
      // for requests issued over HTTP-only channels.
      assert(primary != NULL);
      primary->remove_active_request(req);
    }
  if (req->is_cid_active_receiver)
    { 
      assert(cid != NULL);
      cid->remove_active_receiver(req);
    }
  if (req->unblock_primary_upon_comms_complete && (primary != NULL))
    { 
      assert(primary->active_requester == this);
      primary->active_requester = NULL;
      req->unblock_primary_upon_comms_complete = false;
    }
  if (this->unreliable_transport)
    { // We may be on somebody's dependency list
      kdc_request *rrq, *alt_dependency=NULL;
      for (rrq=first_incomplete; rrq != req; rrq=rrq->next)
        if (!rrq->communication_complete())
          alt_dependency = rrq;
      kdc_request_queue *queue;
      for (queue=client->request_queues; queue != NULL; queue=queue->next)
        { 
          if (queue == this)
            continue; // Leave current queue until last
          rrq=queue->first_incomplete;
          for (; rrq != queue->first_unrequested; rrq=rrq->next)
            { 
              if (force_untrusted &&
                  ((queue->cid != this->cid) || (req->qid < rrq->qid)))
                rrq->untrusted = true;
              rrq->remove_dependency(req,alt_dependency);
            }
          queue->process_completed_requests();
        }
      for (rrq=req->next; rrq != this->first_unrequested; rrq=rrq->next)
        { 
          if (force_untrusted)
            rrq->untrusted = true;
          rrq->remove_dependency(req,alt_dependency);
        }
    }
  this->process_completed_requests();
}

/*****************************************************************************/
/*              kdc_request_queue::process_completed_requests                */
/*****************************************************************************/

void kdc_request_queue::process_completed_requests()
{
  kdc_request *req, *next_req;
  for (req=first_incomplete; req != NULL; req=next_req)
    { 
      next_req = req->next; // Be prepared to remove intermediate requests
      if (!req->is_complete())
        continue;

      if (req->dependencies != NULL)
        { // It is possible that the request still has some dependencies, but
          // these are superfluous because it is already untrusted, or because
          // its EOR code did not provide any meaningful information about the
          // state of the service.  In any case, we will mark `req' as
          // untrusted and remove its `dependencies' list right away here.
          req->untrusted = true;
          client->recycle_dependencies(req->dependencies);
          req->dependencies = NULL;
        }
      if (req->image_done && !req->untrusted)
        client->image_done = true;
      if (req->session_limit_reached)
        client->session_limit_reached = true;
  
      // Eliminate any future requests which are made redundant by this one
      if (req->window_completed && !req->untrusted)
        { 
          kdc_request *rrq, *next_rrq; // Scan potentially redundant requests
          for (rrq=first_unrequested; rrq != NULL; rrq=next_rrq)
            { 
              next_rrq = rrq->next;
              if (close_when_idle && (rrq == request_tail))
                break; // Don't remove the last request (it is used for cclose)
              if ((rrq->copy_src == req) || req->window.contains(rrq->window))
                { 
                  if (rrq == next_req)
                    next_req = next_rrq;
                  remove_request(rrq);
                }
            }
        }
        
      // Adjust the request queue
      if (req == first_incomplete)
        { // Easy case: keep the request and advance `first_incomplete'
          first_incomplete = next_req;
          while (request_head != req)
            { // Remove all earlier completed requests
              assert(request_head != NULL);
              remove_request(request_head);
            }
        }
      else if (next_req != first_unreplied)
        { // We can remove the request right now; there is a later one
          assert(req != first_unrequested);
          remove_request(req);
        }
      
      // Adjust the CID's count of current active requests
      cid->num_active_requests--;
      
      // See if we need to create model managers for code-streams covered by
      // the server's response.  This will be true if the communication is
      // ongoing and stateless.
      if (client->is_stateless && !client->non_interactive)
        { 
          kdu_sampled_range cs_range;
          int cs_range_num, cs_idx;
          for (cs_range_num=0;
               !(cs_range =
                 req->window.codestreams.get_range(cs_range_num)).is_empty();
               cs_range_num++)
            { 
              if (cs_range.from < 0)
                continue; // Cannot provide model info for unknown codestreams
              for (cs_idx=cs_range.from; cs_idx <= cs_range.to;
                   cs_idx+=cs_range.step)
                { 
                  bool main_header_complete;
                  client->get_databin_length(KDU_MAIN_HEADER_DATABIN,cs_idx,0,
                                             &main_header_complete);
                  if (main_header_complete)
                    client->add_model_manager(cs_idx);
                }
            }
        }
    }
  
  // See if the request queue is now idle, taking appropriate action
  if (first_incomplete == NULL)
    { 
      this->set_idle();
      if (close_when_idle)
        {
          client->have_queues_ready_to_close = true;
          if (client->non_interactive)
            signal_status(client->final_status =
                          "Non-interactive service complete.");
          else
            signal_status("Not connected.");
        }
      else if (client->image_done)
        signal_status("Image complete.");
      else
        signal_status("Connection idle.");
    }  
}

/*****************************************************************************/
/*                      kdc_request_queue::issue_request                     */
/*****************************************************************************/

void kdc_request_queue::issue_request(kdu_long &current_time,
                                      kdc_chunk_gap * &gaps_to_abandon)
{
  kdc_request *req = first_unrequested;
  kdc_primary *primary = cid->primary_channel;
  assert((req != NULL) && (primary->active_requester == NULL) &&
         (!cid->newly_assigned_by_server));
  kdcs_message_block &send_block = primary->send_block;
  kdcs_message_block &query_block = primary->query_block;
  send_block.restart();
  query_block.restart();
  
  if (client->obliterating_requests_in_flight > 0)
    req->untrusted = true;
  
  // Special processing for timed and other byte limited requests
  assert(req->byte_limit == 0); // Byte limits should be set nowhere else.
  if (req->target_duration > 0)
    { 
      assert(req->nominal_start_time >= 0);
      kdu_long target_duration = req->target_duration; // We may change this
      int max_len = cid->flow_regulator.get_request_byte_limit(0,true);
          // Note: we allow ourselves to ask for the full Lmax bytes,
          // even if the number of current outstanding requested bytes is
          // non-zero; this is different to the way we handle regular
          // (non-timed) HTTP-only communication.  We do this to encourage
          // more regular request sizes so as to reduce the risk of splitting
          // requests in a weird pattern during constant rate animations.
      kdu_long horizon = // Max target duration to assume for disparity comp.
        cid->flow_regulator.estimate_usecs_for_bytes(max_len) >> 1;
      if (horizon > target_duration)
        horizon = target_duration;
      kdu_long disparity_usecs =
        find_disparity_compensation(horizon,cid->target_end_time_disparity,
                                    cid->outstanding_target_duration,
                                    cid->outstanding_disparity_compensation);
      int disparity_bytes =
        cid->flow_regulator.estimate_bytes_for_usecs(disparity_usecs);
      if (disparity_bytes >= max_len)
        disparity_bytes = max_len-1; // Should not really happen
      max_len -= disparity_bytes; // Limit for target part of the request
      int target_bytes =
        cid->flow_regulator.estimate_bytes_for_usecs(target_duration);
      if (target_bytes > max_len)
        { // Reduce target duration; if the request was posted as a timed
          // request, the ensuing call to `adjust_request_timing' will
          // split the request in two, with new nominal start times and
          // posted request times.
          double fraction = max_len / (double) target_bytes;
          fraction = (fraction > 0.7)?0.7:fraction; // We may ask for less,
            // so as to split the request up more evenly, but the disparity
            // compensation calculated above was based on a `horizon' of
            // only 50% of the original byte limit, so this reduction in the
            // request size should still leave us with a reasonable value for
            // `disparity_usecs'.
          target_bytes = (int)(target_bytes * fraction);
          assert((target_bytes + disparity_bytes) > 0);
          target_duration = (kdu_long)(0.5 + req->target_duration * fraction);
        }
      req->byte_limit = target_bytes + disparity_bytes;
      if (req->byte_limit < 1)
        req->byte_limit = 1; // Just in case
      req->disparity_compensation = disparity_usecs;
      cid->adjust_request_timing(req,target_duration);
          // The above call augments `cid->last_target_end_time' by
          // `target_duration' and makes sure that the average
          // `next_nominal_start_time' amongst the CID's queue's grows by
          // exactly the same amount.  The function also decides whether
          // or not `req' should be duplicated -- this happens if `req'
          // was not posted as a timed request or if the amount by which
          // the associated queue's `next_nominal_start_time' member is
          // incremented is not close enough to `req->posted_service_time'.
      assert(req->target_duration == target_duration);
      this->last_noted_target_duration = target_duration;
      req->target_end_time = cid->last_target_end_time;
      cid->outstanding_disparity_compensation += req->disparity_compensation;
    }
  else if (!cid->uses_aux_channel)
    { 
      int num_outstanding_bytes = cid->calculate_num_outstanding_bytes();
      req->byte_limit =
        cid->flow_regulator.get_request_byte_limit(num_outstanding_bytes,true);
      if (req->next_copy == NULL)
        duplicate_request(req); // So we issue the request again in the future
    }
  
  // Add unparsed query fields, if any
  if (req->extra_query_fields != NULL)
    {
      query_block << req->extra_query_fields;
      query_block << "&"; // For sure we have at least one more request field
    }
  
  // Add target identification query fields
  if (cid->channel_id != NULL)
    query_block << JPIP_FIELD_CHANNEL_ID "=" << cid->channel_id;
  else
    { // No session (yet) need to identify target & return type explicitly
      query_block << JPIP_FIELD_TYPE "=" << "jpp-stream";
      if (client->target_id[0] != '\0')
        query_block << "&" JPIP_FIELD_TARGET_ID "=" << client->target_id;
      else
        {
          if (client->target_name != NULL)
            query_block << "&" JPIP_FIELD_TARGET "=" << client->target_name;
          if (client->sub_target_name != NULL)
            query_block << "&" JPIP_FIELD_SUB_TARGET "="
              << client->sub_target_name;
          query_block << "&" JPIP_FIELD_TARGET_ID "=0"; // Ask for target-id
        }
    }
  
  // Add channel/session manipulation fields
  if (just_started && (client->requested_transport[0] != '\0'))
    { 
      query_block << "&" JPIP_FIELD_CHANNEL_NEW "=";
      query_block << client->requested_transport;
      if (kdcs_has_caseless_prefix(client->requested_transport,"http-udp"))
        this->unreliable_transport = true; // Until proven otherwise
    }
  if (close_when_idle && (req == request_tail) && (cid->channel_id != NULL))
    { // Issue a channel-close request unless there are other queues which
      // are still alive.
      kdc_request_queue *qscan;
      for (qscan=client->request_queues; qscan != NULL; qscan=qscan->next)
        if ((qscan->cid == cid) && !qscan->close_when_idle)
          break;
      if (qscan == NULL)
        {
          query_block << "&" JPIP_FIELD_CHANNEL_CLOSE "=" << cid->channel_id;
          cid->channel_close_requested = true;
          gaps_to_abandon = cid->find_gaps_to_abandon(current_time,true,
                                                      gaps_to_abandon);
        }
   }

  // Add Request-id field if sequencing of requests is important
  if (primary->using_proxy || this->unreliable_transport)
    { 
      req->qid = cid->next_qid++;
      query_block << "&" JPIP_FIELD_REQUEST_ID "=" << req->qid;
    }

  // Issue an abandonment request if `gaps_to_abandon' is non-NULL, along with
  // a Barrier request field to inform the server that we have abandoned
  // whole requests, so that there will be no further abandonment of these
  // requests -- this will probably make no difference, but could potentially
  // provide the server with implicit acknowledgement of some chunks for
  // which it might not have received the acknowledgement.
  bool wrote_gap=false;
  kdu_long barrier_qid = 0;
  kdc_chunk_gap *gap = gaps_to_abandon;
  while (gap != NULL)
    { 
      if (!wrote_gap)
        { 
          req->obliterating = wrote_gap = true;
          query_block << "&" JPIP_FIELD_CHUNK_ABANDON "=";
        }
      else
        query_block << ",";
      if (gap->qid > barrier_qid)
        barrier_qid = gap->qid;
      query_block << ((kdu_uint16) gap->qid) << ":";
      query_block << gap->seq_from;
      if (gap->seq_to != gap->seq_from)
        { 
          query_block << "-";
          if (gap->seq_to > 0)
            query_block << gap->seq_to;
        }
      gap = gap->next;
    }
  if (wrote_gap)
    query_block << "&" JPIP_FIELD_BARRIER_ID "=" << barrier_qid;
      
  // Add window-related query fields
  if ((req->window.resolution.x > 0) && (req->window.resolution.y > 0) &&
      (req->window.region.size.x > 0) && (req->window.region.size.y > 0))
    {
      // Perform some modifications if necessary, to be sure we are not
      // issuing an illegal request.
      int x_pos=req->window.region.pos.x,  y_pos=req->window.region.pos.y;
      int x_siz=req->window.region.size.x, y_siz=req->window.region.size.y;
      if (x_pos < 0) { x_siz += x_pos; x_pos = 0; }
      if (y_pos < 0) { y_siz += y_pos; y_pos = 0; }
      if (x_siz < 1) x_siz = 1;
      if (y_siz < 1) y_siz = 1;
      if ((x_pos+x_siz) > req->window.resolution.x)
        x_siz = req->window.resolution.x - x_pos;
      if ((y_pos+y_siz) > req->window.resolution.y)
        y_siz = req->window.resolution.y - y_pos;
      if (x_siz < 1) { x_siz = 1; x_pos = req->window.resolution.x-1; }
      if (y_siz < 1) { y_siz = 1; y_pos = req->window.resolution.y-1; }

      query_block << "&" << JPIP_FIELD_FULL_SIZE "="
        << req->window.resolution.x << "," << req->window.resolution.y;
      if (req->window.round_direction > 0)
        query_block << ",round-up";
      else if (req->window.round_direction == 0)
        query_block << ",closest";
      query_block << "&" JPIP_FIELD_REGION_OFFSET "=" << x_pos << "," << y_pos;
      query_block << "&" JPIP_FIELD_REGION_SIZE "=" << x_siz << "," << y_siz;
    }
  
  if (!req->window.components.is_empty())
    {
      query_block << "&" << JPIP_FIELD_COMPONENTS "=";
      int c=0;
      kdu_sampled_range *rg;
      for (; (rg=req->window.components.access_range(c)) != NULL; c++)
        {
          if (c > 0)
            query_block << ",";
          query_block << rg->from;
          if (rg->to == INT_MAX)
            query_block << "-";
          else if (rg->to > rg->from)
            query_block << "-" << rg->to;
        }
    }
  
  if (req->window.codestreams.is_empty() && req->window.contexts.is_empty())
    req->window.codestreams.add(0); // Explicitly include the default
  
  if (!req->window.codestreams.is_empty())
    { 
      int c;
      bool request_field_started = false;
      kdu_sampled_range *rg;
      for (c=0; (rg=req->window.codestreams.access_range(c)) != NULL; c++)
        { 
          if (rg->context_type == KDU_JPIP_CONTEXT_TRANSLATED)
            continue;
          if (!request_field_started)
            { // We do this because it may turn out that all codestreams are
              // in fact translated from context requests
              query_block << "&" << JPIP_FIELD_CODESTREAMS "=";
              request_field_started = true;
            }
          if (c > 0)
            query_block << ",";
          query_block << rg->from;
          if (rg->to > (INT_MAX-rg->step))
            query_block << "-";
          else if (rg->to > rg->from)
            query_block << "-" << rg->to;
          if (rg->step != 1)
            query_block << ":" << rg->step;
        }
    }
  
  if (!req->window.contexts.is_empty())
    { // Generally need to hex-hex encode context requests
      query_block << "&" << JPIP_FIELD_CONTEXTS "=";
      int hex_hex_start = query_block.get_remaining_bytes();
      int c=0;
      kdu_sampled_range *rg;
      for (; (rg=req->window.contexts.access_range(c)) != NULL; c++)
        {
          if (c > 0)
            query_block << ",";
          if ((rg->context_type != KDU_JPIP_CONTEXT_JPXL) &&
              (rg->context_type != KDU_JPIP_CONTEXT_MJ2T))
            continue;
          if (rg->context_type == KDU_JPIP_CONTEXT_JPXL)
            query_block << "jpxl";
          else if (rg->context_type == KDU_JPIP_CONTEXT_MJ2T)
            query_block << "mj2t";
          else
            assert(0);
          query_block << "<" << rg->from;
          if (rg->to > rg->from)
            query_block << "-" << rg->to;
          if ((rg->step > 1) && (rg->to > rg->from))
            query_block << ":" << rg->step;
          if ((rg->context_type == KDU_JPIP_CONTEXT_MJ2T) &&
              (rg->remapping_ids[1] == 0))
            query_block << "+now";
          query_block << ">";
          if (rg->context_type == KDU_JPIP_CONTEXT_JPXL)
            {
              if ((rg->remapping_ids[0] >= 0) && (rg->remapping_ids[1] >= 0))
                query_block << "[s" << rg->remapping_ids[0]
                << "i" << rg->remapping_ids[1] << "]";
            }
          else if (rg->context_type == KDU_JPIP_CONTEXT_MJ2T)
            {
              if (rg->remapping_ids[0] == 0)
                query_block << "[track]";
              else if (rg->remapping_ids[0] == 1)
                query_block << "[movie]";
            }
        }
      int hex_hex_chars = query_block.get_remaining_bytes() - hex_hex_start;
      query_block.hex_hex_encode_tail(hex_hex_chars,"?&=");
    }
  
  if (req->window.max_layers > 0)
    query_block << "&" << JPIP_FIELD_LAYERS "=" << req->window.max_layers;
  if (req->window.metareq != NULL)
    { // Generally need to hex-hex encode metareqs
      kdu_metareq *mrq_start, *mrq_lim, *smrq;
      query_block << "&" << JPIP_FIELD_META_REQUEST "=";
      int hex_hex_start = query_block.get_remaining_bytes();
      for (mrq_start=req->window.metareq; mrq_start != NULL; mrq_start=mrq_lim)
        {
          mrq_lim = mrq_start->next;
          while ((mrq_lim != NULL) &&
                 (mrq_lim->root_bin_id == mrq_start->root_bin_id) &&
                 (mrq_lim->max_depth == mrq_start->max_depth))
            mrq_lim = mrq_lim->next; // Can combine multiple requests
          query_block << "[";
          for (smrq=mrq_start; smrq != mrq_lim; smrq=smrq->next)
            { // Concatenate the [] contents of all compatible requests
              if (smrq != mrq_start)
                query_block << ";";
              if (smrq->box_type == 0)
                query_block << "*";
              else
                {
                  char typebuf[17]; // Allow for all bytes to use octal
                 query_block << kdu_write_type_code(smrq->box_type,typebuf);
                }
              if (smrq->recurse)
                query_block << ":r";
              else if (smrq->byte_limit < INT_MAX)
                query_block << ":" << smrq->byte_limit;
              if ((smrq->qualifier != KDU_MRQ_DEFAULT) &&
                  (smrq->qualifier & KDU_MRQ_ANY))
                {
                  query_block << "/";
                  if (smrq->qualifier & KDU_MRQ_WINDOW)
                    query_block << "w";
                  if (smrq->qualifier & KDU_MRQ_STREAM)
                    query_block << "s";
                  if (smrq->qualifier & KDU_MRQ_GLOBAL)
                    query_block << "g";
                  if (smrq->qualifier & KDU_MRQ_ALL)
                    query_block << "a";
                }
              if (smrq->priority)
                query_block << "!";
            }
          query_block << "]";
          if (mrq_start->root_bin_id != 0)
            {
              query_block << "R";
              kdu_long tmp, id = mrq_start->root_bin_id;
              if (id < 0) id = 0;
              int num_digits = 1;
              for (tmp=id; tmp > 9; tmp /= 10, num_digits++);
              assert(num_digits < 24);
              char buf[24]; buf[num_digits] = '\0';
              for (; num_digits > 0; id = id / 10)
                buf[--num_digits] = (char)('0'+(id % 10));
              query_block << buf;
            }
          if (mrq_start->max_depth < INT_MAX)
            query_block << "D" << mrq_start->max_depth;
          if (mrq_lim != NULL)
            query_block << ",";
        }
      if (req->window.metadata_only)
        query_block << "!!";
      int hex_hex_chars = query_block.get_remaining_bytes() - hex_hex_start;
      query_block.hex_hex_encode_tail(hex_hex_chars,"?&=");
    }
  
  // Add other request qualifying fields
  if (req->byte_limit > 0)
    query_block << "&" << JPIP_FIELD_MAX_LENGTH "=" << req->byte_limit;
  bool request_is_preemptive = (cid->channel_id != NULL);
  if (request_is_preemptive &&
      (cid->last_request_had_byte_limit || !req->preemptive))
    { 
      request_is_preemptive = false;
      query_block << "&" << JPIP_FIELD_WAIT "=yes";
    }
  
  // Add cache model manipulation fields
  if (client->is_stateless || req->new_elements)
    { // Generally need to hex-hex encode model manipulation request
      int hex_hex_start = query_block.get_remaining_bytes();
      if (client->signal_model_corrections(req->window,query_block))
        {
          const char *peek = // Beware: `peek' is NOT null-terminated!!
            ((const char *) query_block.peek_block()) + hex_hex_start;
          assert(*peek == '&');
          int hex_hex_chars = query_block.get_remaining_bytes()-hex_hex_start;
          while ((hex_hex_chars > 0) && (*peek != '='))
            { peek++; hex_hex_chars--; }
          if (hex_hex_chars > 1)
            query_block.hex_hex_encode_tail(hex_hex_chars-1,"?&=");
        }
    }
  
  // Add service preference modifications
  int pref_sets_to_signal = cid->prefs.update(this->prefs);
  if (client->is_stateless)
    pref_sets_to_signal = cid->prefs.preferred | cid->prefs.required;
  if (pref_sets_to_signal != 0)
    {
      int num_chars = cid->prefs.write_prefs(NULL,pref_sets_to_signal);
      char *pref_buf = new char[1+num_chars];
      cid->prefs.write_prefs(pref_buf,pref_sets_to_signal);
      query_block << "&" << JPIP_FIELD_PREFERENCES "=";
      query_block.write_raw((kdu_byte *) pref_buf,num_chars);
      query_block.hex_hex_encode_tail(num_chars,"?&=");
      delete[] pref_buf;
    }
  
  // May need to make `req' dependent upon requests in this or another queue.
  // Note, we must do this before adjusting `this->first_unrequested'.
  kdc_request_queue *qscan;
  for (qscan=client->request_queues; qscan != NULL; qscan=qscan->next)
    { 
      if (!qscan->unreliable_transport)
        continue;
      kdc_request *dep, *latest_dep=NULL;
      for (dep=qscan->first_incomplete; dep != qscan->first_unrequested;
           dep=dep->next)
        if (!dep->communication_complete())
          latest_dep = dep;
      if (latest_dep != NULL)
        req->add_dependency(latest_dep);
    }
  
  // May need to make `req' a dependency for requests issued over other CID's
  if (this->unreliable_transport)
    for (qscan=client->request_queues; qscan != NULL; qscan=qscan->next)
      { 
        if (qscan->cid == this->cid)
          continue;
        kdc_request *dep;
        for (dep=qscan->first_incomplete; dep != qscan->first_unrequested;
             dep=dep->next)
          if (!dep->response_terminated)
            dep->add_dependency(req);
      }
  
  // May need to add an initial chunk gap to the newly issued request
  if (this->unreliable_transport)
    { 
      req->chunk_gaps = client->alloc_chunk_gap();
      req->chunk_gaps->qid = req->qid;
      req->chunk_gaps->seq_from = 0;
      req->chunk_gaps->seq_to = -1;
      req->chunk_gaps->next = NULL;
    }
  
  // Advance the `first_unrequested' pointer, make ourselves the primary
  // channel's active requester and put the new request on the primary
  // channel's active request list and the CID's active receiver list. 
  primary->active_requester = this;
  first_unrequested = req->next;
  primary->set_last_active_request(req);
  cid->set_last_active_receiver(req);
  cid->last_requester = this;

  // Prepare the complete request in `send_block'
  int query_bytes = query_block.get_remaining_bytes();
  bool using_post = false;
  if ((query_bytes + strlen(cid->resource)) < 200)
    {
      send_block << "GET ";
      if (primary->using_proxy)
        {
          send_block << "http://" << cid->server;
          if (cid->request_port != 80)
            send_block << ":" << cid->request_port;
        }
      send_block << "/" << cid->resource << "?";
      send_block.append(query_block);
      send_block << " HTTP/1.1\r\n";
      query_block.restart();
    }
  else
    { // Using Post request
      using_post = true;
      send_block << "POST ";
      if (primary->using_proxy)
        {
          send_block << "http://" << cid->server;
          if (cid->request_port != 80)
            send_block << ":" << cid->request_port;
        }
      send_block << "/" << cid->resource << " HTTP/1.1\r\n";
      send_block << "Content-type: application/x-www-form-urlencoded\r\n";
      send_block << "Content-length: " << query_bytes << "\r\n";
    }
  if ((cid->server[0] != '[') &&
      kdcs_sockaddr::test_ip_literal(cid->server,KDCS_ADDR_FLAG_IPV6_ONLY))
    send_block << "Host: [" << cid->server << "]";
  else
    send_block << "Host: " << cid->server;
  if (cid->request_port != 80)
    send_block << ":" << cid->request_port;
  send_block << "\r\n";
  
  // See if the primary channel can remain persistent
  if (!client->check_for_local_cache)
    { // May need to dispatch a new request after cache is checked
      if (client->non_interactive)
        primary->is_persistent = false;
      else if (primary->is_persistent && close_when_idle &&
               !primary->keep_alive)
        { // This may be the last request over this HTTP channel.  To be sure,
          // we need to verify that there are no request queues using the
          // channel which could potentially need to deliver another request.
          for (qscan=client->request_queues; qscan != NULL; qscan=qscan->next)
            if ((qscan->cid->primary_channel == primary) &&
                ((qscan->first_unrequested != NULL) ||
                 !qscan->close_when_idle))
              break;
          if (qscan == NULL)
            primary->is_persistent = false;
        }
    }
  if (!primary->is_persistent)
    {
      primary->keep_alive = false;
      send_block << "Connection: close\r\n";
    }
  
  if (!client->is_stateless)
    send_block << "Cache-Control: no-cache\r\n";
  send_block << "\r\n";
  if (using_post)
    { // Write the POST request's entity body
      send_block.append(query_block);
      query_block.restart();
    }
  
  // Finally, see if we need to synthesize a copy of any potentially
  // pre-empted request from another request queue.
  if (request_is_preemptive && (cid->num_request_queues > 1))
    { 
      kdc_request *qreq = cid->first_active_receiver;
      for (; qreq != NULL; qreq=qreq->cid_next_receiver)
        { 
          kdc_request_queue *queue = qreq->queue;
          if ((queue != this) && (qreq->next == queue->first_unrequested) &&
              (!qreq->response_terminated) && (qreq->next_copy == NULL) &&
              ((qreq->next == NULL) || !qreq->next->preemptive))
            { // This request is likely to be pre-empted within the server
              // because the server is not aware that the single CID has
              // multiple request queues.  However, the client's application
              // does not expect the request to be pre-empted, because it
              // has not issued a subsequent pre-empting request.  So to
              // make sure the client application gets what it expects, we
              // have to make a copy of the request on the queue.  It is
              // possible that this copy is redundant (if the current
              // outstanding request does not get pre-empted).  To minimize
              // the likelihood of this, the `process_completed_requests'
              // function checks for future requests which are rendered
              // redundant by the completion of a current request.
              qreq->queue->duplicate_request(qreq);
            }
        }
    }
}

/*****************************************************************************/
/*                      kdc_request_queue::process_reply                     */
/*****************************************************************************/

kdc_request *kdc_request_queue::process_reply(const char *reply,
                                              kdu_long &current_time)
{
  if ((*reply == '\0') || (*reply == '\n'))
    return NULL;
  
  kdc_primary *primary = cid->primary_channel;
  
  const char *header, *cp = strchr(reply,' ');
  int code;
  float version;
  if (!(kdcs_has_caseless_prefix(reply,"HTTP/") &&
        sscanf(reply+5,"%f",&version)))
    { KDU_ERROR(e,14); e <<
      KDU_TXT("Server reply to client window request does not "
              "appear to contain an HTTP version number as the first token.  "
              "Complete server response is:\n\n") << reply;
    }
  if (version < 1.1)
    primary->is_persistent = primary->keep_alive = false;
  else if ((header = kdcs_caseless_search(reply,"\nConnection:")) != NULL)
    {
      while (*header == ' ') header++;
      if (kdcs_has_caseless_prefix(header,"close"))
        primary->is_persistent = primary->keep_alive = false;
    }
  if ((cp == NULL) || (sscanf(cp+1,"%d",&code) == 0))
    { KDU_ERROR(e,15); e <<
      KDU_TXT("Server reply to client window request "
              "does not appear to contain a status code as the second token.  "
              "Complete server response is:\n\n") << reply;
    }
  if (code >= 400)
    { KDU_ERROR(e,16); e <<
      KDU_TXT("Server could not process client window request.  "
              "Complete server response is:\n\n") << reply;
    }
  if ((code >= 100) && (code < 200))
    return NULL; // Ignore 100-series responses

  // Find the relevant request and update timing statistics
  kdc_request *req = first_unreplied;
  assert((req != NULL) && !req->reply_received);
  if (req->chunk_received || !cid->uses_aux_channel)
    { // Enough to wait for reply only
      assert(req->request_issue_time >= 0);
      cid->update_request_rtt(current_time - req->request_issue_time);
    }
  
  // Process window-specific JPIP response headers
  int val1, val2;
  if ((header = kdcs_parse_jpip_header(reply,JPIP_FIELD_MAX_LENGTH)) != NULL)
    {
      if ((sscanf(header,"%d",&val1) != 1) || (val1 < 0))
        {
          KDU_ERROR(e,18); e <<
          KDU_TXT("Incorrectly formatted")
          << " \"JPIP-" JPIP_FIELD_MAX_LENGTH ":\" " <<
          KDU_TXT("header in server's reply to window request.  "
                  "Expected a strictly positive byte limit parameter.  "
                  "Complete server reply paragraph was:\n\n")
          << reply;
        }
      if (val1 > req->byte_limit)
        cid->flow_regulator.set_min_request_byte_limit(val1);
    }
  if ((header = kdcs_parse_jpip_header(reply,JPIP_FIELD_FULL_SIZE)) != NULL)
    {
      if ((sscanf(header,"%d,%d",&val1,&val2) != 2) ||
          (val1 <= 0) || (val2 <= 0))
        {
          KDU_ERROR(e,19); e <<
          KDU_TXT("Incorrectly formatted")
          << " \"JPIP-" JPIP_FIELD_FULL_SIZE ":\" " <<
          KDU_TXT("header in server's reply to window request.  "
                  "Expected positive horizontal and vertical dimensions, "
                  "separated only by a comma.  Complete server reply "
                  "paragraph was:\n\n")
          << reply;
        }
      req->window.resolution.x = val1;
      req->window.resolution.y = val2;
    }
  if ((header=kdcs_parse_jpip_header(reply,JPIP_FIELD_REGION_OFFSET)) != NULL)
    {
      if ((sscanf(header,"%d,%d",&val1,&val2) != 2) ||
          (val1 < 0) || (val2 < 0))
        {
          KDU_ERROR(e,20); e <<
          KDU_TXT("Incorrectly formatted")
          << " \"JPIP-" JPIP_FIELD_REGION_OFFSET ":\" " <<
          KDU_TXT("header in server's reply to window request.  "
                  "Expected non-negative horizontal and vertical offsets from "
                  "the upper left hand corner of the requested image "
                  "resolution.  Complete server reply paragraph was:\n\n")
          << reply;
        }
      req->window.region.pos.x = val1;
      req->window.region.pos.y = val2;
    }
  if ((header = kdcs_parse_jpip_header(reply,JPIP_FIELD_REGION_SIZE)) != NULL)
    {
      if ((sscanf(header,"%d,%d",&val1,&val2) != 2) ||
          (val1 < 0) || (val2 < 0))
        { 
          KDU_ERROR(e,21); e <<
          KDU_TXT("Incorrectly formatted")
          << " \"JPIP-" JPIP_FIELD_REGION_SIZE ":\" " <<
          KDU_TXT("header in server's reply to window request.  "
                  "Expected non-negative horizontal and vertical dimensions "
                  "for the region of interest within the requested image "
                  "resolution.  Complete server reply paragraph was:\n\n")
          << reply;
        }
      req->window.region.size.x = val1;
      req->window.region.size.y = val2;
    }
  if ((header = kdcs_parse_jpip_header(reply,JPIP_FIELD_COMPONENTS)) != NULL)
    {
      char *end_cp;
      int from, to;
      req->window.components.init();
      while ((*header != '\0') && (*header != '\n') && (*header != ' '))
        {
          while (*header == ',')
            header++;
          from = to = strtol(header,&end_cp,10);
          if (end_cp > header)
            {
              header = end_cp;
              if (*header == '-')
                {
                  header++;
                  to = strtol(header,&end_cp,10);
                  if (end_cp == header)
                    to = INT_MAX;
                  header = end_cp;
                }
            }
          else
            header-=3; // Force an error
          if (((*header != ',') && (*header != ' ') && (*header != '\0') &&
               (*header != '\n')) || (from < 0) || (from > to))
            {
              KDU_ERROR(e,22); e <<
              KDU_TXT("Incorrectly formatted")
              << " \"JPIP-" JPIP_FIELD_COMPONENTS ":\" " <<
              KDU_TXT("header in server's reply to window request.  "
                      "Complete server reply paragraph was:\n\n")
              << reply;
            }
          req->window.components.add(from,to);
        }
    }
  if ((header = kdcs_parse_jpip_header(reply,JPIP_FIELD_CODESTREAMS)) != NULL)
    {
      char *end_cp;
      kdu_sampled_range range;
      req->window.codestreams.init();
      while ((*header != '\0') && (*header != '\n') && (*header != ' '))
        {
          if (*header == ',')
            header++;
          
          // Check for a translation identifier
          range.context_type = 0;
          if (*header == '<')
            {
              range.context_type = KDU_JPIP_CONTEXT_TRANSLATED;
              header++;
              range.remapping_ids[0] = strtol(header,&end_cp,10);
              if ((end_cp == header) || (range.remapping_ids[0] < 0) ||
                  (*end_cp != ':'))
                {
                  KDU_ERROR(e,23); e <<
                  KDU_TXT("Illegal translation identifier in")
                  << " \"JPIP-" JPIP_FIELD_CODESTREAMS ":\" " <<
                  KDU_TXT("header in server's reply to window request.  "
                          "Complete server reply paragraph was:\n\n")
                  << reply;
                }
              header = end_cp+1;
              range.remapping_ids[1] = strtol(header,&end_cp,10);
              if ((end_cp == header) || (range.remapping_ids[1] < 0) ||
                  (*end_cp != '>'))
                {
                  KDU_ERROR(e,24); e <<
                  KDU_TXT("Illegal translation identifier in")
                  << " \"JPIP-" JPIP_FIELD_CODESTREAMS ":\" " <<
                  KDU_TXT("header in server's reply to window request.  "
                          "Complete server reply paragraph was:\n\n")
                  << reply;
                }
              header = end_cp+1;
            }
          
          // Parse the rest of the list element into a sampled range
          range.step = 1;
          range.from = range.to = strtol(header,&end_cp,10);
          if (end_cp > header)
            {
              header = end_cp;
              if (*header == '-')
                {
                  header++;
                  range.to = strtol(header,&end_cp,10);
                  if (end_cp == header)
                    range.to = INT_MAX;
                  header = end_cp;
                }
              if (*header == ':')
                {
                  range.step = strtol(header+1,&end_cp,10);
                  if (end_cp > (header+1))
                    header = end_cp;
                }
            }
          else
            header-=3; // Force an error
          if (((*header != ',') && (*header != ' ') && (*header != '\0') &&
               (*header != '\n')) || (range.from < 0) ||
              (range.from > range.to) || (range.step < 1))
            {
              KDU_ERROR(e,25); e <<
              KDU_TXT("Illegal value or range in")
              << " \"JPIP-" JPIP_FIELD_CODESTREAMS ":\" " <<
              KDU_TXT("header in server's reply to window request.  "
                      "Complete server reply paragraph was:\n\n")
              << reply;
            }
          req->window.codestreams.add(range);
        }
    }
  
  if ((header = kdcs_parse_jpip_header(reply,JPIP_FIELD_CONTEXTS)) != NULL)
    {
      req->window.contexts.init();
      while ((*header != '\0') && (*header != '\n'))
        {
          while ((*header == ';') || (*header == ' '))
            header++;
          cp = req->window.parse_context(header);
          if ((*cp != ';') && (*cp != '\n') && (*cp != ' ') && (*cp != '\0'))
            {
              KDU_ERROR(e,26); e <<
              KDU_TXT("Incorrectly formatted")
              << " \"JPIP-" JPIP_FIELD_CONTEXTS ":\" " <<
              KDU_TXT("header in server's reply to window request.  "
                      "Complete server reply paragraph was:\n\n")
              << reply;
            }
          header = cp;
        }
    }
  
  if ((header = kdcs_parse_jpip_header(reply,JPIP_FIELD_LAYERS)) != NULL)
    {
      if ((sscanf(header,"%d",&val1) != 1) || (val1 < 0))
        { 
          KDU_ERROR(e,27); e <<
          KDU_TXT("Incorrectly formatted")
          << " \"JPIP-" JPIP_FIELD_LAYERS ":\" " <<
          KDU_TXT("header in server's reply to window request.  "
                  "Expected non-negative maximum number of quality layers.  "
                  "Complete server reply paragraph was:\n\n")
          << reply;
        }
      req->window.max_layers = val1;
    }
  
  if ((header = kdcs_parse_jpip_header(reply,JPIP_FIELD_META_REQUEST)) != NULL)
    {
      req->window.init_metareq();
      for (cp=header; (*cp != '\0') && (*cp != ' ') && (*cp != '\n'); cp++);
      int mrbuf_len = (int)(cp-header);
      char *mrbuf = client->make_temp_string(header,mrbuf_len);
      kdu_hex_hex_decode(mrbuf); // Mainly just to catch hex-encoded box types
      cp = req->window.parse_metareq(mrbuf);
      if (cp != NULL)
        {
          KDU_ERROR(e,28); e <<
          KDU_TXT("Incorrectly formatted")
          << " \"JPIP-" JPIP_FIELD_META_REQUEST ":\" " <<
          KDU_TXT("header in server's reply to window request.  Error "
                  "encountered at:\n\n\t" << cp << "\n\nComplete server reply "
                  "paragraph was:\n\n") << reply;
        }
    }
  
  // Check for availability of a new target-id
  if ((header = kdcs_parse_jpip_header(reply,JPIP_FIELD_TARGET_ID)) != NULL)
    {
      int length = (int) strcspn(header," \n");
      if (length < 256)
        {
          char new_id[256];
          strncpy(new_id,header,length);
          new_id[length] = '\0';
          if (new_id[0] == '0')
            { // Check for numerical 0, converting to a single 0.
              for (cp=new_id; *cp != '\0'; cp++)
                if (*cp != '0')
                  break;
              if (*cp == '\0')
                new_id[1] = '\0';
            }
          if (client->target_id[0] == '\0')
            strcpy(client->target_id,new_id);
          else if (strcmp(client->target_id,new_id) != 0)
            { // Target ID has changed while in the middle of browsing
              KDU_ERROR(e,29); e <<
              KDU_TXT("Server appears to have issued a new unique target "
                      "identifier, while we were in the middle of browsing "
                      "the image.  Most likely, the image has been modified "
                      "on the server and you should re-open it.");
            }
        }
    }
  else if (client->target_id[0] == '\0')
    { // Must have specified `tid=0' in request
      KDU_ERROR(e,30); e <<
      KDU_TXT("Server has responded with a successful status code, but has "
              "not included a")
      << " \"JPIP-" JPIP_FIELD_TARGET_ID ":\" " <<
      KDU_TXT("response header, even though we requested the target-id with a")
      << "\"" JPIP_FIELD_TARGET_ID "=0\" " <<
      KDU_TXT("request field.  Complete server reply paragraph was:\n\n")
      << reply;
    }
  
  // Check for establishment of a new channel
  bool was_stateless = client->is_stateless;
  if ((header = kdcs_parse_jpip_header(reply,JPIP_FIELD_CHANNEL_NEW)) != NULL)
    { 
      if ((!just_started) || (client->requested_transport[0] == '\0'))
        {
          KDU_ERROR(e,31); e <<
          KDU_TXT("Server appears to have issued a new channel ID where "
                  "none was requested!!");
        }
      
      // Create a new CID for this request queue, unless the connection is
      // currently stateless, in which case we can re-use the existing one.
      if (!client->is_stateless)
        { 
          kdc_cid *new_cid = client->add_cid(cid->primary_channel,
                                             cid->server,cid->resource);
          new_cid->prefs.copy_from(cid->prefs); // Replicate server inheritance
          new_cid->request_port = cid->request_port;
          new_cid->return_port = cid->return_port;
          new_cid->last_msg_class_id = cid->last_msg_class_id;
          new_cid->last_msg_stream_id = cid->last_msg_stream_id;
          new_cid->server_address = cid->server_address; // In case resolved
          new_cid->next_qid = cid->next_qid; // So qid's are consecutive
          this->transfer_to_new_cid(new_cid,req);
        }
      client->is_stateless = false;
      cid->newly_assigned_by_server = true; // Even if we are using old CID obj
      cid->flow_regulator.set_disjoint_requests(false);
    
      int length = (int) strcspn(header," \n");
      char *channel_params = client->make_temp_string(header-1,length+1);
      channel_params[0] = ',';
      assert(cid->channel_id == NULL);
      if ((header = kdcs_caseless_search(channel_params,",cid=")) != NULL)
        {
          length = (int) strcspn(header,",");
          cid->channel_id = make_new_string(header,length);
          for (kdc_cid *scan=client->cids; scan != NULL; scan=scan->next)
            if ((scan != cid) &&
                (strcmp(scan->channel_id,cid->channel_id) == 0))
              { KDU_WARNING(w,0x02040901); w <<
                KDU_TXT("Server has assigned the same JPIP Channel ID to a "
                        "new channel (via a")
                << " \"JPIP-" JPIP_FIELD_CHANNEL_NEW "\" " <<
                KDU_TXT("response header) as that used for a previously "
                        "assigned JPIP channel.  This is probably illegal, "
                        "unless the server has only just closed the old "
                        "channel, in which case it is just very bad "
                        "practice.");
              }
        }
      if ((header=kdcs_caseless_search(channel_params,",transport=")) != NULL)
        { 
          cid->uses_aux_channel = cid->aux_channel_is_udp = false;
          if (kdcs_has_caseless_prefix(header,"http-udp"))
            cid->uses_aux_channel = cid->aux_channel_is_udp = true;
          else if (kdcs_has_caseless_prefix(header,"http-tcp"))
            cid->uses_aux_channel = true;
          if (cid->uses_aux_channel)
            { 
              cid->return_port = cid->request_port;
              assert(cid->primary_channel->num_http_only_cids > 0);
              cid->primary_channel->num_http_only_cids--;
              cid->primary_channel->num_http_aux_cids++;
            }
        }
      if ((header = kdcs_caseless_search(channel_params,",host=")) != NULL)
        {
          length = (int) strcspn(header,",");
          char *new_server = make_new_string(header,length);
          if (strcmp(cid->server,new_server) != 0)
            cid->server_address.reset(); // Need to resolve address later
          delete[] cid->server;
          cid->server = new_server;
        }
      if ((header = kdcs_caseless_search(channel_params,",port=")) != NULL)
        {
          if (sscanf(header,"%d",&val1) != 0)
            cid->request_port = cid->return_port = (kdu_uint16) val1;
        }
      if ((header = kdcs_caseless_search(channel_params,",auxport=")) != NULL)
        {
          if (sscanf(header,"%d",&val1) != 0)
            cid->return_port = (kdu_uint16) val1;
        }
      if ((header = kdcs_caseless_search(channel_params,",path=")) != NULL)
        {
          delete[] cid->resource;
          cid->resource = NULL;
          length = (int) strcspn(header,",");
          cid->resource = make_new_string(header,length);
        }
      if ((cid->channel_id == NULL) || (cid->channel_id[0] == '\0'))
        {
          KDU_ERROR(e,0x13030901); e <<
          KDU_TXT("Server has failed to include a non-empty new channel-id "
                  "in the set of channel parameters returned via the")
          << " \"JPIP-" JPIP_FIELD_CHANNEL_NEW ":\" " <<
          KDU_TXT("header in its HTTP reply paragraph.  "
                  "Complete server reply paragraph was:\n\n") << reply;
        }
      
      // Now resolve the `cid->server_address' if we don't know it already and
      // we will need to know it.
      if ((!cid->server_address.is_valid()) &&
          (strcmp(cid->server,cid->primary_channel->immediate_server) == 0))
        cid->server_address = cid->primary_channel->immediate_address;
      if ((!cid->server_address.is_valid()) &&
          (cid->uses_aux_channel || !cid->primary_channel->using_proxy))
        {
          assert(client->management_lock_acquired);
          signal_status("Resolving host name for new JPIP channel ...");
          client->release_management_lock();
          resolve_server_address(cid->server,cid->server_address);
          client->acquire_management_lock(current_time);
          signal_status("Host resolved for new JPIP channel.");
          assert(cid->server_address.is_valid());
            // Above function will generate an error if cannot resolve address
        }
    }
  else if (just_started && client->is_stateless)
    client->requested_transport[0] = '\0'; // Don't ask for a session again

  bool parsed_local_cache = false;
  if (client->check_for_local_cache && (client->target_id[0] != '\0'))
    {
      if (client->look_for_compatible_cache_file(current_time))
        parsed_local_cache = true;
      client->check_for_local_cache = false;
    }

  bool obliteration_was_in_progress =
    req->untrusted && (client->obliterating_requests_in_flight == 0);
  bool is_first_reply = just_started;
  if (just_started)
    { // Need to check on reliability of transport
      bool was_unreliable = this->unreliable_transport;
      this->unreliable_transport = cid->aux_channel_is_udp;
      if (this->unreliable_transport && !was_unreliable)
        { KDU_ERROR(e,0x02081002); e <<
          KDU_TXT("Server has created a JPIP channel which uses an unreliable "
                  "transport, yet the client request for a new channel did "
                  "not include any unreliable transports.  This situation is "
                  "both illegal on the part of the server and dangerous for "
                  "the client, because clients need to take special steps "
                  "when issuing requests on unreliable channels to prevent "
                  "inconsistency with requests on other JPIP channels.  We "
                  "were unable to anticipated the possibility of an "
                  "unreliable transport, since we did not request one.");
        }
      else if (was_unreliable && !this->unreliable_transport)
        { // Need to remove this request from any `dependencies' list and also
          // need to remove the opening chunk gap which we created when the
          // unreliably transported request was first issued.
          assert((req == request_head) && (req->chunk_gaps != NULL));
          client->recycle_chunk_gaps(req->chunk_gaps);
          req->chunk_gaps = NULL;
          kdc_request_queue *qscan;
          for (qscan=client->request_queues; qscan != NULL; qscan=qscan->next)
            { 
              if (qscan == this)
                continue;
              kdc_request *rrq=qscan->first_incomplete;
              for (; rrq != qscan->first_unrequested; rrq=rrq->next)
                if (rrq->dependencies != NULL)
                  rrq->remove_dependency(req,NULL);
              qscan->process_completed_requests();
            }
        }
      just_started=false;
    }
  
  if (req->unblock_primary_upon_comms_complete &&
      primary->is_persistent && !client->is_stateless)
    { // Connection turns out to be stateful and persistent; we can unblock
      // the channel (i.e., allow new requests to be issued) now that the
      // reply is complete, rather than waiting for the complete response.
      req->unblock_primary_upon_comms_complete = false;
      req->unblock_primary_upon_reply = true;
    }

  if ((!cid->channel_close_requested) &&
      (parsed_local_cache || obliteration_was_in_progress ||
       (was_stateless && !client->is_stateless)))
    { // Need to duplicate the initial request, so we can signal cache contents
      // to the server, or so we can remove the byte limit supplied with the
      // initial stateless request, or so we can obtain a valid EOR message,
      // since the last request was issued while an obliterating request was
      // in progress.
      if (req->next == NULL)
        { 
          kdc_request *dup = this->duplicate_request(req);
          if (dup != NULL)
            { // Make this request appear to be brand new -- otherwise
              // cache model statements will not be generated
              assert(req->next_copy == dup); // We just made the copy
              dup->copy_src = NULL;
              req->next_copy = NULL;
              dup->preemptive = true;
              dup->new_elements = true;
            }
        }
    }
  
  if (req->obliterating)
    client->obliterating_request_replied();
  req->reply_received = true;
  req->last_event_time = current_time; // In case we encountered delays above
  first_unreplied = req->next;
  
  if (is_first_reply && !client->target_request_successful)
    { 
      client->target_request_successful = true;
      client->signal_status();
    }
  return req;
}

/*****************************************************************************/
/*                 kdc_request_queue::transfer_to_new_cid                    */
/*****************************************************************************/

void
  kdc_request_queue::transfer_to_new_cid(kdc_cid *new_cid, kdc_request *req)
{
  kdc_cid *old_cid = this->cid;
  this->cid = NULL; // Avoid accidentally reassociating with the old CID

  assert(this == old_cid->last_requester);
  old_cid->last_requester = NULL;
  new_cid->last_requester = this;
  
  if (req->target_end_time >= 0)
    { // Account for the fact that `req's response data will be received
      // on `new_cid' -- from the perspective of `old_cid' this is the
      // same as if `req' were completed before any of its byte limit
      // was consumed.
      assert(req->byte_limit > 0);
      int unused_bytes = req->byte_limit;
      kdu_long unused_usecs =
        old_cid->flow_regulator.estimate_usecs_for_bytes(unused_bytes);
      if (unused_usecs > 0)
        { 
          assert(old_cid->last_target_end_time >= 0);
          if (unused_usecs > old_cid->last_target_end_time)
            unused_usecs = old_cid->last_target_end_time; // So it stays >= 0
          for (kdc_request *scn=req; scn != NULL; scn=scn->cid_next_receiver)
            if (scn->target_end_time >= 0)
              scn->target_end_time -= unused_usecs;
          old_cid->last_target_end_time -= unused_usecs;
          kdc_request_queue *queue;
          for (queue=client->request_queues; queue != NULL; queue=queue->next)
            if (queue->cid == old_cid)
              { 
                assert(queue->next_nominal_start_time >= 0);
                queue->next_nominal_start_time -= unused_usecs;
              }
        }
    }
  
  // Drop all timing attributes from `req' before transferring it to `new_cid'
  req->posted_service_time = 0;
  req->nominal_start_time = -1;
  req->target_end_time = -1;
  req->disparity_compensation = 0;

  old_cid->remove_active_receiver(req);
  new_cid->set_last_active_receiver(req);
  if (new_cid->last_idle_time >= 0)
    new_cid->wake_from_idle(-1);

  assert((old_cid->num_request_queues > 0) &&
         (new_cid->num_request_queues == 0));
  old_cid->num_request_queues--;
  new_cid->num_request_queues++;
  
  this->cid = new_cid;
  if (old_cid->num_request_queues == 0)
    { 
      old_cid->reset_request_timing();
      kdc_request_queue *queue = client->add_request_queue(old_cid);
      kdc_request *req = queue->add_request(-1);
      queue->close_when_idle = true;
      req->original_window.init();
      req->window.init();
      req->preemptive = true;
      req->new_elements = false;
    }
  else if (old_cid->last_target_end_time >= 0)
    old_cid->adjust_timing_after_queue_removed();
  
  // Make sure current queue and its request are not in timed request mode;
  // too hard to transfer timed request machinery across to new CID; however,
  // if the request was byte limited, a duplicate was already created.
  assert(new_cid->last_target_end_time < 0); // New CID cannot yet be in timed
                                             // request mode
  next_nominal_start_time = next_posted_start_time = -1;
}

/*****************************************************************************/
/*              kdc_request_queue::adjust_active_usecs_on_idle               */
/*****************************************************************************/

void kdc_request_queue::adjust_active_usecs_on_idle()
{
  if ((!is_idle) || (last_start_time_usecs < 0))
    return; // Should not happen
  kdu_long usecs = client->timer->get_ellapsed_microseconds();
  active_usecs += usecs - last_start_time_usecs;
  last_start_time_usecs = -1;
  if (client->last_start_time_usecs < 0)
    return; // Should not happen
  kdc_request_queue *scan;
  for (scan=client->request_queues; scan != NULL; scan=scan->next)
    if (scan->last_start_time_usecs >= 0)
      return;
  
  // If we get here, all request queues are idle, in the sense used by
  // `client->last_start_time_usecs'.
  client->active_usecs += usecs - client->last_start_time_usecs;
  client->last_start_time_usecs = -1;
}

/*****************************************************************************/
/*            kdc_request_queue::find_initial_posted_start_time              */
/*****************************************************************************/

kdu_long
  kdc_request_queue::find_initial_posted_start_time(kdu_long current_time)
{
  if (this->next_nominal_start_time >= 0)
    return next_nominal_start_time;
  assert(cid->last_target_end_time < 0);
  kdc_request_queue *queue;
  kdu_long result = -1;
  for (queue=client->request_queues; queue != NULL; queue=queue->next)
    { 
      if (queue->cid != this->cid)
        continue;
      kdu_long start_time = queue->next_nominal_start_time;
      if (result < 0)
        result = start_time;
      else if (start_time >= 0)
        assert(start_time == result); // All values must be identical
    }
  if (result < 0)
    result = current_time + cid->request_rtt;
  return result;
}

/*****************************************************************************/
/*            kdc_request_queue::fix_timed_request_discrepancies             */
/*****************************************************************************/

void kdc_request_queue::fix_timed_request_discrepancies()
{
  kdc_request *req = first_unrequested;
  assert((req != NULL) && (req->posted_service_time > 0));
  assert(this->next_nominal_start_time >= 0);
  kdu_long discrepancy = next_nominal_start_time - req->nominal_start_time;
  while (discrepancy > 0)
    { // Need to reduce posted service times, possibly discarding requests
      kdu_long delta = req->posted_service_time;
      if (delta > discrepancy)
        delta = discrepancy;
      req->nominal_start_time += delta;
      req->posted_service_time -= delta;
      discrepancy -= delta;
      if (req->posted_service_time == 0)
        remove_request(req);
      if (((req = first_unrequested) == NULL) ||
          (req->posted_service_time <= 0))
        return;
    }
  if (discrepancy < 0)
    { // Amortise the extra service time over all unrequested reqs
      kdu_long service_span=0;
      for (req=first_unrequested; req != NULL; req=req->next)
        { 
          if (req->posted_service_time <= 0)
            break;
          service_span += req->posted_service_time;
        }
      kdu_long extra_service_time = -discrepancy;
      kdu_long req_time, start_time = next_nominal_start_time;
      for (req=first_unrequested; req != NULL; req=req->next)
        { 
          if ((req_time=req->posted_service_time) <= 0)
            break;
          kdu_long incr = (extra_service_time * req_time) / service_span;
          service_span -= req_time;
          extra_service_time -= incr;
          req->posted_service_time += incr;
          req->nominal_start_time = start_time;
          start_time += req->posted_service_time;
        }
      assert((service_span == 0) && (extra_service_time == 0));
      if (next_posted_start_time >= 0)
        assert(next_posted_start_time == start_time);
    }
}


/* ========================================================================= */
/*                                 kdu_client                                */
/* ========================================================================= */

/*****************************************************************************/
/*                          _kd_start_client_thread_                         */
/*****************************************************************************/

void _kd_start_client_thread_(void *param)
{
  kdu_client *obj = (kdu_client *) param;
  obj->thread_start();
}

/*****************************************************************************/
/* STATIC                   network_thread_start_proc                        */
/*****************************************************************************/

static kdu_thread_startproc_result KDU_THREAD_STARTPROC_CALL_CONVENTION
  network_thread_start_proc(void *param)
{
  _kd_start_client_thread_(param);
  return KDU_THREAD_STARTPROC_ZERO_RESULT;
}

/*****************************************************************************/
/*                            kdu_client::kdu_client                         */
/*****************************************************************************/

kdu_client::kdu_client()
{
  mutex.create();
  management_lock_acquired = false;
  disconnect_event.create(false); // Auto-reset events can be faster
  timer = new kdcs_timer; // Not deleted until destructor is called
  monitor = new kdcs_channel_monitor; // Not deleted until destructor is called
  monitor->synchronize_timing(*timer);
  notifier = NULL;
  context_translator = NULL;
  
  primary_connection_timeout_usecs = 3000000; // 3 seconds
  aux_connection_timeout_usecs = 5000000; // 5 seconds
  
  host_name = resource_name = target_name = sub_target_name = NULL;
  processed_target_name = NULL;
  memset(target_id,0,256);
  cache_path = NULL;
  requested_transport[0] = '\0';
  initial_connection_window_non_empty = false;
  check_for_local_cache = false;
  target_request_successful = false;
  is_stateless = true;
  active_state = false;
  non_interactive = false;
  image_done = false;
  close_requested = false;
  session_limit_reached = false;
  session_untrusted = false;
  obliterating_requests_in_flight = 0;
  final_status = "";
  total_received_bytes = 0;
  client_start_time_usecs = last_start_time_usecs = -1; active_usecs = 0;
  free_requests = NULL;
  free_dependencies = NULL;
  free_chunk_gaps = NULL;
  primary_channels = NULL;
  cids = NULL;
  request_queues = NULL;
  next_request_queue_id = 0;
  model_managers = NULL;
  next_disconnect_usecs = -1;
  have_queues_ready_to_close = false;
  max_scratch_chars = 0;
  scratch_chars = NULL;
  max_scratch_ints = 0;
  scratch_ints = NULL;
}

/*****************************************************************************/
/*                            kdu_client::~kdu_client                        */
/*****************************************************************************/

kdu_client::~kdu_client()
{
  close();
  while (primary_channels != NULL)
    release_primary_channel(primary_channels);
  while (free_requests != NULL)
    {
      kdc_request *req = free_requests;
      free_requests = req->next;
      delete req;
    }
  while (free_dependencies != NULL)
    { 
      kdc_request_dependency *dep = free_dependencies;
      free_dependencies = dep->next;
      delete dep;
    }
  while (free_chunk_gaps != NULL)
    { 
      kdc_chunk_gap *gap = free_chunk_gaps;
      free_chunk_gaps = gap->next;
      delete gap;
    }
  if (monitor != NULL)
    delete monitor;
  if (timer != NULL)
    delete timer;
  disconnect_event.destroy();
  mutex.destroy();
  if (scratch_chars != NULL)
    delete[] scratch_chars;
  if (scratch_ints != NULL)
    delete[] scratch_ints;
}

/*****************************************************************************/
/*                   kdu_client::check_compatible_url                        */
/*****************************************************************************/

const char *
  kdu_client::check_compatible_url(const char *url,
                                   bool resource_component_must_exist,
                                   const char **port_start,
                                   const char **resource_start,
                                   const char **query_start)
{
  const char *result = NULL;
  if ((url != NULL) &&
      (kdcs_has_caseless_prefix(url,"jpip://") ||
       kdcs_has_caseless_prefix(url,"http://")))
    result = url + strlen("jpip://");
  const char *resource_p = NULL;
  if ((result != NULL) && ((resource_p = strchr(result,'/')) != NULL))
    resource_p++;
  const char *query_p = NULL;
  if ((resource_p != NULL) && ((query_p = strrchr(resource_p,'?')) != NULL))
    query_p++;
  const char *port_p = NULL;
  if ((result != NULL) && (result[0] == '['))
    { // Expect a bracketed literal
      if ((port_p = strchr(result,']')) != NULL)
        port_p = strchr(port_p+1,':');
    }
  else if (result != NULL)
    port_p = strchr(result,':');
  if ((port_p != NULL) && (resource_p != NULL) && (port_p >= resource_p))
    port_p = NULL;
  if (port_start != NULL)
    *port_start = port_p;
  if (resource_start != NULL)
    *resource_start = resource_p;
  if (query_start != NULL)
    *query_start = query_p;
  if ((resource_p == NULL) && resource_component_must_exist)
    return NULL;
  return result;
}

/*****************************************************************************/
/*                       kdu_client::get_target_name                         */
/*****************************************************************************/

const char *kdu_client::get_target_name()
{
  if ((!active_state) || (resource_name == NULL))
    return "<no target>";
  if (processed_target_name == NULL)
    {
      const char *base = target_name;
      if (base == NULL)
        base = resource_name;
      int buffer_len = ((int) strlen(base)) + 1;
      if (sub_target_name == NULL)
        {
          processed_target_name = new char[buffer_len];
          strcpy(processed_target_name,base);
        }
      else
        {
          buffer_len += ((int) strlen(sub_target_name)) + 2;
          processed_target_name = new char[buffer_len];
          strcpy(processed_target_name,base);
          const char *cp = strrchr(base,'.');
          if (cp != NULL)
            sprintf(processed_target_name+(int)(cp-base),"(%s)%s",
                    sub_target_name,cp);
          else
            sprintf(processed_target_name+strlen(base),"(%s)",sub_target_name);
        }
      assert(buffer_len > (int) strlen(processed_target_name));
      kdu_hex_hex_decode(processed_target_name);
    }
  return processed_target_name;
}

/*****************************************************************************/
/*                kdu_client::install_context_translator                     */
/*****************************************************************************/

void kdu_client::install_context_translator(kdu_client_translator *translator)
{
  if (this->context_translator == translator)
    return;
  if ((this->context_translator != NULL) && active_state)
    { KDU_ERROR_DEV(e,0); e <<
      KDU_TXT("You may not install a new client context "
              "translator, over the top of an existing one, while the "
              "`kdu_client' object is active (from `connect' to `close').");
    }
  if (this->context_translator != NULL)
    this->context_translator->close();
  if (translator != NULL)
    translator->init(this);
  this->context_translator = translator;
}
      
/*****************************************************************************/
/*                               kdu_client::close                           */
/*****************************************************************************/

bool kdu_client::close()
{
  close_requested = true;
  monitor->wake_from_run();
  thread.destroy();
  kdu_cache::close();
  
  notifier = NULL;
  context_translator = NULL;
  if (host_name != NULL)
    { delete[] host_name; host_name=NULL; }
  if (resource_name != NULL)
    { delete[] resource_name; resource_name=NULL; }
  if (target_name != NULL)
    { delete[] target_name; target_name=NULL; }
  if (sub_target_name != NULL)
    { delete[] sub_target_name; sub_target_name=NULL; }
  if (processed_target_name != NULL)
    { delete[] processed_target_name; processed_target_name=NULL; }
  if (cache_path != NULL)
    { delete[] cache_path; cache_path = NULL; }
  memset(target_id,0,256);
  requested_transport[0] = '\0';
  initial_connection_window_non_empty = false;
  check_for_local_cache = false;
  target_request_successful = false;
  is_stateless = true;
  active_state = false;
  non_interactive = false;
  image_done = false;
  close_requested = false;
  session_limit_reached = false;
  obliterating_requests_in_flight = 0;
  total_received_bytes = 0;
  client_start_time_usecs = last_start_time_usecs = -1; active_usecs = 0;
  assert(cids == NULL);
  assert(request_queues == NULL);
  assert(model_managers == NULL);
  return true;
}

/*****************************************************************************/
/*                              kdu_client::connect                          */
/*****************************************************************************/

int kdu_client::connect(const char *server, const char *proxy,
                        const char *request, const char *transport,
                        const char *cache_dir, kdu_client_mode mode,
                        const char *compatible_url)
{
  kdu_client_notifier *save_notifier = this->notifier;
  close(); // Just in case
  session_untrusted = false; // Allows the flag can persist beyond `close'
  assert(model_managers == NULL);
  assert(!thread);
  assert((host_name == NULL) && (resource_name == NULL) &&
         (target_name == NULL) && (sub_target_name == NULL) &&
         (cache_path == NULL) && (target_id[0] == '\0'));
  this->notifier = save_notifier;
  
  if ((cache_dir != NULL) && (*cache_dir == '\0'))
    cache_dir = NULL;
  
  int request_queue_id = 0;
  try {
      // Start by identifying (and copying) the `server' and `request' strings
      const char *compatible_resource = NULL;
      const char *compatible_host = NULL;
      if (compatible_url != NULL)
        compatible_host = check_compatible_url(compatible_url,true,NULL,
                                               &compatible_resource);
      if (server != NULL)
        {
          host_name = new char[strlen(server)+1];
          strcpy(host_name,server);
        }
      else if (compatible_host != NULL)
        {
          int host_name_len = (int)(compatible_resource-compatible_host) - 1;
          assert(host_name_len >= 0);
          host_name = new char[host_name_len+1];
          memcpy(host_name,compatible_host,(size_t) host_name_len);
          host_name[host_name_len] = '\0';
        }
      server = host_name;
      
      if (request != NULL)
        {
          resource_name = new char[strlen(request)+1];
          strcpy(resource_name,request);
        }
      else if (compatible_resource != NULL)
        {
          resource_name = new char[strlen(compatible_resource)+1];
          strcpy(resource_name,compatible_resource);
        }
      request = resource_name;
      
      is_stateless = true; // Until a session is granted
      if ((transport==NULL) || (*transport=='\0') ||
          ((strlen(transport)==4) &&
           kdcs_has_caseless_prefix(transport,"none")))
        requested_transport[0] = '\0';
      else if ((strlen(transport)==8) &&
               kdcs_has_caseless_prefix(transport,"http-tcp"))
        strcpy(requested_transport,"http-tcp,http"); // Allow server to choose
      else if ((strlen(transport)==8) &&
               kdcs_has_caseless_prefix(transport,"http-udp"))
        strcpy(requested_transport,"http-udp,http-tcp,http");
      else if ((strlen(transport)==4) &&
               kdcs_has_caseless_prefix(transport,"http"))
        strcpy(requested_transport,"http");
      else
        { KDU_ERROR(e,35); e <<
          KDU_TXT("Unrecognized channel transport type")
          << ", \"" << transport << "\n";
        }

      bool using_proxy = false;
      const char *immediate_host = server;
      if ((proxy != NULL) && (*proxy != '\0'))
        {
          immediate_host = proxy;
          using_proxy = true;
        }
      if ((server == NULL) || (*server == '\0'))
        { KDU_ERROR_DEV(e,43); e <<
          KDU_TXT("You must supply a server name or a compatible URL in the "
                  "call to `kdu_client::connect'.");
        }
    
      // Parse the `request' string into its various components
      if ((request == NULL) || (*request == '\0') || (*request == '?'))
        { KDU_ERROR(e,0x06030901); e <<
          KDU_TXT("You must supply a non-empty resource string or a "
                  "compatible URL in the call to `kdu_client::connect'.");
        }
      char *query = NULL;
      if ((query = (char *) strrchr(resource_name,'?')) != NULL)
        {
          *query = '\0';
          query++; // Strip out fields we understand later
        }
    
      // Set up connection details for the first primary communication channel
      kdc_primary *primary =
        add_primary_channel(immediate_host,80,using_proxy);
          // Note: when address resolution occurs, we may find that the new
          // channel actually has the same connection details as an existing
          // primary channel which we can re-use (one saved from a previous
          // connection).  We will discover this later, though.
    
      // Set up the first `kdc_cid' object
      assert(cids == NULL);
      kdc_cid *cid = add_cid(primary,server,resource_name);
      
      // Set up the first request queue
      next_request_queue_id = 0;
      assert(request_queues == NULL);
      kdc_request_queue *queue = add_request_queue(cid);
      request_queue_id = queue->queue_id;
    
      // Set up the first request
      kdc_request *req = queue->add_request(-1);
      non_interactive = (mode == KDU_CLIENT_MODE_NON_INTERACTIVE);
      if (query != NULL)
        {
          bool have_non_target_fields=false;
          parse_query_string(query,req,true,have_non_target_fields);
          if (query[0] != '\0')
            req->extra_query_fields = query;
          if (have_non_target_fields && (mode == KDU_CLIENT_MODE_AUTO))
            non_interactive = true;
          initial_connection_window_non_empty = !req->window.is_empty();
        }
      req->new_elements = true;
    
      // Obtain the `processed_target_name' so we can name the cache file
      active_state = true; // Need this true before `get_target_name'
      const char *tgt_name = this->get_target_name();
    
      // Create the cache path here, up to but not including the file suffix
      if (cache_dir != NULL)
        {
          char *cp;
          cache_path = new char[strlen(cache_dir)+strlen(tgt_name)+20];
                 // Note: cache_path is allocated longer than necessary so that
                 // `look_for_compatible_cache_file' can append suffices.
          strcpy(cache_path,cache_dir);
          strcat(cache_path,"/");
          strcat(cache_path,tgt_name);
          for (cp=cache_path; *cp != '\0'; cp++)
            if ((cp[0] == '.') && (cp[1] != '.') && (cp[1] != '/') &&
                (cp[1] != '\\'))
              *cp = '_'; // Replace file suffices
          check_for_local_cache = true;
        }
    
      // Perform special adjustments for the non-interactive case
      if (non_interactive && (cache_path != NULL))
        { // Check immediately to see if it is worth sending an extra
          // request to determine the target-id -- and hence usability of
          // a local cache.
          char *suffix = cache_path + strlen(cache_path);
          strcpy(suffix,"-1.kjc"); // Examine the first candidate only here
          FILE *cache_file = fopen(cache_path,"rb");
          if (cache_file != NULL)
            { fclose(cache_file); *suffix='\0'; }
          else
            check_for_local_cache = false;
        }
      if (non_interactive)
        {
          queue->close_when_idle = true;
          primary->keep_alive = false;
          if (!check_for_local_cache)
            requested_transport[0] = '\0';
        }
        
      // Launch the network management thread
      final_status = "All network connections closed.";
      management_lock_acquired = false; // Just to be sure
      if (!thread.create(network_thread_start_proc,this))
        thread_cleanup();
    }
  catch (...)
    {
      thread_cleanup();
      throw;
    }
  return request_queue_id;
}

/*****************************************************************************/
/*                kdu_client::check_compatible_connection                    */
/*****************************************************************************/

bool
  kdu_client::check_compatible_connection(const char *server,
                                          const char *request,
                                          kdu_client_mode mode,
                                          const char *compatible_url)
{
  if (!active_state)
    return false;
  const char *compatible_resource = NULL;
  const char *compatible_host = NULL;
  if (compatible_url != NULL)
    compatible_host = check_compatible_url(compatible_url,true,NULL,
                                           &compatible_resource);
  if (server != NULL)
    {
      if (strcmp(host_name,server) != 0)
        return false;
    }
  else if (compatible_host != NULL)
    {
      int len = (int)(compatible_resource-compatible_host)-1;
      if ((len != (int) strlen(host_name)) ||
          (memcmp(host_name,compatible_host,(size_t) len) != 0))
        return false;
    }
  else
    return false;
  
  if ((request == NULL) && ((request = compatible_resource) == NULL))
    return false;
  bool intend_non_interactive = (mode == KDU_CLIENT_MODE_NON_INTERACTIVE);
  char *resource_copy = new char[strlen(request)+1];
  strcpy(resource_copy,request);
  try {
    char *query = (char *) strrchr(resource_copy,'?');
    if (query == NULL)
      query = resource_copy + strlen(resource_copy);
    else
      {
        *query = '\0';
        query++;
      }
    if (strcmp(resource_copy,resource_name) != 0)
      {
        delete[] resource_copy;
        return false;
      }
    bool have_non_target_fields=false;
    kdc_request test_req;
    test_req.init(NULL,session_untrusted);
    if (!parse_query_string(query,&test_req,false,have_non_target_fields))
      {
        delete[] resource_copy;
        return false;
      }
    if (have_non_target_fields && (mode == KDU_CLIENT_MODE_AUTO))
      intend_non_interactive = true;
    
    if (have_non_target_fields)
      {
        kdc_request *req;
        bool is_compatible;
        mutex.lock();
        is_compatible = intend_non_interactive && this->non_interactive &&
          (request_queues != NULL) &&
          ((req=request_queues->request_head) != NULL);
        if (is_compatible && req->original_window.equals(test_req.window))
          {
            if (req->extra_query_fields != NULL)
              is_compatible = (query[0] == '\0');
            else
              is_compatible = (strcmp(query,req->extra_query_fields) == 0);
          }
        mutex.unlock();
        if (!is_compatible)
          {
            delete[] resource_copy;
            return false;
          }
      }
  }
  catch (...) {
    delete[] resource_copy;
    throw;
  }
  delete[] resource_copy;
  return (intend_non_interactive == this->non_interactive);
}

/*****************************************************************************/
/*                          kdu_client::add_queue                            */
/*****************************************************************************/

int kdu_client::add_queue()
{
  int request_queue_id = -1;
  mutex.lock();
  kdc_cid *best_cid = NULL;
  if (!non_interactive)
    { // Find the best CID which is not currently closed or waiting to close
      kdc_request_queue *scan;
      for (scan=request_queues; scan != NULL; scan=scan->next)
        if (!scan->close_when_idle)
          {
            kdc_cid *cid = scan->cid;
            if ((best_cid == NULL) ||
                (cid->num_request_queues < best_cid->num_request_queues))
              best_cid = cid;
          }
    }
  if (best_cid != NULL)
    { 
      // Create the new queue
      kdc_request_queue *queue = add_request_queue(best_cid);
      request_queue_id = queue->queue_id;
      
      // Set up the first request
      kdc_request *req = queue->add_request(-1);
      req->window.init();
      req->original_window.init();
      req->new_elements = true;
    }
  mutex.unlock();
  return request_queue_id;
}

/*****************************************************************************/
/*                          kdu_client::disconnect                           */
/*****************************************************************************/

void kdu_client::disconnect(bool keep_transport_open, int timeout_milliseconds,
                            int queue_id, bool wait_for_completion)
{
  if (timeout_milliseconds < 0)
    timeout_milliseconds = 0;
  
  mutex.lock();
  
  if (non_interactive)
    {
      keep_transport_open = false; // Generally issue "connection: close"
      timeout_milliseconds = 0;
    }
  
  // Start by withdrawing "keep-alive" status from primary channels if required
  kdc_primary *keep_alive_chn = NULL;
  for (keep_alive_chn=primary_channels;
       keep_alive_chn != NULL; keep_alive_chn=keep_alive_chn->next)
    if (keep_alive_chn->keep_alive)
      break;
  if ((keep_alive_chn != NULL) && !keep_transport_open)
    {
      keep_alive_chn->keep_alive = false;
      if ((keep_alive_chn->num_http_aux_cids +
           keep_alive_chn->num_http_only_cids) == 0)
        release_primary_channel(keep_alive_chn);
      keep_alive_chn = NULL;
    }
  
  // See if we need to disconnect the OOB queue, if any.
  bool disconnect_oob_queue = true;
  kdc_request_queue *queue;
  if (queue_id >= 0)
    for (queue=request_queues; queue != NULL; queue=queue->next)
      if ((queue->queue_id >= 0) && (queue->queue_id != queue_id))
        { disconnect_oob_queue = false; break; }
  
  // Now set up the disconnection requests for all relevant channels
  bool something_to_wait_for = false;
  for (queue=request_queues; queue != NULL; queue=queue->next)
    if ((queue_id < 0) || (queue_id == queue->queue_id) ||
        (disconnect_oob_queue && (queue->queue_id < 0)))
      {
        something_to_wait_for = true;
        if (keep_transport_open && (keep_alive_chn == NULL))
          {
            keep_alive_chn = queue->cid->primary_channel;
            if (keep_alive_chn->is_persistent)
              keep_alive_chn->keep_alive = true;
            else
              keep_alive_chn = NULL;
          }
        if (!queue->close_when_idle)
          {
            queue->close_when_idle = true;
            queue->disconnect_timeout_usecs = -1;
            
            // Now remove any unsent requests from the queue
            while (queue->first_unrequested != NULL)
              queue->remove_request(queue->first_unrequested);
            if (queue->first_incomplete == NULL)
              queue->set_idle();
            
            // See if we need a final empty request for cclose
            kdc_request_queue *qp;
            for (qp=request_queues; qp != NULL; qp=qp->next)
              if ((qp->cid == queue->cid) &&
                  ((qp->first_unrequested != NULL) || !qp->close_when_idle))
                break;
            if (qp == NULL)
              { // No other queue will issue a request for the cclose field
                queue->next_posted_start_time = -1;
                kdc_request *req = queue->add_request(-1);
                assert(!queue->is_idle);
                req->preemptive = true;
                req->new_elements = false;
              }
          }
        if (queue->is_idle)
          have_queues_ready_to_close = true;
        else
          { // Set up time limit for closure
            kdu_long timeout_usecs = timer->get_ellapsed_microseconds() +
              (((kdu_long) timeout_milliseconds) * 1000);
            queue->disconnect_timeout_usecs = timeout_usecs;
            if ((this->next_disconnect_usecs < 0) ||
                (this->next_disconnect_usecs > timeout_usecs))
              this->next_disconnect_usecs = timeout_usecs;
          }
      }
  
  if (something_to_wait_for)
    monitor->wake_from_run();
  
  // Block on completion of the disconnection process, if required.
  if (wait_for_completion && something_to_wait_for)
    do {
      disconnect_event.reset();
      disconnect_event.wait(mutex);
      for (queue=request_queues; queue != NULL; queue=queue->next)
        if (((queue_id < 0) || (queue_id == queue->queue_id) ||
             (disconnect_oob_queue && (queue->queue_id < 0))) &&
            queue->close_when_idle)
          break;
    } while (queue != NULL);

  mutex.unlock();
}

/*****************************************************************************/
/*                          kdu_client::post_window                          */
/*****************************************************************************/

bool
  kdu_client::post_window(const kdu_window *window, int queue_id,
                          bool preemptive, const kdu_window_prefs *prefs,
                          kdu_long custom_id, kdu_long external_service_usecs)
{
  if (non_interactive)
    return false;
  bool window_accepted = false;
  mutex.lock();
  kdc_request_queue *queue;
  for (queue=request_queues; queue != NULL; queue=queue->next)
    if ((queue->queue_id == queue_id) && !queue->close_when_idle)
      { 
        bool prefs_changed = false;
        if (prefs != NULL)
          prefs_changed = (queue->prefs.update(*prefs) != 0);
        
        // Determine timing parameters first
        kdu_long service_time=-1, nominal_start_time=-1, current_time=-1;
        if (external_service_usecs > 0)
          { 
            if ((queue->next_posted_start_time < 0) || preemptive)
              { // Start of a new timed request sequence
                preemptive = true;
                assert(queue->cid != NULL);
                queue->cid->last_idle_time = -1; // Avoid time idle
                                                 // compensation step below
                queue->cum_external_service_usecs = 0;
                queue->cum_internal_service_usecs = 0;
                current_time = timer->get_ellapsed_microseconds();
                queue->next_posted_start_time =
                  queue->next_nominal_start_time =
                    queue->find_initial_posted_start_time(current_time);
              }
            service_time =
              convert_to_internal_timebase(external_service_usecs,
                                           queue->cum_internal_service_usecs,
                                           queue->cum_external_service_usecs,
                                           queue->sync_span_internal,
                                           queue->sync_span_external);
            nominal_start_time = queue->next_posted_start_time;
            queue->next_posted_start_time += service_time;
          }
        else
          queue->next_posted_start_time = -1;
        
        // Remove requests pre-empted by this one
        while ((queue->first_unrequested != NULL) && preemptive)
          queue->remove_request(queue->first_unrequested);

        // Figure out if the new window of interest is redundant
        if ((queue->first_incomplete != queue->request_head) &&
            queue->request_head->window_completed &&
            (!queue->request_head->untrusted) &&
            (!prefs_changed) && // A new request is required to signal prefs
            queue->request_head->window.contains(*window))
          { // Window's contents are fully served already
            if (!preemptive)
              break; // A new request can serve no purpose
            else if (queue->is_idle)
              break; // Again, a new request can serve no purpose
          }
        
        kdc_request *recent = queue->request_tail;
        if ((recent != NULL) && (recent->byte_limit == 0) &&
            (recent->posted_service_time == 0) &&
            ((queue->first_unrequested != NULL) || !prefs_changed) &&
            recent->original_window.equals(*window))
          { // This request might be redundant
            if ((recent->custom_id == custom_id) &&
                (recent->preemptive || !preemptive))
              break; // Same custom_id and not newly pre-emptive
          }
        
        // If we get here, we need to add the new WOI request
        kdc_request *req = queue->add_request(current_time);
        req->custom_id = custom_id;
        req->preemptive = preemptive;
        req->window.copy_from(*window);
        req->original_window.copy_from(*window);
        req->new_elements = ((recent == NULL) ||
                             !recent->window.contains(*window));
        req->nominal_start_time = nominal_start_time;
        req->posted_service_time = service_time;
        window_accepted = true;
        monitor->wake_from_run();
        break;
      }
  mutex.unlock();
  return window_accepted;
}

/*****************************************************************************/
/*                        kdu_client::post_oob_window                        */
/*****************************************************************************/

bool kdu_client::post_oob_window(const kdu_window *window, int caller_id,
                                 bool preemptive)
{
  if (non_interactive)
    return false;
  bool window_accepted = true; // Until proven otherwise
  mutex.lock();
  
  // Start by locating the oob request queue, if any
  kdc_request_queue *oob_queue=NULL;
  kdc_request_queue *scan;
  for (scan=request_queues; scan != NULL; scan=scan->next)
    if ((scan->queue_id < 0) && !scan->close_when_idle)
      { oob_queue = scan; break; }
  
  // Now find the best CID out of those not used by the OOB queue
  kdc_cid *best_cid = NULL;
  for (scan=request_queues; scan != NULL; scan=scan->next)
    { 
      kdc_cid *cid = scan->cid;
      if (scan->close_when_idle ||
          ((oob_queue != NULL) && (cid == oob_queue->cid)))
        continue;
      if ((best_cid == NULL) ||
          (cid->num_request_queues < best_cid->num_request_queues) ||
          ((cid->num_request_queues == best_cid->num_request_queues) &&
           (cid->last_request_time < best_cid->last_request_time)))
        best_cid = cid;
    }
  
  // Now see if we need to create an OOB queue or change its CID
  if (oob_queue == NULL)
    { // Need to create the OOB queue
      if (best_cid != NULL)
        {
          oob_queue = add_request_queue(best_cid);
          oob_queue->queue_id = -1; // Mark it as the OOB queue
          next_request_queue_id--; // Adjust to compensate
        }
      else
        { // All CID's are closed and so we should not really be here  
          mutex.unlock();
          return false;
        }
    }
  else if (!oob_queue->have_active_requests() && (best_cid != NULL))
    { // We may be in a position to change the CID of the OOB queue
      kdc_cid *oob_cid = oob_queue->cid;
      if ((oob_cid->num_request_queues > best_cid->num_request_queues) &&
          ((oob_cid->num_request_queues > (best_cid->num_request_queues+1)) ||
           (oob_cid->last_request_time > best_cid->last_request_time)))
        { // Move to `best_cid'
          assert(!is_stateless);
          assert(oob_queue != oob_cid->primary_channel->active_requester);
          oob_cid->num_request_queues--;
          best_cid->num_request_queues++;
          oob_queue->cid = best_cid;
          oob_cid->adjust_timing_after_queue_removed();
          oob_queue->next_nominal_start_time = best_cid->last_target_end_time;
        }
    }
  if ((oob_queue->first_incomplete == NULL) &&
      (oob_queue->cid->num_request_queues > 1))
    { // Request a new JPIP channel, in case the server is now willing to offer
      // one to us.
      oob_queue->just_started = true; // Stricly speaking, we might not have
         // only just started this queue, but we can pretend as if we have, so
         // that a "cnew" request will be issued.  There is no problem with
         // this, because we have no outstanding requests at present.
    }
  
  // Now add a request to the `oob_queue' unless this is not required
  kdc_request *req;
  bool issue_as_preemptive = preemptive;
  for (req=oob_queue->request_head;
       window_accepted && (req != oob_queue->first_unrequested);
       req = req->next)
    if (req->window.contains(*window) && !req->untrusted)
      window_accepted = false;
    else if ((req->oob_caller_id != caller_id) &&
             !req->communication_complete())
      issue_as_preemptive = false; // Cannot preempt another caller
  kdc_request *oob_req = NULL;
  if (preemptive && window_accepted)
    { 
      kdc_request *next_req;
      for (; req != NULL; req=next_req)
        { 
          next_req = req->next;
          if (req->oob_caller_id != caller_id)
            issue_as_preemptive = false; // Cannot preempt another caller
          else if (oob_req == NULL)
            oob_req = req; // Replace this request
          else
            oob_queue->remove_request(req);
        }
    }
  if (window_accepted)
    { 
      if (oob_req == NULL)
        oob_req = oob_queue->add_request(-1);
      oob_req->window.copy_from(*window);
      oob_req->original_window.copy_from(*window);
      oob_req->oob_caller_id = caller_id;
      oob_req->preemptive = issue_as_preemptive;
      oob_req->new_elements=true; // Always mark OOB this way for simplicity
      monitor->wake_from_run();
    }
      
  mutex.unlock();
  return window_accepted;
}

/*****************************************************************************/
/*                         kdu_client::sync_timing                           */
/*****************************************************************************/

kdu_long
  kdu_client::sync_timing(int queue_id, kdu_long app_usecs,
                          bool expect_preemptive)
{
  kdu_long result = -1000000000; // In case the queue is not found
  mutex.lock();
  kdu_long cur_time = timer->get_ellapsed_microseconds();
  kdc_request_queue *queue;
  kdc_cid *cid;
  for (queue=request_queues; queue != NULL; queue=queue->next)
    if ((queue_id == queue->queue_id) && ((cid = queue->cid) != NULL))
      { 
        // Start by updating the clock domain synchronization machinery
        if ((queue->next_posted_start_time < 0) ||
            (queue->sync_span_internal < 0) ||
            (queue->sync_span_external < 0) ||
            (app_usecs <
             (queue->sync_span_external+queue->sync_base_external)))
          { 
            queue->sync_base_external = app_usecs;
            queue->sync_base_internal = cur_time;
            queue->sync_span_external = queue->sync_span_internal = 0;
          }
        else
          { 
            queue->sync_span_external = app_usecs - queue->sync_base_external;
            queue->sync_span_internal = cur_time - queue->sync_base_internal;
          }
        
        // Now compute the return value
        kdu_long next_posted_start_time = queue->next_posted_start_time;
        if ((next_posted_start_time < 0) || expect_preemptive)
          next_posted_start_time =
            queue->find_initial_posted_start_time(cur_time);
        kdu_long next_nominal_start_time = queue->next_nominal_start_time;
        if (next_nominal_start_time < 0)
          next_nominal_start_time = next_posted_start_time;
        else if (cid->last_idle_time >= 0)
          { // Factor in lost service time that will be used to advance the
            // next nominal start times of all queues before issuing another
            // request.
            kdu_long lost_service_time = cur_time - cid->last_idle_time;
            lost_service_time += cid->request_rtt;
            next_nominal_start_time += lost_service_time;
          }
        if (next_nominal_start_time > next_posted_start_time)
          next_posted_start_time = next_nominal_start_time;
        result = next_posted_start_time - cur_time;
        if ((cid->last_target_end_time > 0) &&
            !cid->waiting_to_sync_nominal_request_timing)
          { // Add in any disparity between nominal and actual timing that
            // has not been compensated for already.
            kdu_long disparity = cid->target_end_time_disparity;
            disparity -= cid->outstanding_disparity_compensation;
            if (disparity > 0)
              result += disparity;
          }
        break;
      }
  mutex.unlock();
  return result;
}

/*****************************************************************************/
/*                   kdu_client::get_timed_request_horizon                   */
/*****************************************************************************/

kdu_long
  kdu_client::get_timed_request_horizon(int queue_id, bool expect_preemptive)
{
  kdu_long result = -100000000; // -100 seconds in case no matching live queue
  mutex.lock();
  kdc_request_queue *queue;
  kdc_cid *cid;
  for (queue=request_queues; queue != NULL; queue=queue->next)
    if ((queue_id == queue->queue_id) && ((cid = queue->cid) != NULL))
      { 
        int num_outstanding_bytes = 0;
        if (!expect_preemptive)
          num_outstanding_bytes = cid->calculate_num_outstanding_bytes();
        int horizon_bytes =
          cid->flow_regulator.get_request_byte_limit(num_outstanding_bytes,
                                                     false);
        result = cid->flow_regulator.estimate_usecs_for_bytes(horizon_bytes);
        if (cid->last_target_end_time >= 0)
          { // Adjust for pending requests for service time
            assert(queue->next_nominal_start_time >= 0);
            kdu_long pending_service_usecs = 0;
            if ((queue->next_posted_start_time >= 0) && !expect_preemptive)
              pending_service_usecs += (queue->next_posted_start_time -
                                        cid->last_target_end_time);
            else
              pending_service_usecs += (queue->next_nominal_start_time -
                                        cid->last_target_end_time);
            if (cid->last_idle_time >= 0)
              { // `cid->last_target_end_time' will advance when a request is
                // actually posted.
                kdu_long current_time = timer->get_ellapsed_microseconds();
                kdu_long estimated_idle_time = // See `kdc_cid::wake_from_idle'
                  (current_time - cid->last_idle_time) + cid->request_rtt;
                pending_service_usecs -= estimated_idle_time;
              }
            if (pending_service_usecs > 0)
              result -= pending_service_usecs;            
          }
        break;
      }
  mutex.unlock();
  return result;
}

/*****************************************************************************/
/*                      kdu_client::trim_timed_requests                      */
/*****************************************************************************/

kdu_long
  kdu_client::trim_timed_requests(int queue_id, kdu_long &custom_id,
                                  bool &partially_sent)
{
  kdu_long result = -1;
  mutex.lock();
  kdc_request_queue *queue;
  for (queue=request_queues; queue != NULL; queue=queue->next)
    if (queue_id == queue->queue_id)
      { 
        result = 0;
        kdc_request *req;
        if ((queue->next_posted_start_time >= 0) &&
            ((req=queue->first_unrequested) != NULL) &&
            (req->posted_service_time > 0))
          { 
            kdu_long internal_service_usecs = 0;
            custom_id = req->custom_id;
            partially_sent = (req->copy_src != NULL);
            while ((req=queue->first_unrequested) != NULL)
              { 
                internal_service_usecs += req->posted_service_time;
                queue->remove_request(req);
              }
            if (internal_service_usecs > queue->cum_internal_service_usecs)
              internal_service_usecs = queue->cum_internal_service_usecs;
            if (internal_service_usecs > queue->next_posted_start_time)
              internal_service_usecs = queue->next_posted_start_time;
            queue->next_posted_start_time -= internal_service_usecs;
            result =
              convert_to_external_timebase(internal_service_usecs,
                                           queue->cum_internal_service_usecs,
                                           queue->cum_external_service_usecs,
                                           queue->sync_span_internal,
                                           queue->sync_span_external);
              /* Note: above function does the reverse of the
                 `convert_to_internal_timebase' function that is called by
                 `post_window'.  The above function subtracts the internal
                 and converted external service times from the `cum_...'
                 values, rather than adding them. */
            if (queue->cid->last_target_end_time < 0)
              queue->next_posted_start_time = -1;
          }
        break;
      }
  mutex.unlock();
  return result;
}

/*****************************************************************************/
/*                     kdu_client::get_window_in_progress                    */
/*****************************************************************************/

bool
  kdu_client::get_window_in_progress(kdu_window *window, int queue_id,
                                     int *status_flags, kdu_long *custom_id)
{
  bool result = false;
  if (status_flags != NULL)
    *status_flags = 0;
  mutex.lock();
  kdc_request_queue *queue;
  for (queue=request_queues; queue != NULL; queue=queue->next)
    if (queue_id == queue->queue_id)
      { 
        kdc_request *req = queue->request_head; 
        if (req != NULL)
          {
            while ((req->next != NULL) && req->next->reply_received)
              req = req->next;
          }
        if ((req != NULL) && req->reply_received)
          {
            if (window != NULL)
              window->copy_from(req->window,true);
            if (custom_id != NULL)
              *custom_id = req->custom_id;
            kdc_request *test;
            for (test=req->next; test != NULL; test=test->next)
              if (!test->is_copy)
                break; // Found a more recent request that is not a copy
            result = (test == NULL);
            if (status_flags != NULL)
              { // Return status flags
                if (result)
                  *status_flags |= KDU_CLIENT_WINDOW_IS_MOST_RECENT;
                if (req->chunk_received)
                  *status_flags |= KDU_CLIENT_WINDOW_RESPONSE_STARTED;
                if (result && req->response_terminated)
                  *status_flags |= KDU_CLIENT_WINDOW_RESPONSE_TERMINATED;
                if (req->window_completed && req->is_complete() &&
                    !req->untrusted)
                  *status_flags |= KDU_CLIENT_WINDOW_IS_COMPLETE;
              }
          }
        else if (window != NULL)
          window->init();
        break;
      }
  mutex.unlock();
  return result;
}

/*****************************************************************************/
/*                        kdu_client::get_window_info                        */
/*****************************************************************************/

bool
  kdu_client::get_window_info(int queue_id, int &status_flags,
                              kdu_long &custom_id, kdu_window *window,
                              kdu_long *service_usecs)
{
  bool result = false;
  mutex.lock();
  kdc_request_queue *queue;
  for (queue=request_queues; queue != NULL; queue=queue->next)
    if (queue_id == queue->queue_id)
      { 
        kdc_request *best_req=NULL, *req=queue->request_head;
        bool best_requested=false, best_replied=false, best_has_chunk=false;
        for (; req != NULL; req=req->next)
          { 
            if (req->copy_src != NULL)
              continue; // We have already discounted the copy as returnable
            if (status_flags >= 0)
              { // Need to look for individual flags
                bool is_match = false;
                if (status_flags & KDU_CLIENT_WINDOW_IS_MOST_RECENT)
                  is_match = true;
                else if ((status_flags & KDU_CLIENT_WINDOW_UNREQUESTED) &&
                         (req->request_issue_time < 0))
                  is_match = true;
                else if ((status_flags & KDU_CLIENT_WINDOW_UNREPLIED) &&
                         !req->reply_received)
                  is_match = true;
                else if ((status_flags & KDU_CLIENT_WINDOW_RESPONSE_STARTED) &&
                         req->chunk_received)
                  is_match = true;
                else if ((status_flags &
                          KDU_CLIENT_WINDOW_RESPONSE_TERMINATED) &&
                         req->response_terminated)
                  is_match = true;
                else if (status_flags & KDU_CLIENT_WINDOW_IS_COMPLETE)
                  is_match = (req->window_completed && req->is_complete() &&
                              !req->untrusted);
                if (!is_match)
                  continue;
              }
            else if (custom_id != req->custom_id)
              continue;
            best_requested = (req->request_issue_time >= 0);
            best_replied = req->reply_received;
            best_has_chunk = req->chunk_received;
            while ((req->next_copy != NULL) &&
                   (req->next_copy->chunk_received ||
                    (req->received_service_time > 0)))
              { 
                assert(req->next_copy->copy_src == req);
                req = req->next_copy;
              }
            best_req = req;
          }

        if ((req=best_req) != NULL)
          { // We have a result
            result = true;
            status_flags = 0;
            if (!best_requested)
              status_flags |= KDU_CLIENT_WINDOW_UNREQUESTED;
            if (!best_replied)
              status_flags |= KDU_CLIENT_WINDOW_UNREPLIED;
            if (best_has_chunk)
              status_flags |= KDU_CLIENT_WINDOW_RESPONSE_STARTED;
            if (req->response_terminated)
              status_flags |= KDU_CLIENT_WINDOW_RESPONSE_TERMINATED;
            if (req->window_completed && req->is_complete() &&
                !req->untrusted)
              status_flags |= KDU_CLIENT_WINDOW_IS_COMPLETE;
            custom_id = req->custom_id;
            if (window != NULL)
              window->copy_from(req->window,true);
            if (service_usecs != NULL)
              *service_usecs = req->received_service_time;
            for (; req != NULL; req=req->next)
              if (!req->is_copy)
                break; // Found a more recent request that is not a copy
            if (req == NULL)
              status_flags |= KDU_CLIENT_WINDOW_IS_MOST_RECENT;
          }
        break;
      }
  mutex.unlock();
  return result;
}

/*****************************************************************************/
/*                   kdu_client::get_oob_window_in_progress                  */
/*****************************************************************************/

bool kdu_client::get_oob_window_in_progress(kdu_window *window, int caller_id,
                                            int *status_flags)
{
  bool result = false;
  if (status_flags != NULL)
    *status_flags = 0;
  mutex.lock();
  kdc_request_queue *queue;
  for (queue=request_queues; queue != NULL; queue=queue->next)
    if (queue->queue_id < 0)
      { // Found the (or possible a) OOB request queue
        kdc_request *req=NULL, *test=queue->request_head;
        for (; (test != NULL) && test->reply_received; test=test->next)
          if (test->oob_caller_id == caller_id)
            req = test;
        if (req != NULL)
          { 
            if (window != NULL)
              window->copy_from(req->window,true);
            result = true;
            kdc_request *test;
            for (test=req->next; test != NULL; test=test->next)
              if ((test->oob_caller_id == caller_id) && !test->is_copy)
                break; // Found more recent request that is not replied
            result = (test == NULL);
            if (status_flags != NULL)
              { // Return status flags
                if (result)
                  *status_flags |= KDU_CLIENT_WINDOW_IS_MOST_RECENT;
                if (result && req->response_terminated)
                  *status_flags |= KDU_CLIENT_WINDOW_RESPONSE_TERMINATED;
                if (req->window_completed && req->is_complete() &&
                    !req->untrusted)
                  *status_flags |= KDU_CLIENT_WINDOW_IS_COMPLETE;
              }
          }
        else if (window != NULL)
          window->init();
        break;
      }
  mutex.unlock();
  return result;
}

/*****************************************************************************/
/*                            kdu_client::is_alive                           */
/*****************************************************************************/

bool kdu_client::is_alive(int queue_id)
{
  bool result = false;
  mutex.lock();
  kdc_request_queue *queue;
  for (queue=request_queues; queue != NULL; queue=queue->next)
    if ((queue_id < 0) || (queue_id == queue->queue_id))
      {
        result = true;
        break;
      }
  mutex.unlock();
  return result;
}

/*****************************************************************************/
/*                            kdu_client::is_idle                            */
/*****************************************************************************/

bool kdu_client::is_idle(int queue_id)
{
  bool found_idle = false;
  bool found_non_idle = false;
  mutex.lock();
  kdc_request_queue *queue;
  for (queue=request_queues; queue != NULL; queue=queue->next)
    if ((queue_id < 0) || (queue_id == queue->queue_id))
      {
        if (queue->is_idle)
          found_idle = true;
        else
          { found_non_idle = true; break; }
      }
  mutex.unlock();
  return (found_idle && !found_non_idle);
}

/*****************************************************************************/
/*                           kdu_client::get_status                          */
/*****************************************************************************/

const char *kdu_client::get_status(int queue_id)
{
  mutex.lock();
  const char *result = final_status;
  if (request_queues != NULL)
    {
      result = "Request queue not connected.";
      kdc_request_queue *queue;
      for (queue=request_queues; queue != NULL; queue=queue->next)
        if (queue->queue_id == queue_id)
          {
            result = queue->status_string;
            break;
          }
    }
  mutex.unlock();
  return result;
}

/*****************************************************************************/
/*                        kdu_client::get_timing_info                        */
/*****************************************************************************/

bool kdu_client::get_timing_info(int queue_id, double *request_rtt,
                                 double *suggested_min_posting_interval)
{
  mutex.lock();
  bool queue_found = false;
  kdc_request_queue *queue;
  for (queue=request_queues; queue != NULL; queue=queue->next)
    if ((queue->queue_id == queue_id) ||
        ((queue_id < 0) && (queue->queue_id < 0)))
      { // We have a match with the queue
        kdc_cid *cid = queue->cid;
        if (request_rtt != NULL)
          { 
            *request_rtt = -1.0;
            if ((cid != NULL) && (cid->request_rtt >= 0))
              *request_rtt = 0.000001 * (double)(cid->request_rtt);
          }
        if (suggested_min_posting_interval != NULL)
          { 
            double val = 0.05; // Not a disciplined choice in the stateful case
            if (is_stateless)
              { 
                if (cid->request_rtt <= 0)
                  val = 1.0;
                else
                  { 
                    val = 0.000004 * (double)(cid->request_rtt); // 4 x RTT
                    double min_val = val * 0.5; // 2 x RTT
                    if (val > 1.0)
                      val = 1.0; // Prefer not to go above 1 second
                    if (val < min_val)
                      val = min_val; // Need at least twice RTT
                  }
                *suggested_min_posting_interval = val;
              }
          }
        queue_found = true;
        break;
      }
  mutex.unlock();
  return queue_found;
}

/*****************************************************************************/
/*                        kdu_client::get_received_bytes                     */
/*****************************************************************************/

kdu_long kdu_client::get_received_bytes(int queue_id,
                                        double *non_idle_seconds,
                                        double *seconds_since_first_active)
{
  kdu_long result = 0;
  mutex.lock();
  kdu_long cur_time = -1;
  
  if ((non_idle_seconds != NULL) ||
      (seconds_since_first_active != NULL))
    {
      cur_time = timer->get_ellapsed_microseconds();
      if (non_idle_seconds != NULL)
        *non_idle_seconds = 0.0;
      if (seconds_since_first_active != NULL)
        *seconds_since_first_active = 0.0;
    }
  if (queue_id < 0)
    {
      result = total_received_bytes;
      if ((seconds_since_first_active != NULL) &&
          (client_start_time_usecs >= 0))
        *seconds_since_first_active = 1.0E-6*(cur_time-client_start_time_usecs);
      if (non_idle_seconds != NULL)
        {
          kdu_long active_time = active_usecs;
          if (last_start_time_usecs >= 0)
            active_time += (cur_time - last_start_time_usecs);
          *non_idle_seconds = 1.0E-6 * active_time;
        }
    }
  else
    {
      kdc_request_queue *queue;
      for (queue=request_queues; queue != NULL; queue=queue->next)
        if (queue->queue_id == queue_id)
          {
            result = queue->received_bytes;
            if ((seconds_since_first_active != NULL) &&
                (queue->queue_start_time_usecs >= 0))
              *seconds_since_first_active =
                1.0E-6*(cur_time-queue->queue_start_time_usecs);
            if (non_idle_seconds != NULL)
              {
                kdu_long active_time = queue->active_usecs;
                if (queue->last_start_time_usecs >= 0)
                  active_time += (cur_time - queue->last_start_time_usecs);
                *non_idle_seconds = 1.0E-6 * active_time;
              }
            break;
          }
    }
  mutex.unlock();
  return result;
}

/*****************************************************************************/
/* PRIVATE                kdu_client::add_primary_channel                    */
/*****************************************************************************/

kdc_primary *kdu_client::add_primary_channel(const char *host,
                                             kdu_uint16 default_port,
                                             bool using_proxy)
{
  kdc_primary *chn = new kdc_primary(this);
  chn->next = primary_channels;
  primary_channels = chn;
  
  // Extract immediate server name and port
  chn->using_proxy = using_proxy;
  chn->immediate_server = make_new_string(host);
  chn->immediate_port = default_port;
  check_and_extract_port_suffix(chn->immediate_server,chn->immediate_port);
  return chn;
}

/*****************************************************************************/
/* PRIVATE             kdu_client::release_primary_channel                   */
/*****************************************************************************/

void kdu_client::release_primary_channel(kdc_primary *chn)
{
  if (chn->is_released)
    return; // Prevent recursive entry to this function
  chn->is_released = true;
  while (chn->first_active_request != NULL)
    chn->remove_active_request(chn->first_active_request);
  while ((chn->num_http_aux_cids+chn->num_http_only_cids) > 0)
    {
      kdc_cid *cid;
      for (cid=cids; cid != NULL; cid=cid->next)
        if (cid->primary_channel == chn)
          break;
      if (cid == NULL)
        { assert(0); break; }
      release_cid(cid);
    }
  
  kdc_primary *scan, *prev=NULL;
  for (scan=primary_channels; scan != NULL; prev=scan, scan=scan->next)
    if (scan == chn)
      {
        if (prev == NULL)
          primary_channels = chn->next;
        else
          prev->next = chn->next;
        break;
      }
  if (chn->channel != NULL)
    {
      chn->channel_connected = false;
      chn->channel->close();
      delete chn->channel;
      chn->channel = NULL;
    }
  chn->release();
}

/*****************************************************************************/
/* PRIVATE                     kdu_client::add_cid                           */
/*****************************************************************************/

kdc_cid *kdu_client::add_cid(kdc_primary *primary, const char *server_name,
                             const char *resource_name)
{
  assert((server_name != NULL) && (resource_name != NULL));
  
  kdc_cid *obj = new kdc_cid(this);  
  obj->next = cids;
  cids = obj;
  obj->resource = make_new_string(resource_name);
  obj->server = make_new_string(server_name);
  
  // Extract server name and port (if possible)
  obj->request_port = 80;
  check_and_extract_port_suffix(obj->server,obj->request_port);
  obj->return_port = obj->request_port; // In case we later create aux channel  
  obj->primary_channel = primary;
  primary->num_http_only_cids++; // Stateless counts as HTTP-only.  We may
                                 // change the counts when we have a channel-id
  
  // Assign maximum receive rate, if required
  obj->aux_min_usecs_per_byte = 0; // 100.0; // 10kB/s
  
  // Assign loss probability, if required
  obj->aux_per_byte_loss_probability = 0; // 0.00001;
  
  obj->flow_regulator.set_disjoint_requests(this->is_stateless); // May change
                      // this later if stateful communications are found to be
                      // supported.
  return obj;
}

/*****************************************************************************/
/* PRIVATE                  kdu_client::release_cid                          */
/*****************************************************************************/

void kdu_client::release_cid(kdc_cid *obj)
{
  if (obj->is_released)
    return; // Prevent recursive entry to this function
  obj->is_released = true;
  bool removed_primary_active_request = false;
  while (obj->first_active_receiver != NULL)
    { 
      if (obj->first_active_receiver->is_primary_active_request)
        removed_primary_active_request = true;
      obj->remove_active_receiver(obj->first_active_receiver);
    }
  while (obj->num_request_queues > 0)
    {
      kdc_request_queue *queue;
      for (queue=request_queues; queue != NULL; queue=queue->next)
        if (queue->cid == obj)
          break;
      if (queue == NULL)
        { assert(0); break; }
      release_request_queue(queue);
    }
  assert((obj->last_requester == NULL) &&
         (obj->first_active_receiver == NULL) &&
         (obj->last_active_receiver == NULL));
  
  kdc_cid *scan, *prev;
  for (prev=NULL, scan=cids; scan != NULL; prev=scan, scan=scan->next)
    if (scan == obj)
      {
        if (prev == NULL)
          cids = obj->next;
        else
          prev->next = obj->next;
        break;
      }

  if (obj->aux_tcp_channel != NULL)
    {
      obj->aux_tcp_channel->close();
      delete obj->aux_tcp_channel;
      obj->aux_tcp_channel = NULL;
      obj->aux_channel_connected = false;
    }
  if (obj->aux_udp_channel != NULL)
    { 
      obj->aux_udp_channel->close();
      delete obj->aux_udp_channel;
      obj->aux_udp_channel = NULL;
      obj->aux_channel_connected = false;
    }
  
  kdc_primary *primary = obj->primary_channel;
  obj->primary_channel = NULL;
  if (primary != NULL)
    {
      if (obj->uses_aux_channel)
        primary->num_http_aux_cids--;
      else
        primary->num_http_only_cids--;
      if (removed_primary_active_request)
        { // Closing CID while it is still actively receiving data or HTTP
          // responses from the primary channel, or in the middle of sending
          // a request.  Either way, this means that the primary channel must
          // be closed down immediately (graceful closure not possible).
          release_primary_channel(primary);
        }
      else if ((primary->num_http_aux_cids+primary->num_http_only_cids) == 0)
        { // We no longer need the primary channel, but perhaps we should
          // preserve it for future use.
          if ((primary->channel == NULL) || (!primary->channel_connected) ||
              !(primary->keep_alive && primary->is_persistent))
            release_primary_channel(primary);
        }
    }
  obj->release();
}

/*****************************************************************************/
/* PRIVATE                  kdu_client::alloc_request                        */
/*****************************************************************************/

kdc_request *kdu_client::alloc_request()
{
  kdc_request *qp = free_requests;
  if (qp == NULL)
    qp = new kdc_request;
  else
    free_requests = qp->next;
  return qp;
}

/*****************************************************************************/
/* PRIVATE                 kdu_client::recycle_request                       */
/*****************************************************************************/

void kdu_client::recycle_request(kdc_request *qp)
{
  if (qp->dependencies != NULL)
    { recycle_dependencies(qp->dependencies); qp->dependencies = NULL; }
  if (qp->chunk_gaps != NULL)
    { recycle_chunk_gaps(qp->chunk_gaps); qp->chunk_gaps = NULL; }
  qp->next = free_requests;
  free_requests = qp;
  assert((qp->next_copy == NULL) && // make sure there are no connections
         (qp->copy_src == NULL));   // accidentally left behind.
}

/*****************************************************************************/
/* PRIVATE                 kdu_client::alloc_dependency                      */
/*****************************************************************************/

kdc_request_dependency *kdu_client::alloc_dependency()
{
  kdc_request_dependency *dep = free_dependencies;
  if (dep == NULL)
    dep = new kdc_request_dependency;
  else
    free_dependencies = dep->next;
  return dep;
}

/*****************************************************************************/
/* PRIVATE              kdu_client::recycle_dependencies                     */
/*****************************************************************************/

void kdu_client::recycle_dependencies(kdc_request_dependency *list)
{ 
  kdc_request_dependency *tail = list;
  while (tail->next != NULL)
    tail = tail->next;
  tail->next = free_dependencies;
  free_dependencies = list;
}

/*****************************************************************************/
/* PRIVATE                 kdu_client::alloc_chunk_gap                       */
/*****************************************************************************/

kdc_chunk_gap *kdu_client::alloc_chunk_gap()
{
  kdc_chunk_gap *gap = free_chunk_gaps;
  if (gap == NULL)
    gap = new kdc_chunk_gap;
  else
    free_chunk_gaps = gap->next;
  return gap;
}

/*****************************************************************************/
/* PRIVATE               kdu_client::recycle_chunk_gaps                      */
/*****************************************************************************/

void kdu_client::recycle_chunk_gaps(kdc_chunk_gap *list)
{ 
  kdc_chunk_gap *tail = list;
  while (tail->next != NULL)
    tail = tail->next;
  tail->next = free_chunk_gaps;
  free_chunk_gaps = list;
}

/*****************************************************************************/
/* PRIVATE                 kdu_client::add_request_queue                     */
/*****************************************************************************/

kdc_request_queue *kdu_client::add_request_queue(kdc_cid *cid)
{
  kdc_request_queue *queue = new kdc_request_queue(this);
  queue->next = request_queues;
  request_queues = queue;
  queue->cid = cid;
  queue->queue_id = next_request_queue_id++;
  if (next_request_queue_id < 0)
    next_request_queue_id = 1;
  cid->num_request_queues++;
  if (cid->last_target_end_time >= 0)
    queue->next_nominal_start_time = cid->last_target_end_time;
  return queue;
}

/*****************************************************************************/
/* PRIVATE               kdu_client::release_request_queue                   */
/*****************************************************************************/

void kdu_client::release_request_queue(kdc_request_queue *queue)
{
  signal_status();
  kdc_request_queue *scan, *prev=NULL;
  for (scan=request_queues; scan != NULL; prev=scan, scan=scan->next)
    if (scan == queue)
      {
        if (prev == NULL)
          request_queues = queue->next;
        else
          prev->next = queue->next;
        break;
      }

  bool check_communication_complete = true;
  bool session_should_not_be_trusted = false;
  while (queue->request_head != NULL)
    { 
      if (queue->request_head == queue->first_unrequested)
        check_communication_complete = false;
      else if (check_communication_complete &&
               !queue->request_head->communication_complete())
        { // Removing a request which has been issued, but whose response is
          // not yet complete.  This is potentially dangerous, since the
          // server may be assuming we have cached response data which we will
          // never receive.  The implementation should do everything it can to
          // avoid this sort of thing happening, but if we get to this point,
          // we must at least flag the fact that all future communication may
          // leave us with incomplete cache contents, regardless of what EOR
          // messages may be provided by the server.
          session_should_not_be_trusted = true;
        }
      queue->remove_request(queue->request_head);
    }

  kdc_cid *cid = queue->cid;
  queue->cid = NULL;
  if (cid != NULL)
    { 
      if ((cid->primary_channel != NULL) &&
          (cid->primary_channel->active_requester == queue))
        cid->primary_channel->active_requester = NULL;
      cid->num_request_queues--;
      if (cid->last_requester == queue)
        cid->last_requester = NULL; // So we don't reference it accidentally
      if (cid->num_request_queues == 0)
        release_cid(cid);
      else if (cid->last_target_end_time >= 0)
        cid->adjust_timing_after_queue_removed();
    }
  delete queue;
  queue = NULL;
  
  if (session_should_not_be_trusted && !this->session_untrusted)
    { // Mark all current requests as untrusted; from now on, all newly
      // generated requests will immediately be marked as untrusted.
      this->session_untrusted = false;
      for (queue = request_queues; queue != NULL; queue=queue->next)
        { 
          kdc_request *req;
          for (req=queue->first_incomplete; req != NULL; req=req->next)
            req->untrusted = true;
        }
    }
  
  disconnect_event.set();
}

/*****************************************************************************/
/* PRIVATE                kdu_client::parse_query_string                     */
/*****************************************************************************/

bool kdu_client::parse_query_string(char *query, kdc_request *req,
                                    bool create_target_strings,
                                    bool &contains_non_target_fields)
{
  assert((!create_target_strings) || !thread);
  contains_non_target_fields = false;
  bool all_fields_understood_and_compatible = true;
  kdu_window *window = NULL;
  if (req != NULL)
    window = &(req->window);
  
  const char *scan, *qp, *field_sep;
  bool have_size = false;
  bool have_target_field = false;
  bool have_sub_target_field = false;
  for (qp=query, field_sep=NULL; qp != NULL; qp=field_sep)
    {
      if (field_sep != NULL)
        qp++; // Skip over field separator
      if (*qp == '\0')
        break;
      field_sep = strchr(qp,'&');
      const char *field_start = qp;
      if (kdcs_parse_request_field(qp,JPIP_FIELD_TARGET))
        {
          have_target_field = true;
          int len = 0;
          while ((qp[len] != '&') && (qp[len] != '\0'))
            len++;
          if (create_target_strings)
            {
              assert(target_name == NULL);
              target_name = new char[len+1];
              memcpy(target_name,qp,(size_t) len);
              target_name[len] = '\0';
            }
          else if ((target_name == NULL) ||
                   (len != (int) strlen(target_name)) ||
                   (memcmp(target_name,qp,(size_t) len) != 0))
            all_fields_understood_and_compatible = false;
        }
      else if (kdcs_parse_request_field(qp,JPIP_FIELD_SUB_TARGET))
        {
          have_sub_target_field = true;
          int len = 0;
          while ((qp[len] != '&') && (qp[len] != '\0'))
            len++;
          if (create_target_strings)
            {
              assert(sub_target_name == NULL);
              sub_target_name = new char[len+1];
              memcpy(sub_target_name,qp,(size_t) len);
              sub_target_name[len] = '\0';
            }
          else if ((sub_target_name == NULL) ||
                   (len != (int) strlen(sub_target_name)) ||
                   (memcmp(sub_target_name,qp,(size_t) len) != 0))
            all_fields_understood_and_compatible = false;
        }
      else if (kdcs_parse_request_field(qp,JPIP_FIELD_FULL_SIZE))
        {
          contains_non_target_fields = true;
          int val1, val2;
          if ((sscanf(qp,"%d,%d",&val1,&val2) != 2) ||
              (val1 <= 0) || (val2 <= 0))
            { KDU_ERROR(e,2); e <<
              KDU_TXT("Malformed ")
              << "\"" JPIP_FIELD_FULL_SIZE "\" " <<
              KDU_TXT("field in query component of "
                      "requested URL; query string is:\n\n") << query;
            }
          qp = strchr(qp,','); assert(qp != NULL);
          for (qp++; *qp != '\0'; qp++)
            if ((*qp == '&') || (*qp == ','))
              break;
          int round_direction = -1; // Default is round-down
          if (kdcs_has_caseless_prefix(qp,",round-up"))
            round_direction = 1;
          else if (kdcs_has_caseless_prefix(qp,",closest"))
            round_direction = 0;
          else if (kdcs_has_caseless_prefix(qp,",round-down"))
            round_direction = -1;
          else if ((*qp != '\0') && (*qp != '&'))
            { KDU_ERROR(e,3); e <<
              KDU_TXT("Malformed ")
              << "\"" JPIP_FIELD_FULL_SIZE "\" " <<
              KDU_TXT("field in query component of "
                      "requested URL; query string is:\n\n") << query;
            }
          if (window != NULL)
            {
              window->resolution.x = val1;
              window->resolution.y = val2;
              if (!have_size)
                window->region.size = window->resolution;
              window->round_direction = round_direction;
            }
        }
      else if (kdcs_parse_request_field(qp,JPIP_FIELD_REGION_OFFSET))
        {
          contains_non_target_fields = true;
          int val1, val2;
          if ((sscanf(qp,"%d,%d",&val1,&val2) != 2) || (val1 < 0) || (val2 < 0))
            { KDU_ERROR(e,4); e <<
              KDU_TXT("Malformed ")
              << "\"" JPIP_FIELD_REGION_OFFSET "\" " <<
              KDU_TXT("field in query component of "
                      "requested URL; query string is:\n\n") << query;
            }
          if (window != NULL)
            {
              window->region.pos.x = val1;
              window->region.pos.y = val2;
            }
        }
      else if (kdcs_parse_request_field(qp,JPIP_FIELD_REGION_SIZE))
        {
          contains_non_target_fields = true;
          have_size = true;
          int val1, val2;
          if ((sscanf(qp,"%d,%d",&val1,&val2) != 2) ||
              (val1 <= 0) || (val2 <= 0))
            { KDU_ERROR(e,5); e <<
              KDU_TXT("Malformed ")
              << "\"" JPIP_FIELD_REGION_SIZE "\" " <<
              KDU_TXT("field in query component of "
                      "requested URL; query string is:\n\n") << query;
            }
          if (window != NULL)
            {
              window->region.size.x = val1;
              window->region.size.y = val2;
            }
        }
      else if (kdcs_parse_request_field(qp,JPIP_FIELD_COMPONENTS))
        {
          contains_non_target_fields = true;
          char *end_cp;
          int from, to;
          for (scan=qp; (*scan != '&') && (*scan != '\0'); )
            {
              while (*scan == ',')
                scan++;
              from = to = strtol(scan,&end_cp,10);
              if (end_cp > scan)
                {
                  scan = end_cp;
                  if (*scan == '-')
                    {
                      scan++;
                      to = strtol(scan,&end_cp,10);
                      if (end_cp == scan)
                        to = INT_MAX;
                      scan = end_cp;
                    }
                }
              else
                scan--; // To force an error
              if (((*scan != ',') && (*scan != '&') && (*scan != '\0')) ||
                  (from < 0) || (from > to))
                { KDU_ERROR(e,6); e <<
                  KDU_TXT("Malformed ")
                  << "\"" JPIP_FIELD_COMPONENTS "\" " <<
                  KDU_TXT("field in query component of requested URL; "
                          "query string is:\n\n") << query;
                }
              if (window != NULL)
                window->components.add(from,to);
            }
        }
      else if (kdcs_parse_request_field(qp,JPIP_FIELD_CODESTREAMS))
        {
          contains_non_target_fields = true;
          char *end_cp;
          kdu_sampled_range range;
          for (scan=qp; (*scan != '&') && (*scan != '\0'); )
            {
              while (*scan == ',')
                scan++;
              range.step = 1;
              range.from = range.to = strtol(scan,&end_cp,10);
              if (end_cp > scan)
                {
                  scan = end_cp;
                  if (*scan == '-')
                    {
                      scan++;
                      range.to = strtol(scan,&end_cp,10);
                      if (end_cp == scan)
                        range.to = INT_MAX;
                      scan = end_cp;
                    }
                  if (*scan == ':')
                    {
                      scan++;
                      range.step = strtol(scan,&end_cp,10);
                      if (end_cp > scan)
                        scan = end_cp;
                      else
                        scan--; // To force an error
                    }
                }
              else
                scan--; // To force an error
              if (((*scan != ',') && (*scan != '&') && (*scan != '\0')) ||
                  (range.from < 0) || (range.from > range.to) ||
                  (range.step < 1))
                { KDU_ERROR(e,7); e <<
                  KDU_TXT("Malformed ")
                  << "\"" JPIP_FIELD_COMPONENTS "\" " <<
                  KDU_TXT("field in query component of requested URL; "
                          "query string is:\n\n") << query;
                }
              if (window != NULL)
                window->codestreams.add(range);
            }
        }
      else if (kdcs_parse_request_field(qp,JPIP_FIELD_CONTEXTS))
        { // Note: contexts may contain non-URIC chars -- need hex-hex decode
          contains_non_target_fields = true;
          if (window != NULL)
            {
              kdu_hex_hex_decode((char *)qp,field_sep);
              for (scan=qp; (*scan != '\0') && (scan != field_sep); )
                {
                  scan = window->parse_context(scan);
                  if ((*scan != ',') && (*scan != '&') && (*scan != '\0'))
                    { KDU_ERROR(e,8); e <<
                      KDU_TXT("Malformed ")
                      << "\"" JPIP_FIELD_CONTEXTS "\" " <<
                      KDU_TXT("field in query component of requested URL; "
                              "query string is:\n\n") << query;
                    }
                  while (*scan == ',')
                    scan++;
                }
            }
        }
      else if (kdcs_parse_request_field(qp,JPIP_FIELD_LAYERS))
        {
          contains_non_target_fields = true;
          int val;
          if ((sscanf(qp,"%d",&val) != 1) || (val < 0))
            { KDU_ERROR(e,9); e <<
              KDU_TXT("Malformed ")
              << "\"" JPIP_FIELD_LAYERS "\" " <<
              KDU_TXT("field in query component of "
                      "requested URL; query string is:\n\n") << query;
            }
          if (window != NULL)
            window->max_layers = val;
        }
      else if (kdcs_parse_request_field(qp,JPIP_FIELD_MAX_LENGTH))
        {
          contains_non_target_fields = true;
          int val;
          if ((sscanf(qp,"%d",&val) != 1) || (val < 0))
            { KDU_ERROR(e,0x02070901); e <<
              KDU_TXT("Malformed ")
              << "\"" JPIP_FIELD_MAX_LENGTH "\" " <<
              KDU_TXT("field in query component of "
                      "requested URL; query string is:\n\n") << query;
            }
          if (req != NULL)
            req->byte_limit = val;
        }      
      else if (kdcs_parse_request_field(qp,JPIP_FIELD_META_REQUEST))
        { // Note: metareqs may contain non-URIC chars -- need hex-hex decode
          contains_non_target_fields = true;
          if (*qp == '\0')
            { KDU_ERROR(e,10); e <<
              KDU_TXT("Malformed ")
              << "\"" JPIP_FIELD_META_REQUEST "\" " <<
              KDU_TXT("field in query component of requested URL.  At "
                      "least one descriptor must appear in the body of the "
                      "request field.  Query string is:\n\n") << query;
            }
          if (window != NULL)
            {
              kdu_hex_hex_decode((char *)qp,field_sep);
              const char *failure = window->parse_metareq(qp);
              if (failure != NULL)
                { KDU_ERROR(e,11); e <<
                  KDU_TXT("Malformed ")
                  << "\"" JPIP_FIELD_META_REQUEST "\" " <<
                  KDU_TXT("field in query component of requested URL.  "
                          "Problem encountered at:\n\n\t") << failure <<
                  KDU_TXT("\n\nComplete query string is:\n\n\t") << query;
                }
            }
        }
      if (qp != field_start)
        { // Remove the most recently parsed field.
          char *cp = (char *) field_start;
          if (field_sep == NULL)
            { // Just parsed the last request field
              if (field_start == query)
                *query = '\0';
              else
                cp[-1] = '\0';
            }
          else
            { // Copy everything from `field_sep'+1 to `field_start'
              for (field_sep++; *field_sep != '\0'; )
                *(cp++) = *(field_sep++);
              field_sep = field_start-1; // Location of the effective separator
              *cp = '\0';
            }
        }
    }
  
  if (query[0] != '\0')
    {
      contains_non_target_fields = true;
      all_fields_understood_and_compatible = false;
    }
  if (!create_target_strings)
    {
      if ((target_name != NULL) && !have_target_field)
        all_fields_understood_and_compatible = false;
      if ((sub_target_name != NULL) && !have_sub_target_field)
        all_fields_understood_and_compatible = false;
    }
  if (req != NULL)
    req->original_window.copy_from(req->window);
  return all_fields_understood_and_compatible;
}

/*****************************************************************************/
/* PRIVATE        kdu_client::obliterating_request_replied                   */
/*****************************************************************************/

void kdu_client::obliterating_request_replied()
{
  assert(obliterating_requests_in_flight > 0);
  obliterating_requests_in_flight--;
  if ((obliterating_requests_in_flight == 0) && !is_stateless)
    { 
      kdc_request_queue *queue;
      for (queue=request_queues; queue != NULL; queue=queue->next)
        { 
          kdc_request *req = queue->request_tail;
          if ((req != NULL) && req->untrusted &&
              (req->posted_service_time <= 0))
            { 
              req = queue->duplicate_request(req);
              if (req != NULL)
                req->preemptive = true;
            }
        }
    }
}

/*****************************************************************************/
/* PRIVATE                kdu_client::make_temp_string                       */
/*****************************************************************************/

char *kdu_client::make_temp_string(const char *src, int max_copy_chars)
{
  const char *cp;
  int len, max_len=max_copy_chars;
  if ((max_len < 0) || (max_len > (1<<16))) // A pretty massive string anyway
    max_len = (1 << 16);
  for (len=0, cp=src; *cp != '\0'; cp++, len++)
    if (len == max_len)
      {
        if (max_len != max_copy_chars)
          { KDU_ERROR(e,0x13030903); e <<
            KDU_TXT("Attempting to make a temporary copy of a string "
                    "(probably a network supplied name) which is ridiculously "
                    "long (more than 65K characters).  The copy is being "
                    "aborted to avoid potential exploitation by malicious "
                    "network agents.");
          }
        break;
      }
  if (len >= max_scratch_chars)
    {
      max_scratch_chars += (len+1);
      if (scratch_chars != NULL)
        delete[] scratch_chars;
      scratch_chars = NULL;
      scratch_chars = new char[max_scratch_chars];
    }
  memcpy(scratch_chars,src,(size_t) len);
  scratch_chars[len] = '\0';
  return scratch_chars;  
}

/*****************************************************************************/
/* PRIVATE        kdu_client::look_for_compatible_cache_file                 */
/*****************************************************************************/

bool kdu_client::look_for_compatible_cache_file(kdu_long &current_time)
{
  if (*target_id == '\0')
    return false;
  char *suffix = cache_path + strlen(cache_path);
  int result=-1, suffix_id = 1;
  while (result < 0)
    {
      sprintf(suffix,"-%d.kjc",suffix_id);
      release_management_lock();
      result = read_cache_contents(cache_path,target_id);
      acquire_management_lock(current_time);
      suffix_id++;
    }
  if (result == 1)
    {
      signal_status(); // Actually found a compatible file
      return true;
    }
  return false;
}

/*****************************************************************************/
/* PRIVATE             kdu_client::read_cache_contents                       */
/*****************************************************************************/

int kdu_client::read_cache_contents(const char *path, const char *tgt_id)
{
  FILE *cache_file = fopen(path,"rb");
  if (cache_file == NULL)
    return 0;
  
  int cache_store_len = 300;
  kdu_byte *cache_store_buf = new kdu_byte[cache_store_len];
  char *cp, *char_buf = (char *) cache_store_buf; *char_buf = '\0';
  if ((fgets(char_buf,80,cache_file) == NULL) || // Read version info
      (strcmp(char_buf,"kjc/1.1\n") != 0))
    {
      fclose(cache_file);
      delete[] cache_store_buf;
      return -2;
    }
  try {
    if ((fgets(char_buf,299,cache_file) == NULL) ||
        !kdcs_has_caseless_prefix(char_buf,"Host:"))
      { kdu_error e; e << "Error encountered in cache file header.  Expected "
        "\"Host:<host name>\" at line:\n\t" << char_buf;
      }
    if ((fgets(char_buf,299,cache_file) == NULL) ||
        !kdcs_has_caseless_prefix(char_buf,"Resource:"))
      { kdu_error e; e << "Error encountered in cache file header.  Expected "
        "\"Resource:<original resource name>\" at line:\n\t" << char_buf;
      }
    if ((fgets(char_buf,299,cache_file) == NULL) ||
        !kdcs_has_caseless_prefix(char_buf,"Target:"))
      { kdu_error e; e << "Error encountered in cache file header.  Expected "
        "\"Target:[<original target name>]\" at line:\n\t" << char_buf;
      }
    if ((fgets(char_buf,299,cache_file) == NULL) ||
        !kdcs_has_caseless_prefix(char_buf,"Sub-target:"))
      { kdu_error e; e << "Error encountered in cache file header.  Expected "
        "\"Sub-target:[<original sub-target>]\" at line:\n\t" << char_buf;
      }
    if ((fgets(char_buf,299,cache_file) == NULL) ||
        (!kdcs_has_caseless_prefix(char_buf,"Target-id:")) ||
        ((cp=strchr(char_buf,'\n')) == NULL))
      { kdu_error e; e << "Error encountered in cache file header.  Expected "
        "\"Target-id:<original target-id>\" at line:\n\t" << char_buf;
      }
    *cp = '\0';
    if (strcmp(char_buf+strlen("Target-id:"),tgt_id) != 0)
      { // Incompatible target-id
        fclose(cache_file);
        delete[] cache_store_buf;
        return -1;
      }
  
    kdu_long bin_id, cs_id;
    int i, id_bytes, cs_bytes, length;
    kdu_byte *sp;
    while (fread(cache_store_buf,1,2,cache_file) == 2)
      {
        cs_bytes = (cache_store_buf[1] >> 4) & 0x0F;
        id_bytes = cache_store_buf[1] & 0x0F;
        if (fread(cache_store_buf+2,1,(size_t)(cs_bytes+id_bytes+4),
                  cache_file) != (size_t)(cs_bytes+id_bytes+4))
          break;
        for (cs_id=0, sp=cache_store_buf+2, i=0; i < cs_bytes; i++)
          cs_id = (cs_id << 8) + *(sp++);
        for (bin_id=0, i=0; i < id_bytes; i++)
          bin_id = (bin_id << 8) + *(sp++);
        for (length=0, i=0; i < 4; i++)
          length = (length << 8) + *(sp++);
        bool is_complete = (cache_store_buf[0] & 1)?true:false;
        int cls = (cache_store_buf[0] >> 1);
        if (length > cache_store_len)
          {
            cache_store_len += length + 256;
            delete[] cache_store_buf;
            cache_store_buf = new kdu_byte[cache_store_len];
          }
        if (fread(cache_store_buf,1,
                  (size_t) length,cache_file) != (size_t)length)
          break;
        if ((cls >= 0) && (cls < KDU_NUM_DATABIN_CLASSES))
          add_to_databin(cls,cs_id,bin_id,cache_store_buf,0,length,
                         is_complete,false,true);
        if ((cls == KDU_MAIN_HEADER_DATABIN) && is_complete)
          add_model_manager(cs_id);
      }
  }
  catch (...) {
    fclose(cache_file);
    remove(path);
    delete[] cache_store_buf;
    return 0;
  }
  
  fclose(cache_file);
  delete[] cache_store_buf;
  return 1;
}

/*****************************************************************************/
/* PRIVATE             kdu_client::save_cache_contents                       */
/*****************************************************************************/

bool kdu_client::save_cache_contents(const char *path, const char *tgt_id,
                                     const char *host, const char *res,
                                     const char *tgt, const char *sub_tgt)
{
  FILE *cache_file = fopen(path,"wb");
  if (cache_file == NULL)
    return false;
  fprintf(cache_file,"kjc/1.1\n");
  fprintf(cache_file,"Host:%s\n",host);
  fprintf(cache_file,"Resource:%s\n",res);
  fprintf(cache_file,"Target:%s\n",(tgt==NULL)?"":tgt);
  fprintf(cache_file,"Sub-target:%s\n",(sub_tgt==NULL)?"":sub_tgt);
  fprintf(cache_file,"Target-id:%s\n",tgt_id);
  
  bool is_complete;
  int cls, length, id_bits, cs_bits, i;
  kdu_long cs_id, bin_id;
  kdu_byte *hd, header[24];

  int cache_store_len = 300;
  kdu_byte *cache_store_buf = new kdu_byte[cache_store_len];
  
  try {
    cs_id = get_next_codestream(-1);
    while (cs_id >= 0)
      {
        for (cls=0; cls < KDU_NUM_DATABIN_CLASSES; cls++)
          {
            bin_id = get_next_lru_databin(cls,cs_id,-1,false);
            while (bin_id >= 0)
              {
                length = get_databin_length(cls,cs_id,bin_id,&is_complete);
                if ((length > 0) || is_complete)
                  { // Save data-bin
                    if (length > cache_store_len)
                      {
                        if (cache_store_buf != NULL) delete[] cache_store_buf;
                        cache_store_len += length+256;
                        cache_store_buf = new kdu_byte[cache_store_len];
                      }
                    length = get_databin_prefix(cls,cs_id,bin_id,
                                                cache_store_buf,length);
                    hd = header;
                    if (is_complete)
                      *(hd++) = (kdu_byte)(2*cls+1);
                    else
                      *(hd++) = (kdu_byte)(2*cls);
                    for (cs_bits=0; (cs_id>>cs_bits) > 0; cs_bits+=8);
                    for (id_bits=0; (bin_id>>id_bits) > 0; id_bits+=8);
                    *(hd++) = (kdu_byte)((cs_bits<<1) | (id_bits>>3));
                    for (i=cs_bits-8; i >= 0; i-=8)
                      *(hd++) = (kdu_byte)(cs_id>>i);
                    for (i=id_bits-8; i >= 0; i-=8)
                      *(hd++) = (kdu_byte)(bin_id>>i);
                    for (i=24; i >= 0; i-=8)
                      *(hd++) = (kdu_byte)(length>>i);
                    fwrite(header,1,(size_t)(hd-header),cache_file);
                    fwrite(cache_store_buf,1,(size_t) length,cache_file);
                  }
                bin_id = get_next_lru_databin(cls,cs_id,bin_id,false);
              }
          }
        cs_id = get_next_codestream(cs_id);
      }
  }
  catch (...) { // Something went wrong; delete the cache file
    fclose(cache_file);
    remove(path);
    delete[] cache_store_buf;
    return false;
  }
  fclose(cache_file);
  delete[] cache_store_buf;
  return true;
}

/*****************************************************************************/
/* PRIVATE                kdu_client::get_scratch_ints                       */
/*****************************************************************************/

int *kdu_client::get_scratch_ints(int len)
{
  if (len & 0xFF000000)
    { KDU_ERROR(e,0x13030904); e <<
      KDU_TXT("Attempting to make a temporary buffer to store data "
              "(probably based on network-supplied parameters) which is "
              "ridiculously long (more than 65K characters).  The allocation "
              "is being aborted to avoid potential exploitation by malicious "
              "network agents.");
    }
  if (len > max_scratch_ints)
    {
      max_scratch_ints += len;
      if (scratch_ints != NULL)
        delete[] scratch_ints;
      scratch_ints = NULL;
      scratch_ints = new int[max_scratch_ints];
    }
  return scratch_ints;
}

/*****************************************************************************/
/* PRIVATE               kdu_client::add_model_manager                       */
/*****************************************************************************/

kdc_model_manager *
  kdu_client::add_model_manager(kdu_long codestream_id)
{
  kdc_model_manager *mgr, *prev_mgr=NULL;
  for (mgr=model_managers; mgr != NULL; prev_mgr=mgr, mgr=mgr->next)
    if (mgr->codestream_id >= codestream_id)
      break;
  if ((mgr != NULL) && (mgr->codestream_id == codestream_id))
    return mgr; // Already exists
  kdc_model_manager *new_mgr = new kdc_model_manager;
  new_mgr->next = mgr;
  if (prev_mgr == NULL)
    model_managers = new_mgr;
  else
    prev_mgr->next = new_mgr;
  new_mgr->codestream_id = codestream_id;
  new_mgr->aux_cache.attach_to(this);
  new_mgr->aux_cache.set_read_scope(KDU_MAIN_HEADER_DATABIN,codestream_id,0);
  new_mgr->codestream.create(&new_mgr->aux_cache);
  new_mgr->codestream.set_persistent();
  return new_mgr;
}

/*****************************************************************************/
/* PRIVATE              kdu_client::signal_model_corrections                 */
/*****************************************************************************/

bool kdu_client::signal_model_corrections(kdu_window &ref_window,
                                          kdcs_message_block &block)
{
  int start_chars = block.get_remaining_bytes(); // To backtrack if required
  block << "&model=";
  int test_chars = block.get_remaining_bytes();
  
  // Scan through all relevant code-streams and code-stream contexts
  bool cs_started, is_complete;
  int available_bytes;
  kdu_long bin_id;
  int cs_idx, cs_range_num, ctxt_idx, ctxt_range_num;
  int member_idx, num_members;
  kdu_sampled_range *rg=NULL;
  kdu_client_translator *translator = context_translator;
  if (translator != NULL)
    translator->update();
  cs_range_num = ctxt_range_num = ctxt_idx = member_idx = num_members = 0;
  kdu_window_context ctxt;
  while (1)
    {
      // Find next code-stream or code-stream context
      kdu_coords res = ref_window.resolution;
      kdu_dims reg = ref_window.region;
      int num_context_comps = 0;
      const int *context_comps = NULL;
      int num_cs_comps = 0;
      int *cs_comps = NULL;
      if (translator != NULL)
        { // Get codestream and window by translating codestream context
          member_idx++;
          if (member_idx >= num_members)
            { // Load next context
              if ((rg == NULL) || ((ctxt_idx += rg->step) > rg->to))
                { // Advance to next range
                  rg = ref_window.contexts.access_range(ctxt_range_num++);
                  if (rg == NULL)
                    { translator = NULL; continue; }
                  else
                    ctxt_idx = rg->from;
                }
              member_idx = 0;
              ctxt = translator->access_context(rg->context_type,ctxt_idx,
                                                rg->remapping_ids);
              if ((!ctxt) ||
                  ((num_members=ctxt.get_num_members(rg->remapping_ids)) <= 0))
                continue;
            }
          cs_idx = ctxt.get_codestream(rg->remapping_ids,member_idx);
          if (!ctxt.perform_remapping(rg->remapping_ids,member_idx,res,reg))
            continue; // Cannot translate view window
          context_comps = ctxt.get_components(rg->remapping_ids,member_idx,
                                              num_context_comps);
        }
      else
        { // Get codestream directly, without translation
          if ((rg == NULL) || ((cs_idx += rg->step) > rg->from))
            { // Advance to next range
              rg = ref_window.codestreams.access_range(cs_range_num++);
              if (rg == NULL)
                break; // Break out of main loop; all code-streams checked
              if (rg->context_type == KDU_JPIP_CONTEXT_TRANSLATED)
                { rg = NULL; continue; }
              cs_idx = rg->from;
            }
        }
      
      if (cs_idx < 0)
        continue;
      cs_started = false;
      
      // Check main code-stream header first
      available_bytes = get_databin_length(KDU_MAIN_HEADER_DATABIN,
                                           cs_idx,0,&is_complete);
      if (((available_bytes > 0) || is_complete) &&
          (is_stateless ||
           mark_databin(KDU_MAIN_HEADER_DATABIN,cs_idx,0,false)))
        write_cache_descriptor(cs_idx,cs_started,
                               "Hm",-1,available_bytes,is_complete,block);
      kdc_model_manager *mgr;
      for (mgr=model_managers; mgr != NULL; mgr=mgr->next)
        if (mgr->codestream_id >= cs_idx)
          break;
      if ((mgr == NULL) || (mgr->codestream_id != cs_idx) || !is_complete)
        continue; // No model info available for this code-stream, or else
                  // no point looking for precinct and tile headers, since
                  // main header is not available.
      
      kdu_codestream aux_stream = mgr->codestream;
      if (!aux_stream)
        continue; // Can't open this code-stream yet.
      if ((res.x < 1) || (res.y < 1) || (reg.size.x < 1) || (reg.size.y < 1))
        continue; // No relevant tile headers or precincts
      
      // Find the set of available components
      int total_cs_comps = aux_stream.get_num_components();
      bool expand_ycc = false;
      if (context_comps == NULL)
        { // Expand the codestream component ranges to simplify matters
          cs_comps = get_scratch_ints(total_cs_comps);
          if ((num_cs_comps =
               ref_window.components.expand(cs_comps,0,total_cs_comps-1)) == 0)
            for (num_cs_comps=0; num_cs_comps < total_cs_comps; num_cs_comps++)
              cs_comps[num_cs_comps] = num_cs_comps; // Includes all components
          if (total_cs_comps >= 3)
            { // See if we need to include any extra components to allow for
              // inverting a Part-1 decorrelating transform
              int i;
              bool ycc_usage[3]={false,false,false};
              for (i=0; i < num_cs_comps; i++)
                if (cs_comps[i] < 3)
                  expand_ycc = ycc_usage[cs_comps[i]] = true;
              if (expand_ycc)
                for (i=0; i < 3; i++)
                  if (!ycc_usage[i])
                    cs_comps[num_cs_comps++] = i;
            }
        }
      
      // Get global codestream parameters
      kdu_dims image_dims, total_tiles;
      aux_stream.apply_input_restrictions(0,0,0,0,NULL,
                                          KDU_WANT_OUTPUT_COMPONENTS);
      aux_stream.get_dims(-1,image_dims);
      aux_stream.get_valid_tiles(total_tiles);
      
      // Mimic the server's window adjustments.
      // Start by figuring out the number of discard levels
      int round_direction = ref_window.round_direction;
      kdu_coords min = image_dims.pos;
      kdu_coords size = image_dims.size;
      kdu_coords lim = min + size;
      kdu_dims active_res; active_res.pos = min; active_res.size = size;
      kdu_long target_area = ((kdu_long) res.x) * ((kdu_long) res.y);
      kdu_long best_area_diff = 0;
      int active_discard_levels=0, d=0;
      bool done = false;
      while (!done)
        {
          if (round_direction < 0)
            { // Round down
              if ((size.x <= res.x) && (size.y <= res.y))
                {
                  active_discard_levels = d;
                  active_res.size = size; active_res.pos = min;
                  done = true;
                }
            }
          else if (round_direction > 0)
            { // Round up
              if ((size.x >= res.x) && (size.y >= res.y))
                {
                  active_discard_levels = d;
                  active_res.size = size; active_res.pos = min;
                }
              else
                done = true;
            }
          else
            { // Round to closest in area
              kdu_long area = ((kdu_long) size.x) * ((kdu_long) size.y);
              kdu_long area_diff =
                (area < target_area)?(target_area-area):(area-target_area);
              if ((d == 0) || (area_diff < best_area_diff))
                {
                  active_discard_levels = d;
                  active_res.size = size; active_res.pos = min;
                  best_area_diff = area_diff;
                }
              if (area <= target_area)
                done = true; // The area can only keep on getting smaller
            }
          min.x = (min.x+1)>>1;   min.y = (min.y+1)>>1;
          lim.x = (lim.x+1)>>1;   lim.y = (lim.y+1)>>1;
          size = lim - min;
          d++;
        }
      
      // Now scale the image region to match the selected image resolution
      kdu_dims active_region;
      min = reg.pos;
      lim = min + reg.size;
      active_region.pos.x = (int)
        ((((kdu_long) min.x)*((kdu_long)active_res.size.x))/((kdu_long)res.x));
      active_region.pos.y = (int)
        ((((kdu_long) min.y)*((kdu_long)active_res.size.y))/((kdu_long)res.y));
      active_region.size.x = 1 + (int)
        ((((kdu_long)(lim.x-1)) *
          ((kdu_long) active_res.size.x)) / ((kdu_long) res.x))
        - active_region.pos.x;
      active_region.size.y = 1 + (int)
        ((((kdu_long)(lim.y-1)) *
          ((kdu_long) active_res.size.y)) / ((kdu_long) res.y))
        - active_region.pos.y;
      active_region.pos += active_res.pos;
      active_region &= active_res;
      
      // Now adjust the active region up onto the full codestream canvas
      active_region.pos.x <<= active_discard_levels;
      active_region.pos.y <<= active_discard_levels;
      active_region.size.x <<= active_discard_levels;
      active_region.size.y <<= active_discard_levels;
      active_region &= image_dims;
  
      // Now scan through the tiles
      kdu_dims active_tiles, active_precincts;
      aux_stream.apply_input_restrictions(0,0,0,0,&active_region,
                                          KDU_WANT_OUTPUT_COMPONENTS);
      aux_stream.get_valid_tiles(active_tiles);
      kdu_coords t_idx, abs_t_idx, p_idx;
      kdu_tile tile;
      kdu_tile_comp tcomp;
      kdu_resolution rs;
      int tnum, r, num_resolutions;
      for (t_idx.y=0; t_idx.y < active_tiles.size.y; t_idx.y++)
        for (t_idx.x=0; t_idx.x < active_tiles.size.x; t_idx.x++)
          {
            abs_t_idx = t_idx + active_tiles.pos;
            tnum = abs_t_idx.x + abs_t_idx.y * total_tiles.size.x;
            available_bytes = get_databin_length(KDU_TILE_HEADER_DATABIN,
                                                 cs_idx,tnum,&is_complete);
            if (((available_bytes > 0) || is_complete) &&
                (is_stateless ||
                 mark_databin(KDU_TILE_HEADER_DATABIN,cs_idx,tnum,false)))
              write_cache_descriptor(cs_idx,cs_started,"H",tnum,
                                     available_bytes,is_complete,block);
            if (!is_complete)
              continue;
            tile = aux_stream.open_tile(abs_t_idx);
            bool have_ycc = tile.get_ycc() && expand_ycc;
            if (context_comps != NULL)
              { // Convert context components into codestream components for
                // this tile.
                int nsi, nso, nbi, nbo;
                tile.set_components_of_interest(num_context_comps,
                                                context_comps);
                tile.get_mct_block_info(0,0,nsi,nso,nbi,nbo);
                num_cs_comps = nsi;
                cs_comps = get_scratch_ints(num_cs_comps);
                tile.get_mct_block_info(0,0,nsi,nso,nbi,nbo,
                                        NULL,NULL,NULL,NULL,cs_comps);
              }
            
            for (int nc=0; nc < num_cs_comps; nc++)
              {
                int c_idx = cs_comps[nc];
                if (((c_idx >= 3) || !have_ycc) &&
                    !(ref_window.components.is_empty() ||
                      ref_window.components.test(c_idx)))
                  continue; // Component is excluded
                
                tcomp = tile.access_component(c_idx);
                num_resolutions = tcomp.get_num_resolutions();
                num_resolutions -= active_discard_levels;
                if (num_resolutions < 1)
                  num_resolutions = 1;
                for (r=0; r < num_resolutions; r++)
                  {
                    rs = tcomp.access_resolution(r);
                    rs.get_valid_precincts(active_precincts);
                    for (p_idx.y=0; p_idx.y < active_precincts.size.y;
                         p_idx.y++)
                      for (p_idx.x=0; p_idx.x < active_precincts.size.x;
                           p_idx.x++)
                        {
                          bin_id =
                          rs.get_precinct_id(p_idx+active_precincts.pos);
                          available_bytes =
                            get_databin_length(KDU_PRECINCT_DATABIN,
                                               cs_idx,bin_id,&is_complete);
                          if (((available_bytes > 0) || is_complete) &&
                              (is_stateless ||
                               mark_databin(KDU_PRECINCT_DATABIN,
                                            cs_idx,bin_id,false)))
                            write_cache_descriptor(cs_idx,cs_started,
                                                   "P",bin_id,available_bytes,
                                                   is_complete,block);
                        }
                  }
              }
            tile.close();
          }
    }
  
  // Finally, signal whatever meta-data bins we already have
  cs_started = true; // Prevent inclusion of additional code-stream qualifiers
  bin_id = get_next_lru_databin(KDU_META_DATABIN,0,-1,!is_stateless);
  while (bin_id >= 0)
    {
      available_bytes =
      get_databin_length(KDU_META_DATABIN,0,bin_id,&is_complete);
      if (((available_bytes > 0) || is_complete) &&
          (is_stateless || mark_databin(KDU_META_DATABIN,0,bin_id,false)))
        write_cache_descriptor(0,cs_started,"M",bin_id,available_bytes,
                               is_complete,block);
      bin_id = get_next_lru_databin(KDU_META_DATABIN,0,bin_id,!is_stateless);
    }

  // Now we are done writing all model information
  if (block.get_remaining_bytes() == test_chars)
    {
      block.backspace(block.get_remaining_bytes()-start_chars);
      return false;
    }
  else
    block.backspace(1); // Backspace over the trailing comma.
  return true;  
}

/*****************************************************************************/
/* PRIVATE                  kdu_client::thread_cleanup                       */
/*****************************************************************************/

void kdu_client::thread_cleanup()
{
  kdu_long current_time;
  acquire_management_lock(current_time);
  if (!non_interactive)
    { // Under some conditions we can provide a more informative final
      // status message for the session than that which might be there
      // currently.
      if (image_done)
        final_status = "Image completely downloaded.";
      else if (session_limit_reached)
        final_status = "Session limit reached (server side).";
      signal_status();
    }
  requested_transport[0] = '\0';
  is_stateless = true;
  
  while (request_queues != NULL)
    release_request_queue(request_queues);
  next_request_queue_id = 0;
  while (cids != NULL)
    release_cid(cids);
  kdc_primary *chn, *next_chn;
  for (chn=primary_channels; chn != NULL; chn=next_chn)
    {
      next_chn = chn->next;
      if (!(chn->keep_alive && chn->is_persistent))
        release_primary_channel(chn);
    }
  while (model_managers != NULL)
    {
      kdc_model_manager *mdl = model_managers;
      model_managers = mdl->next;
      delete mdl;
    }
  next_disconnect_usecs = -1;
  have_queues_ready_to_close = false;
  
  if (notifier != NULL)
    {
      notifier->notify();
      notifier = NULL;
    }
  context_translator = NULL;
  
  disconnect_event.set(); // Just in case
  release_management_lock();
}

/*****************************************************************************/
/*                          kdu_client::thread_start                         */
/*****************************************************************************/

void kdu_client::thread_start()
{
  kdcs_start_network(); // Does nothing in Unix-like OS's
  try {
    run();
  }
  catch (...)
  { }
  thread_cleanup();
  kdcs_cleanup_network();
}

/*****************************************************************************/
/*                     kdu_client::acquire_management_lock                   */
/*****************************************************************************/

void kdu_client::acquire_management_lock(kdu_long &current_time)
{ 
  if (!management_lock_acquired)
    { mutex.lock(); management_lock_acquired = true; }
  current_time = this->timer->get_ellapsed_microseconds();
}

/*****************************************************************************/
/* PRIVATE                       kdu_client::run                             */
/*****************************************************************************/

void kdu_client::run()
{
  kdu_long current_time;
  acquire_management_lock(current_time);
  while ((request_queues != NULL) &&
         !(close_requested || session_limit_reached))
    { 
      // See if we are waiting for a disconnect to timeout
      kdu_long max_monitor_wait_usecs = 2000000; // 2 seconds is a lot
      if (next_disconnect_usecs >= 0)
        max_monitor_wait_usecs = next_disconnect_usecs - current_time;
      
      // First, see if we need to release any request queues
      if (have_queues_ready_to_close || (max_monitor_wait_usecs <= 0))
        {
          next_disconnect_usecs = -1;
          kdc_request_queue *queue, *next_queue;
          for (queue=request_queues; queue != NULL; queue=next_queue)
            {
              next_queue = queue->next;
              if (queue->close_when_idle)
                {
                  if (queue->is_idle ||
                      (queue->disconnect_timeout_usecs <= current_time))
                    {
                      release_request_queue(queue);
                      next_queue = request_queues;
                    }
                  else if ((next_disconnect_usecs < 0) ||
                           (next_disconnect_usecs >
                            queue->disconnect_timeout_usecs))
                    next_disconnect_usecs = queue->disconnect_timeout_usecs;
                }
            }
          max_monitor_wait_usecs = 2000000; // 2 seconds is a lot
          if (next_disconnect_usecs >= 0)
            max_monitor_wait_usecs = next_disconnect_usecs - current_time;
          have_queues_ready_to_close = false;
        }
      
      // Next see if there are any CID's whose auxiliary channels need to
      // be kicked into life, or which are able to deliver new requests.
      bool issued_new_request = false;
      kdc_cid *cid, *next_cid;
      for (cid=cids; cid != NULL; cid=next_cid)
        { 
          next_cid = cid->next; // Just in case we release the current CID
          if (cid->newly_assigned_by_server)
            continue; // Cannot issue a new request until primary connection
                      // details have been verified and/or reassigned.
          if (cid->uses_aux_channel && (cid->aux_tcp_channel == NULL) &&
              (cid->aux_udp_channel == NULL))
            { // Ready to fire up the auxiliary channel.  We need at least to
              // initiate the connection process here, even if it must be
              // completed within the `cid->service_channel' function.  Since
              // connection attempts may throw exceptions, we need to be in
              // a position to release the CID and move on; this is a good
              // context for doing that.
              if (cid->aux_channel_is_udp)
                cid->aux_udp_channel = new kdcs_udp_channel(monitor,true);
              else
                cid->aux_tcp_channel = new kdcs_tcp_channel(monitor,true);
              cid->aux_channel_connected = false; // Just to be safe
              try {
                if (cid->connect_aux_channel(current_time))
                  while (cid->read_aux_chunk(current_time));
              }
              catch (kdu_exception) {
                acquire_management_lock(current_time);
                    // In case exception while unlocked
                release_cid(cid);
                next_cid = cids; // Go back and start from scratch
                continue;
              }
            }
          if (cid->primary_channel->active_requester != NULL)
            continue; // Cannot issue a new request yet; primary channel busy
          kdc_chunk_gap *gaps = NULL;
          if (!cid->channel_close_requested)
            gaps = cid->find_gaps_to_abandon(current_time,false);
          try {
            kdc_request_queue *queue =
              cid->find_next_requester(current_time,(gaps != NULL));
            if (queue != NULL)
              { 
                queue->issue_request(current_time,gaps);
                issued_new_request = true;
              }
          }
          catch (kdu_exception exc) {
            acquire_management_lock(current_time);
            if (gaps != NULL)
              recycle_chunk_gaps(gaps);
            throw exc;
          }
          if (gaps != NULL)
            { 
              recycle_chunk_gaps(gaps);
              obliterating_request_issued(); // Even if for some reason we
                    // did not manage to issue the obliterating request, we
                    // should call this function, since it means that all
                    // responses will be marked untrusted until the reply to
                    // the obliterating request returns.  If there is no such
                    // reply, the entire session will remain untrusted
                    // indefinitely, which is the right behaviour in case
                    // somehow the request did not get issued.
            }
        }
      
      // Now perform any special servicing required for primary channels
      if (issued_new_request)
        {
          kdc_primary *chn, *next_chn;
          for (chn=primary_channels; chn != NULL; chn=next_chn)
            {
              next_chn = chn->next;
              if ((chn->active_requester != NULL) &&
                  (chn->send_block.get_remaining_bytes() > 0))
                {
                  try {
                    chn->send_active_request(current_time);
                  }
                  catch (kdu_exception) {
                    acquire_management_lock(current_time);
                       // In case exc. while unlocked
                    release_primary_channel(chn);
                    next_chn = primary_channels; // Go back and start again
                  }
                }
            }
        }
  
      // Finally, run the network channel monitor once
      release_management_lock();
      kdu_long max_select_wait_usecs = 50000; // 50 ms
      if (max_monitor_wait_usecs < max_select_wait_usecs)
        max_select_wait_usecs = max_monitor_wait_usecs;
      monitor->run_once((int) max_select_wait_usecs,
                        (int) max_monitor_wait_usecs);
      acquire_management_lock(current_time);
    }
  if ((cache_path != NULL) && (!check_for_local_cache) &&
      (target_id[0] != '\0'))
    {
      const char *save_status_text = final_status;
      final_status = "Saving cache contents to disk.";
      signal_status();
      release_management_lock();
      save_cache_contents(cache_path,target_id,host_name,resource_name,
                          target_name,sub_target_name);
      acquire_management_lock(current_time);
      final_status = save_status_text;
    }
  release_management_lock();
}
