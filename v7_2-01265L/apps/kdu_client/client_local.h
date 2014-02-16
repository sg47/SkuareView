/*****************************************************************************/
// File: client_local.h [scope = APPS/KDU_CLIENT]
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
  Private definitions used in the implementation of the "kdu_client" class.
******************************************************************************/

#ifndef CLIENT_LOCAL_H
#define CLIENT_LOCAL_H

#include "kdcs_comms.h" // Must include this first
#include "kdu_client.h"

// Defined here:
struct kdc_chunk_gap;
struct kdc_request_dependency;
struct kdc_request;
class kdc_flow_regulator;
class kdc_model_manager;
class kdc_primary;
class kdc_cid;
struct kdc_request_queue;

#define KDC_ABANDON_FACTOR 2
#define KDC_WINDOW_TARGET 10
#define KDC_MAX_REQUEST_RTT (3000000/KDC_ABANDON_FACTOR)

/*****************************************************************************/
/*                               kdc_chunk_gap                               */
/*****************************************************************************/

struct kdc_chunk_gap {
    kdu_long qid; // request-id of the request to which gap belongs
    int seq_from; // 1st sequence number in range of consecutive missing chunks
    int seq_to; // final sequence number in range, or -1 if open ended
    kdc_chunk_gap *next;
  };
  /* Notes:
        This structure plays an important role in supporting unreliable
     transport protocols such as HTTP-UDP.  The structure is used to build
     a list which keeps track of data chunks which have not yet arrived in
     response to a request. The list is initialized with a single chunk
     gap of the form (0,-), meaning everything is missing from chunk 0 to the
     unknown end of the response.  As soon as a chunk arrives which contains
     the EOR message, the final element in the list is adjusted to express
     a closed range (i.e., `seq_to' becomes non-negative).
        Once the list becomes empty, the request has been completely served.
     However, the request may have dependencies which have not yet been
     satisfied -- dependencies are prior requests or requests on other
     channels, whose response data has not yet been completely received.
     For more on this, see `kdc_request'. */

/*****************************************************************************/
/*                           kdc_request_dependency                          */
/*****************************************************************************/

struct kdc_request_dependency {
    kdc_request_queue *queue;
    kdu_long qid;
    kdc_request_dependency *next;
  };

/*****************************************************************************/
/*                                kdc_request                                */
/*****************************************************************************/

struct kdc_request {
  public: // Member functions
    void init(kdc_request_queue *queue, bool session_untrusted)
      { 
        custom_id=0; posted_service_time=0; this->queue = queue;
        window.init(); extra_query_fields=NULL; oob_caller_id = 0;
        preemptive = true; qid = -1; byte_limit = 0;
        received_body_bytes = received_message_bytes = 0;
        response_terminated = reply_received = chunk_received = false;
        window_completed = image_done = session_limit_reached = false;
        quality_limit_reached = byte_limit_reached = false;
        new_elements = true; obliterating = is_copy = false;
        untrusted = session_untrusted;
        request_issue_time=-1; last_event_time=-1; received_service_time=0;
        nominal_start_time = target_end_time = -1;
        target_duration = disparity_compensation = 0;
        unblock_primary_upon_reply = false;
        unblock_primary_upon_comms_complete = false;
        is_primary_active_request = is_cid_active_receiver = false;
        primary_next_request = cid_next_receiver = NULL;
        copy_src = next_copy = next = NULL;
        chunk_gaps = NULL; dependencies = NULL;
      }
    void set_response_terminated(kdu_long current_time);
      /* Never set `response_terminated' to true directly; this function
         performs any additional actions that should accompany the detection
         of the last data chunk of a request or the fact that this has been
         missed.  These additional actions depend upon the EOR reason code
         flags, which should be set as appropriate before the function is
         called from within `kdc_cid::process_return_data'. */
    bool communication_complete()
      { // Returns true, if all communication associated with the request has
        // been completed.
        return response_terminated && reply_received && (chunk_gaps==NULL);
      }
    bool is_complete()
      { // Returns true if the request can be retired.  This happens only
        // once communication is complete and all dependencies have been
        // satisfied.
        return communication_complete() &&
               (untrusted || (dependencies==NULL) ||
                !(window_completed || quality_limit_reached ||
                  byte_limit_reached || session_limit_reached || image_done));
      }
    void remove_dependency(const kdc_request *dep, const kdc_request *alt_dep);
      /* This function searches the `dependencies' list for one which refers
         to the request identified by `dep'.  If one is found, the dependency
         is either removed (if `alt_dep' is NULL) or changed to refer to the
         `alt_dep' request instead. */
    void add_dependency(const kdc_request *dep);
      /* This function adds `dep' to the list of other requests on which
         this request depends.  The `dep' request should not be added as a
         dependency unless it is issued within an unreliable transport
         channel.  If the request already contains a dependency from the
         same request queue as `dep', the existing dependency is replaced
         with `dep'. */
  public: // Data members that identify the request
    kdu_long custom_id; // 0 unless a custom-id was supplied with `post_window'
    kdu_long posted_service_time; // `service_usecs' supplied by `post_window'
    kdc_request_queue *queue; // identifies the queue to which request belongs
    kdu_window original_window; // Original window used to formulate request
    kdu_window window; // Same as `original_window' but with any server mods
    const char *extra_query_fields; // For first request issued after `connect'
    int oob_caller_id; // Used only with the OOB request queue
    bool preemptive; // True if this request preempts earlier ones
  public: // Data members that keep track of the state of the request
    kdu_long qid; // -1 if request is issued without a JPIP QID field
    int byte_limit; // 0 if no limit sent when issuing the request
    int received_body_bytes; // Message body bytes received (excludes EOR body)
    int received_message_bytes; // Message bytes received (excludes EOR body)
    bool new_elements; // True if not a known subset of a previous request
    bool response_terminated; // If response empty or received EOR message
    bool window_completed; // True if all data for the window has been sent
    bool quality_limit_reached; // True if response terminated at quality limit
    bool byte_limit_reached; // True if response terminated at byte limit
    bool session_limit_reached; // These two members hold EOR conditions which
    bool image_done; // are reflected to `kdu_client' when request completes.
    bool reply_received; // True if server has replied to the request
    bool chunk_received; // True if any data chunk for request has arrived
    bool is_copy; // If `copy_src' is or ever was non-NULL
    bool untrusted; // If can't trust completeness of this request's response
    bool obliterating; // See below for definition of `obliterating' requests
  public: // Timing members
    kdu_long request_issue_time; // Time at which request issued to server
    kdu_long last_event_time; // See below
    kdu_long received_service_time; // Cumulative time between chunk receipt
      // events; service time is accumulated across copies of this request.
    kdu_long nominal_start_time; // -ve if not a timed request; see below
    kdu_long target_end_time; // -ve until known; see below
    kdu_long target_duration; // 0 if not a timed request; see below
    kdu_long disparity_compensation; // See below
  public: // Data members that identify outstanding actions
    bool unblock_primary_upon_reply; // These flags hold conditions for primary
    bool unblock_primary_upon_comms_complete; // active_requester field -> NULL
    bool is_primary_active_request; // If still on primary active request list
    bool is_cid_active_receiver;    // If still on CID active receiver list
  public: // Lists and links
    kdc_request *primary_next_request; // For `kdc_primary' active request list
    kdc_request *cid_next_receiver; // For `kdc_cid' active receiver list
    kdc_request *copy_src; // Request we were copied from, if any
    kdc_request *next_copy; // Request that represents a copy of us, if any
    kdc_chunk_gap *chunk_gaps; // See below
    kdc_request_dependency *dependencies; // See below
    kdc_request *next; // Used to build linked list of requests within a queue
  };
  /* Notes:
        The `chunk_gaps' list is partially explained in the definition of
     `kdc_chunk_gap'.  It plays an important role in managing the
     out-of-order arrival of data chunks in response to this request, when
     the auxiliary data channel does not use a reliable, sequential medium
     such as TCP.  Requests are not generally cleared from the request
     `queue' until all missing data chunks have arrived.  Moreover, the
     most recent incomplete request in the `queue' is not cleared until
     all of its `dependencies' have been satisfied.  The only way to
     escape from these requirements is to explicitly send negative
     acknowledge request fields over the primary request channel.
        The `dependencies' list keeps track of the most recent known
     request within this or any other request queue, whose response must
     arrive in full before we can trust the information provided by this
     request's EOR response message.  When an EOR code is detected, the
     `response_terminated' field is set to true immediately, and
     `window_completed' and/or `quality_limit_reached' are set, as
     appropriate.  However, not until `chunk_gaps' becomes NULL and
     `dependencies' also becomes NULL can the request be considered truly
     complete.  These requests remain on the `kdc_request_queue's
     incomplete list.
        When a request is issued, the `dependencies' list is initialized with
     the identity of the most recent request from this request queue and each
     other request queue, which has been issued, but for which not all response
     data has yet been received.  New requests within other CID's can become
     dependencies if they are issued before the current request's EOR message
     is received.
        An "obliterating" request is one which potentially removes something
     from the server's cache model.  Any request containing negative
     acknowledgement of data chunks is obliterating.  Any session-based
     request which subtractively manipulates the cache contents, issuing a
     corresponding subtractive cache model manipulation request field or an
     "mset" request field, is also considered obliterating.  Since
     obliterating requests are accompanied by the removal (or commitment to
     remove or not receive) some data which the server may have already sent
     (or committed to send in the future), the reason codes provided with EOR
     messages delivered on the same or a different JPIP channel cannot be
     taken seriously.  This is because these EOR messages may reflect an
     assumption about the client's cache contents which is overly optimistic.
     This remains true until the obliterating request is received and
     processed at the server, but we can only be sure that this has happened
     once we receive the reply paragraph to the obliterating request.  Up
     until that point, any newly issued requests (other than the obliterating
     one in question) must be marked as `untrusted'.  We say that an
     obliterating request is "in flight" when it has been issued, but the
     reply has not yet been received.  The `kdu_client' object itself
     keeps track of the number of obliterating requests which are in flight
     via the `kdu_client::obliterating_requests_in_flight' variable, which
     is manipulated via `kdu_client::obliterating_request_issued' and
     `kdu_client::obliterating_request_replied'.
        It should be noted that the `untrusted' condition is very special.  It
     is only set in one of two ways: a) if obliterating requests are sent
     to the server, requests are marked as untrusted until such point as we
     can be sure that the server has taken all the obliterating statements
     into account; and b) if response data is permanently lost without
     issuing any obliterating request (e.g., a JPIP channel dies unexpectedly
     with outstanding requests in progress), the entire session is marked as
     untrusted and all current and future requests within that session
     (except those which have already completed) are marked as `untrusted'.
        The `session_limit_reached' and `image_done' flags are set when
     the EOR message code is received.  If either of these is true, the
     corresponding member in `kdu_client' is also set to true, but not until
     the `kdc_request_queue::process_completed_requests' function is called,
     which does not happen until all missing chunks and dependencies have
     been cleared.
        The `last_event_time' member stores the absolute time (in microseconds)
     when the last network event occurred for this request.  When the request
     is first issued, this member and `request_issue_time' are both set to
     the time at which the request went out.  When the reply paragraph comes
     back, this becomes the time at which the reply was received.  Thereafter,
     each time a new data chunk comes back this member becomes the time at
     which the last data chunk was received.  Even though data chunks may
     arrive before the reply is received, the times associated with those
     events are not recorded.
        The `nominal_start_time' member is used with timed requests.  If
     multiple request queues share a single CID (JPIP channel) and any of
     them issues timed requests (requests with `posted_service_time' > 0),
     then all of them issue timed requests, which means that timed requests
     may have to be synthesized for some of the queues on the fly.  If the
     application posts a timed request to a queue, the `posted_service_time'
     and `nominal_start_time' members are set immediately.  For the first
     in a sequence of timed requests, a suitable value for `nominal_start_time'
     must be guessed, based on round-trip-time statistics; however, once
     the first chunk of data for a timed request sequence is received, this
     initial guess is corrected to reflect the actual "start time" of the
     response data.  All requests have their `nominal_start_time' values
     corrected at the same time.  If a timed requests must be synthesized,
     this is done at the point when the request is about to be issued, by
     duplicating the original request, and writing a synthesized value for
     `nominal_start_time' into the first such copy.  Synthesized timed
     requests never acquire a non-zero `posted_start_time', but they are
     assigned a `target_duration', as described below.
        The `target_duration' and `target_end_time' members are also used
     with timed requests, but they are not set until the request is issued.
     The `kdc_cid' object, which represents a JPIP channel, maintains a
     `last_target_end_time' state variable.  When a timed request is issued,
     the `target_duration' of the timed request is assigned and added to
     `last_target_end_time' -- the new value of `last_target_end_time' is
     then written to the request's `target_end_time' member.
     When the first timed request in an uninterrupted sequence (as defined
     above) is sent, `kdc_cid::last_target_end_time' is initialized to
     the same value as `nominal_start_time', before adding the
     `target_duration'.  Moreover, when the first chunk of return
     data arrives for this request, the `kdc_cid::last_target_end_time'
     value and the `target_end_time' members of all in-flight requests are
     adjusted by the same amount as the `nominal_start_time' members, as
     described above.  The client monitors the times at which individual
     requests complete (evidenced either by the receipt of their EOR message
     or the arrival of a chunk of data for the ensuing request).  When this
     happens, `target_end_time' is reset to -1 (useful for UDP transports
     where chunks might arrive out of order) and an internal record of the
     "disparity" between the actual and target end times is updated.
        If the server and channel realize the expected return data rate
     exactly, there should be no disparity.  However, in the real world,
     we expect there to be a disparity which is likely to exhibit the
     statistics of a classical "random walk".  In order to reduce disparity,
     an adjustment, recorded here as `disparity_compensation', is determined
     at the point where the request is issed; this disparity compensation
     is added to the `target_duration' before using the expected channel
     data rate to determine a byte limit for the request.  The `kdc_cid'
     object keeps track of the cumulative disparity compensation associated
     with requests that have been issued but whose timing disparity has not
     yet been observed through the arrival of the last chunk of response data
     (as explained above).  This "outstanding disparity compensation" is
     used together with the cumulative target duration of outstanding
     requests and the most recently observed disparity between actual and
     target request end times, to determine disparity compensation values
     for future requests.
        The `target_duration' member is initialized by
     `kdc_cid::find_next_requester'.  When there is only one request queue
     that is posting requests, the `target_duration' will be equal to
     `posted_service_time'.  To understand how `target_duration' is assigned
     when there are multiple queues, you should consult the discussions at
     the end of `kdc_cid' and `kdc_request_queue'.
  */

/*****************************************************************************/
/*                             kdc_flow_regulator                            */
/*****************************************************************************/

class kdc_flow_regulator {
  public: // Member functions
    kdc_flow_regulator()
      { disjoint_requests = false; min_request_byte_limit = 256;
        cumulative_chunk_bytes = cumulative_chunk_usecs = 0;
        estimated_rate = 0.002; // 0.002 bytes/usec = 2kB/s
        last_request_time=last_chunk_time=0; potential_pause=true;
        last_request_gap = last_arrival_gap = -1; last_initial_chunk_bytes = 0;
        next_Omax_state = 0; current_Lmax_value = 1000.0;
      }
    void set_disjoint_requests(bool disjoint)
      { this->disjoint_requests = disjoint; }
      /* This function determines whether or not requests are supposed to
         be disjoint, meaning that a new request will not be sent until the
         complete response to an existing request has been received.  This
         happens when communication is stateless, because cache model
         statements for a new request cannot be correctly formulated until
         all response data from the previous request has been received.
         In the disjoint request mode, the value returned by
         `get_Lmax_request_limit' is evaluated differently and typically
         produces larger values.  The disjoint request status can be
         changed at any time. */
    void chunk_received(int chunk_length, kdu_long request_issue_time,
                        kdu_long chunk_received_time,
                        bool first_in_request, bool have_more_requests,
                        bool request_has_byte_limit, kdu_long request_rtt);
      /* This function provides the basis for all timing information.  It
         is called each time a chunk of data is received on the relevant
         CID.  The number of bytes in the chunk is supplied along with
         two times, both measured in microseconds relative to the point at
         which the relevant timer was started.  The `request_issue_time'
         is the time at which the request associated with this chunk was
         issued.  The `chunk_received_time' is the time at which the last
         byte of the data chunk was received.
         [//]
         The `first_in_request' flag is true if this is the first data chunk
         to be received for a request.  The `have_more_requests' flag is
         true if there are any requests beyond the one to which the data
         chunk belongs, which have either been sent or are waiting to be
         sent.  If a call arrives with `first_in_request' true and the
         previous call had `have_more_requests' false, the two requests
         are considered to have an intervening "pause" condition, as
         discussed in the notes below.
         [//]
         The `request_has_byte_limit' flag is true if the request to which
         this chunk belongs was issued with a byte limit.  If not,
         inter-request and inter-arrival gaps should not be used to adapt
         the internal `current_Lmax_value' estimate, because the Lmax value
         has no impact on the way in which requests are generated, so there
         is no closed feedback loop that can be used to impart meaning to
         the inter-request and inter-arrival gaps.
         [//]
         Regardless of whether `request_byte_limit' is true or false, the
         internal `current_Lmax_value' estimate is adjusted, if required,
         to satisfy the following bounds:
         1) `current_Lmax_value'/Ravg <= max{Tmax,1.5*`request_rtt'} 
         2) `current_Lmax_value'/Ravg >= max{Tmin,`request_rtt'}
         Here, Ravg is the current estimate of the return channel's data
         rate, measured in bytes per microsecond, Tmax is an upper bound
         on the time window associated with outstanding requests, which
         currently defaults to 5000000 us (5 seconds) and Tmin currently
         defaults to 500000 us (0.5 seconds).  The `request_rtt' value is
         the caller's current estimate of the typical round trip time
         between the sending of a request and the receipt of the first
         byte of response data from that request.
         [//]
         This function is intended to work even if data chunks arrive out of
         order (could happen with UDP transports).  For this reason, we
         do not try to identify the last chunk associated with a request,
         nor do we insist that the first chunk received for a request is
         the very first chunk that should have arrived in response to that
         request. */
    void note_idle() { potential_pause = true; }
      /* This function is called if it is found that all request queues for
         a CID have gone idle (no requests to send and no more response data
         to receive).  This information is normally redundant, since the
         `have_more_requests' argument should have been false in the last call
         to `chunk_received' (should have been the last chunk for the last
         request before the CID became idle).  However, it may happen that
         when the last chunk of data was received, there were additional
         requests to send, but these requests were subsequently found to be
         unnecessary (due to the completion of the earlier request) and were
         thus discarded.  To avoid a situation in which the transport appears
         to be very slow yet not paused, this function is provided. */
    int get_Omax_request_threshold()
      { 
        if (disjoint_requests) return 0;
        int val = (1 + (int) current_Lmax_value) * (3 + 2*next_Omax_state);
        return (val+4) >> 3; // Leaves either 3/8 Lmax or 5/8 Lmax
      }
      /* Returns the threshold Omax, that should be used to determine
         the earliest point at which a new byte limited request is issued
         to the server.  Omax represents the maximum number of byte from
         previous byte limited requests that should be outstanding (not yet
         received) before a new request is issued.  For more on Omax, see
         the extensive discussion in the notes that appear below. */
    int get_request_byte_limit(int num_outstanding_bytes,
                               bool will_issue_request)
      { 
        assert(min_request_byte_limit > 0); // So we always ask for something
        int limit = ((int) current_Lmax_value) - num_outstanding_bytes;
        if (limit < min_request_byte_limit)
          limit = min_request_byte_limit;
        if (will_issue_request)
          next_Omax_state = 1-next_Omax_state;
        return limit;
      }
      /* Returns the maximum number of bytes to ask for in a byte limited
         request, based on the internal channel estimation state and
         the number of outstanding (not yet received) bytes from previous
         requests, as supplied to the function.  If `will_issue_request' is
         true, the function also flips the state variable that is used to
         modulate the thresholds returned by `get_Omax_request_threshold' --
         as explained in the notes below, this introduces jitter into the
         inter-request interval which helps the internal machinery to
         estimate the channel/server characteristics so as to determine good
         byte limits to use in the future. */
    void set_min_request_byte_limit(int val)
      { 
        if (val <= min_request_byte_limit) return;
        min_request_byte_limit = val;
        if (current_Lmax_value < (val*(8.0/3.0)))
          current_Lmax_value = val*(8.0/3.0);
          // NB: The number of outstanding bytes should be no larger than 5/8
          // of Lmax when a new request is issued, meaning that the request's
          // byte limit (as returned by the above function) should be no
          // less than 3/8*Lmax.  By forcing Lmax >= 8/3*`val', we ensure that
          // the requested byte limit is no smaller than `val', so the
          // presence of the lower bound does not interfere with the
          // modulation of request separations.
      }
      /* This function is used to inform the object of any lower bound
         received in a reply paragraph from the server -- a server may
         be indicating that the client is using byte limits that are too
         small for it to respect. */
    int estimate_bytes_for_usecs(kdu_long num_usecs)
      { /* Returns an estimate of the number of bytes that could be
           transported over the return data channel within the indicated
           number of microseconds, based on current estimates of the
           channel data rate. */
        return (int)(0.5 + (estimated_rate * (double) num_usecs));
      }
    kdu_long estimate_usecs_for_bytes(int num_bytes)
      { /* Returns an estimate of the number of microseconds that would
           be occupied by the transport of `num_bytes' of data, based
           on current estimates of the channel data rate. */
        kdu_long result = (kdu_long)(0.5 + num_bytes / estimated_rate);
        return (result <= 0)?1:result;
      }
  //---------------------------------------------------------------------------
  private: // Configuration parameters
    bool disjoint_requests;
    int min_request_byte_limit; // Lower bound (may be server-supplied)
  private: // Rate estimation state machinery
    kdu_long cumulative_chunk_bytes;
    kdu_long cumulative_chunk_usecs;
    double estimated_rate; // Bytes per microsecond
  private: // Record of values passed in last call to `chunk_received'
    kdu_long last_request_time; // value of the `request_issue_time' argument
    kdu_long last_chunk_time; // value of the `chunk_received_time' argument
    bool potential_pause; // true if `have_more_requests' argument was false
  private: // Lmax/Omax estimation machinery
    kdu_long last_request_gap; // Last GR_j value -- -ve if we have none yet
    kdu_long last_arrival_gap; // last GA0_j value -- -ve if we have none yet
    int last_initial_chunk_bytes; // LA0_j value -- 0 if we have none yet
    int next_Omax_state; // 0 means f=3/8, 1 means f=5/8 -- see notes below
    double current_Lmax_value; // Most recently determined `Lmax' value
  };
  /* Notes:
       The purpose of the flow regulator is to help the client manage the
     way in which it issues byte limited requests.  The most immediate
     issue is that when a client is connected to a server via
     the HTTP-only transport, the server has no reliable way to estimate
     channel conditions, so it may send a very large response to a client
     query that clogs up the channel for an indeterminate period of time,
     damaging the responsiveness of an interactive application in which
     a client is likely to issue subsequent pre-emptive requests based on
     new imagery of interest.  To avoid this difficulty, the client
     issues a sequence of byte limited requests, where this object helps
     it to determine appropriate byte limits.  The flow regulator is also
     important for applications in which a client posts timed requests
     (see `post_window'), since these may be implemented through the
     issuing of byte limited requests to the server.  Essentially,
     the flow regulator is involved in all byte limited request generation.
     [//]
     The flow regulator lives within a `kdc_cid' object, which represents
     a JPIP channel.  The JPIP channel has its own transport for return
     data but may share a primary request/response TCP link with other JPIP
     channels (in cases where one of the HTTP-AUX transports is employed).
     For this reason, we need to be careful to base the flow regulator's
     channel estimates on return data, as opposed to response headers that
     arrive on the primary request link.
     [//]
     The information produced by the flow regulator is as follows:
     * Estimates of the average data rate, Ravg for return data.  If requests
       are not being issued in an optimal manner, so that the server sits
       idle for a short time between sending the response to one request
       and receiving a subsequent request, there will be gaps in the data
       chunks received by the client.  These gaps are deliberately not
       factored out by the rate estimation machinery.  On the other hand,
       if the client becomes idle, meaning that the return data from
       existing requests has all been received and there are no new requests
       to be sent, the rate estimation machinery is paused.  The value
       of Rmax is reported indirectly by the `estimate_bytes_for_usecs' and
       `estimate_usecs_for_bytes' functions.
     * Two parameters, Omax and Lmax, that should be used to drive the issuing
       of byte limited requests.  Omax represents the earliest point at which
       a new request can be issued, expressed in terms of the maximum number
       of outstanding bytes (yet to be received) from existing byte limited
       requests.  Lmax represents the maximum number of outstanding bytes
       that should be tolerated for byte limited requests -- a new request's
       byte limit should not exceed Lmax minus the number of outstanding
       bytes from earlier requests.  If the resulting byte limit is
       insufficient to accommodate the entire request, a copy of the request
       is created on the relevant `kdc_request_queue' so that the remainder
       can be requested later on.  You can learn the value of Lmax by
       calling `get_request_byte_limit' with a `num_outstanding_bytes'
       argument of 0.
      [//]
      The algorithm employed by the flow regulator to deduce instantaneous
      values for the Omax and Lmax parameters is as follows.  First, the
      problem is reduced to that of finding Lmax only, by setting
      Omax = f * Lmax, where the value of f is flipped between 3/8 and 5/8,
      each time a request is issued.  This means that successive requests
      typically have quite different inter-request spacing, which is
      important for the determination of a good Lmax value.
      [//]
      The idea behind the selection of Lmax is to choose the smallest value
      we can get away with, while minimizing the likelihood of significant
      gaps between the end of the return data for one request and the start
      of the return data for the next request.  It is not so trivial to
      estimate the inter-request gap, because return data is packetized
      and packets experience a variet of uncertain delays.  We measure only
      the times at which a chunk of return data is received and these
      inter-chunk times are used to estimate the average return data rate,
      Ravg, that is mentioned above.  By increasing Lmax and hence Omax,
      we can reduce the likelihood of gaps, which will tend to increase Ravg.
      On the other hand, increasing Lmax and Omax means that the system
      becomes less responsive to an interactive user and also less able
      to respond effectively to dynamic changes in the channel conditions.
      With these things in mind, the Lmax value is determined as follows:
      1. Let TR_j denote the time at which request j was sent.
      2. Let TE_j denote the time at which the first chunk of data from
         request j was received (E is for "entry").
      3. Let TF_j denote the time at which the final chunk of data from
         request j was received (F is for "final").
      4. Let LE_j denote the number of bytes in the first (entry) chunk to
         arrive in response to request j -- this is the chunk which was
         completely received at time TE_j.
      5. Let GA0_j be the gap between the the last received chunk of data
         from the previous request and the first received chunk of data from
         request j; i.e.,
            GA0_j = TE_j - TF_{j-1}
      6. Let GA_j be the estimated arrival gap between request j and request
         j-1, calculated from:
            GA_j = GA0_j - (LA_j / Ravg)
         If requests are issued early enough, and the statistical variations
         in 1/Ravg are uncorrelated with LA_j, GA_j should have an expected
         value of 0.  Otherwise, if requests are not issued early enough,
         the expected value of GA_j will be greater than 0.  Of course, the
         GA_j values will be considerably affected by delay jitter and
         rate measurement uncertainties so that -ve values of GA_j would
         not be too surprising.
      7. Let GR_j be the request gap (measured in seconds) between the
         sending of request j and that of request j-1, given by:
            GR_j = TR_j - TR_{j-1}
      8. Let Delta_GA be the difference between two successive arrival gaps,
         calculated as:
            Delta_GA = GA_{j-1} - GA_{j-2} =
                       (GA0_j - GA0_{j-1}) - (LA_j - LA_{j-1}) / Ravg
         Note that we compute this all at once at any given point in time,
         using a single value for Ravg, so that changes in Ravg over time do
         not introduce bias into the computation of Delta_GA.
      9. Let Delta_GR be the difference between two successive request gaps,
         calculated as:
            Delta_GR = GR_j - GR_{j-1} = TR_j + TR_{j-2} - 2*TR_{j-1}
      [//]
      When the first chunk of data from request j arrives, we can evaluate
      the values of Delta_GA and Delta_GR above.  If Lmax is so small that
      Omax - f*Lmax results in a transmission gap for all values of f
      (remember that f is modulated from 0.4 to 0.6), we would expect to
      find that Delta_GA / Delta_GR is approximately equal to 1.0, although
      both quantities are very noisy of course.  On the other hand, if Lmax
      is so large that Omax - f*Lmax results in no transmission gap for all
      values of f, we would expect to find that Delta_GA / Delta_GR is
      approximately equal to 0.0.  If transmission gaps occur for the
      smaller value of f but not the larger one, we would expect to find
      that Delta_GA / Delta_GR lies somewhere between 0.0 and 1.0.  Of
      course, in practice, Delta_GA / Delta_GR may take values outside the
      range 0 to 1 due to timing uncertainties.  Moreover, the reliability
      of this ratio is related to the absolute value of Delta_GR (i.e., the\
      degree to which request separation is successfully modulated).
      [//]
      The flow control algorithm updates Lmax according to
          Lmax <- Lmax * F^(|Delta_GR| * [Delta_GA / Delta_GR - mu])
                 = Lmax * F^(sign(Delta_GR) * Delta_GA - mu*|Delta_GR|)
      where F = 1.5 (the ratio between the two values of f that drive Omax)
      and mu = 0.1 is the targeted expected value for the ratio
          Delta_GA / Delta_GR, meaning that we expect to sometimes get
          small transmission gaps -- the mu term provides a small downward
          force on Lmax.
      [//]
      In addition to the above update rule, the flow control algorithm
      explicitly limits Lmax to be no larger than Rmax*TU and no smaller
      than Rmax*TL, where the upper limit TU is set to the larger of
      5 seconds and 1.5 * the estimated round trip time, while the
      lower limit TL is set to the larger of 0.5 seconds and the current
      and the estimated round trip time.  This means that the time taken
      to respond to a new window of interest from the application is
      unlikely to be much less than 0.5 seconds but should not exceed 5
      seconds unless the channel has very high delay.  Within these
      two bounds, the algorithm tries to keep the response time as small
      as possible while achieving a high transmission efficiency (small
      transmission gap).
      [//]
      Evidently, the above update rule requires information from two pairs
      of consecutive requests.  However, these pairs do not themselves need
      to be consecutive -- by consecutive, we mean requests that are not
      separated by a "pause" condition, in which the return data from a
      request completed without any new requests being available to send.
      Each time we receive the first chunk of data from a new request, if
      there was a previous request, not separated by a pause condition, the
      quantities GA0_j, LA_j and GR_j are recorded.  Updates in Lmax can
      occur as soon as we have a second such triplet.  The initial value for
      Lmax is set to something small (e.g., 2000 bytes).  As soon as we have
      an estimate for Ravg this initial value of Lmax may be forced to a
      much larger value by the lower bound of Ravg*0.5 that was mentioned
      above.  This allows us to have a conservative start while rapidly
      escalating Lmax to match the channel's capabilities.  As Lmax reaches
      its optimal value, Ravg also reaches its peak.
  */

/*****************************************************************************/
/*                             kdc_model_manager                             */
/*****************************************************************************/

  // This class manages objects which are used to determine and signal
  // cache model information for a single code-stream.
class kdc_model_manager {
  public: // Member functions
    kdc_model_manager()
      { codestream_id=-1; next = NULL; }
    ~kdc_model_manager()
      { if (codestream.exists()) codestream.destroy(); }
  public: // Data
    kdu_long codestream_id;
    kdu_cache aux_cache;
    kdu_codestream codestream;
    kdc_model_manager *next; // For linked list of code-streams
  };

/*****************************************************************************/
/*                               kdc_primary                                 */
/*****************************************************************************/

class kdc_primary : public kdcs_channel_servicer {
  protected: // Destructor may not be invoked directly
    virtual ~kdc_primary()
      {
        if (immediate_server != NULL) delete[] immediate_server;
        if (channel != NULL) delete channel;
      }
  public: // Member functions
    kdc_primary(kdu_client *client)
      { 
        immediate_server=NULL; immediate_port=0;
        channel_connected=false; channel_reconnect_allowed = false;
        channel=NULL; channel_timeout_set = false; using_proxy=false;
        is_persistent=true; keep_alive=false; is_released=false;
        num_http_aux_cids = num_http_only_cids = 0; active_requester=NULL;
        first_active_request = last_active_request = NULL;
        waiting_to_read = false; in_http_body = false;
        waiting_for_chunk_terminator_after_eor = false;
        chunked_transfer = false; chunk_length = 0; total_chunk_bytes = 0;
        this->client = client; next = NULL;
      }
    void release() { release_ref(); }
      /* Call this function in place of the destructor. */
    void set_last_active_request(kdc_request *req);
      /* Appends `req' to the tail of the object's active request list,
         managed by `first_active_request' and `last_active_request'. */
    void remove_active_request(kdc_request *req);
      /* Always use this function to remove `req' from the object's active
         request list, managed by `first_active_request' and
         `last_active_request'.  If this leaves the active request list empty
         and the channel is not persistent, this function also closes the TCP
         channel, but otherwise leaves everything intact.  If the associated
         CID has been newly created in response to a JPIP-cnew response from
         the server, `kdc_cid::assign_ongoing_primary_channel' is called as
         soon as `req' has been removed from the active request list. */
    void resolve_address(kdu_long &current_time);
      /* This function is called to resolve the `immediate_address', if
         `channel' is found to be NULL.  If the address cannot be resolved,
         an error is generated.  If the address resolves to the same address
         as that of an existing primary channel which has no users (no CID's),
         that primary channel is released, after taking control of its TCP
         connection.  Otherwise, a new TCP `channel' is created here, but
         the connection process is not itself initiated. */
    void send_active_request(kdu_long &current_time);
      /* This function is called whenever there is non-NULL `active_requester'
         and the `send_block' is non-empty.  If the immediate server address
         has not yet been resolved, it is done here.  If the channel is not
         yet connected, an attempt is made to connect it here.  If the
         attempt to send the request over a (supposedly) connected channel
         fails and `allow_channel_reconnect' is true, this function initiates
         another connection attempt, but sets `allow_channel_reconnect' to
         false, so that this will not happen again, at least until a reply
         is successfully received on the channel.  If the request is sent
         and `waiting_to_read' is false, the function also initiates the
         process of reading reply/body data so as to ensure that the
         `service_channel' function gets called when data is available for
         receiving. */
    bool read_reply(kdu_long &current_time);
      /* This function is invoked to read HTTP reply text into the
         `recv_block'.  If `in_http_body' is true, this function should not
         be used (but will return false anyway), since the data next to be
         read belongs to an HTTP response body, as opposed to the reply
         paragraph.  The function also return false if the first active CID
         is not expecting a reply.  It returns true if and only if it reads
         a complete reply paragraph.  The `waiting_to_read' member is set to
         true if an attempt is made to read a reply paragraph, but not all the
         required characters are available yet from the channel. */
    bool read_body_chunk(kdu_long &current_time);
      /* This function is invoked to read a single chunk of HTTP response
         data (in chunked transfer mode) or the complete response (for
         non-chunked transfer).  It returns true if it succeeds in reading a
         chunk.  In chunked transfer mode, if the function enters with
         `chunk_length'=0, the function first attempts to read the length
         of the next chunk.  If it encounters the chunked transfer terminator,
         or if it finds that `chunk_length'=0 when the transfer mode is
         not chunked, the function returns true but leaves `in_http_body'
         equal to false.  As with `read_reply', the `waiting_to_read' flag
         is set to true if an attempt is made to read a chunk of data or
         its header/trailer and we fail to get all the way through.
            Note that this function may duplicate the current request
         so as to implement the HTTP-only flow control strategy.  Duplicated
         requests are marked as such so that they are not duplicated again.
         This might otherwise be a risk if the request being duplicated
         is non-preemptive, since duplicates may be inserted ahead of other
         non-preemptive requests. */
    void signal_status(const char *text);
      /* Adjusts the `status_string' of all request queues which use this
         primary channel, then invokes the `client->notifier->notify'
         function. */
    protected: // Implementation of `kdcs_channel_servicer' interface
    virtual void service_channel(kdcs_channel_monitor *monitor,
                                 kdcs_channel *channel, int cond_flags);
      /* This function is invoked by the channel monitor when the `channel'
         object needs to be serviced. */
  public: // Data
    char *immediate_server; // Name or IP address of server or proxy
    kdu_uint16 immediate_port; // Port to use with `immediate_server'
    kdcs_sockaddr immediate_address; // Resolved address from above members
    kdcs_tcp_channel *channel; // NULL if address resolution not yet complete
    bool channel_connected; // True once `channel' is connected
    bool channel_reconnect_allowed; // See `send_active_request'
    bool channel_timeout_set; // If a scheduled timeout has been set already
    bool using_proxy; // True if `immediate_server' is actually a proxy
    bool is_persistent; // True if `channel' persists to the next request
    bool is_released; // Set by `client->release_primary_channel'
    bool keep_alive; // Set if the channel is to be preserved beyond `close'
  public: // Members used to keep track of the channel's users
    int num_http_aux_cids; // Number of HTTP-AUX CID's using me (0, 1, 2, ...)
    int num_http_only_cids; // Number of HTTP-only CID's using me (0 or 1)
    kdc_request_queue *active_requester;
    kdc_request *first_active_request; // See below
    kdc_request *last_active_request; // See below
  public: // Members used to handle requests & responses for active CID's
    bool waiting_to_read; // If channel monitor knows we have more data to read
    bool in_http_body; // If next bytes to be read belong to HTTP response body
    bool waiting_for_chunk_terminator_after_eor; // If body already read in
         // chunked transfer mode, to the point where EOR was received, so we
         // only want to see the chunk terminator.
    bool chunked_transfer; // If body data to be transfered in chunks
    int chunk_length; // Length of next chunk or entire response if not chunked
    int total_chunk_bytes; // Accumulates chunk lengths from single response
    kdcs_message_block query_block;
    kdcs_message_block send_block; // If non-empty, request is still outgoing
    kdcs_message_block recv_block;
  public: // Links
    kdu_client *client;
    kdc_primary *next; // Used to build list of primary HTTP transport channels
  };
  /* Notes:
        This object represents a single HTTP transport, for issuing requests
     and receiving replies and/or response data.  Each JPIP channel
     (represented by a `kdc_cid' object) uses a single primary HTTP channel
     (i.e., a single `kdc_primary' object).
        Each JPIP channel which uses the HTTP-only transport has its own
     primary channel.  However, HTTP-TCP and HTTP-UDP transported JPIP
     channels share a common HTTP channel wherever possible.  We refer to
     these collectively as HTTP-AUX transported JPIP channels.
        The `active_requester' member, if non-NULL, points to the request
     queue associated with a currently scheduled request.  This pointer
     remains valid at least until the request has been delivered over the
     HTTP channel.  The `active_requester' member blocks new requests from
     being delivered, so we sometimes leave it non-NULL beyond the point at
     which the request is sent, so as to temporarily prevent unwanted request
     interleaving.  For a new request queue (one with
     `kdc_request_queue::just_started'=true) we always leave the queue as
     the active requester until the reply is received, which allows
     persistence to be determined and also allows for the fact that the
     first reply may assign a new CID for the queue whose transport type
     could affect the policy described above.  If the channel turns out to
     be non-persistent, we leave the `active_requester' non-NULL
     until the HTTP response (reply and any response data) are received.  We
     do the same thing for stateless communications (i.e., where there is no
     channel-ID), since we cannot formulate a comprehensive set of cache
     model statements until we have received all outstanding data.  Of course,
     this makes stateless communication less responsive, but that is why
     JPIP is designed with stateful sessions in mind.
        `first_active_request' points to a list of all requests which have
     been sent, for which the reply (including HTTP response data for
     HTTP-only CID's) is still outstanding.
  */

/*****************************************************************************/
/*                                 kdc_cid                                   */
/*****************************************************************************/

class kdc_cid : public kdcs_channel_servicer {
  protected: // Destructor may not be invoked directly
    virtual ~kdc_cid()
      {
        if (channel_id != NULL) delete[] channel_id;
        if (resource != NULL) delete[] resource;
        if (server != NULL) delete[] server;
        if (aux_tcp_channel != NULL) delete aux_tcp_channel;
        if (aux_udp_channel != NULL) delete aux_udp_channel;
      }
  public: // Member functions
    kdc_cid(kdu_client *client)
      { 
        channel_id = resource = server = NULL;
        request_port = return_port = 0; primary_channel = NULL;
        aux_tcp_channel = NULL; aux_udp_channel = NULL;
        uses_aux_channel = aux_channel_is_udp = aux_channel_connected = false;
        aux_connect_deadline = aux_recv_gate=0;
        aux_min_usecs_per_byte = aux_per_byte_loss_probability = -1.0;
        newly_assigned_by_server = false; channel_close_requested = false;
        is_released = false; next_qid = 1; last_requester = NULL;
        first_active_receiver = last_active_receiver = NULL;
        num_request_queues = 0; have_unsent_ack = false; tcp_chunk_length = 0;
        total_aux_chunk_bytes = 0; have_new_data_since_last_alert=false;
        last_msg_class_id = 0; last_msg_stream_id = 0;
        last_request_had_byte_limit = false;
        last_target_end_time = -1;  target_end_time_disparity = 0;
        outstanding_target_duration = outstanding_disparity_compensation = 0;
        waiting_to_sync_nominal_request_timing = false;
        request_rtt = 500000; // Start out by assuming 500ms
        last_request_time = 0; num_active_requests = 0; last_idle_time = -1;
        this->client = client; next = NULL;
      }  
    void release() { release_ref(); } // Call this instead of destructor
    void set_last_active_receiver(kdc_request *req);
      /* Appends `req' to the tail of the object's active-receiver list
         managed by `first_active_receiver' and `last_active_receiver'. */
    void remove_active_receiver(kdc_request *req);
      /* Convenience function to remove the `req' from the object's
         active receiver list, managed by `first_active_receiver' and
         `last_active_receiver'. */
    int calculate_num_outstanding_bytes();
      /* Returns the total number of bytes in byte limited requests that
         are still active, which have not yet been received. */
    kdc_chunk_gap *find_gaps_to_abandon(kdu_long current_time,
                                        bool abandon_all,
                                        kdc_chunk_gap *append_to=NULL);
      /* This function examines all active request queues associated with
         the CID, to determine if any of them have missing data chunks for
         which negative acknowledgement is in order.  The function appends
         any newly discovered gaps to the existing list headed by
         `append_to' (this is usually NULL) and returns the head of the
         new list.  If data is delivered over a reliable transport, the
         function adds nothing and would not be called with non-NULL
         `append_to', so NULL will be returned.  For unreliable transports,
         the function determines that missing data chunks within a request
         require negative acknowledgement if at least some multiple
         (`KDC_ABANDON_FACTOR') of the typical request-rtt time has passed
         since any data was received for the request.  If missing chunks are
         discovered, they are added to the returned list of missing chunk
         gaps and the requests from which they originate are processed as if
         they have been completed -- that is, their missing chunks are cleared
         and dependencies upon them are removed, but along the way all modified
         requests are marked as `untrusted'.  In this process, quite a few
         active requests may be cleared from their request queues, in this
         and other CID's.
            If the `channel_close_requested' flag is true, this function
         behaves a bit differently.  It is called in this way only once,
         from within `kdc_request_queue::issue_request' once we know that
         the we are going to issue a request that contains the
         `JPIP_FIELD_CHANNEL_CLOSE' request field.  Since that request cannot
         be followed by any subsequent requests within the same channel, we
         must explicitly `abandon_all' data chunks that have not yet arrived.
         If we do not do this, other channels may draw incorrect conclusions
         regarding the meaning of their response data. */
    kdc_request_queue *
      find_next_requester(kdu_long current_time, bool force_new_request);
      /* This function scans for request queues which are using this CID to
         determine one which can transmit next.  The function returns NULL
         if no request queue is ready or allowed to transmit on this CID
         at present.
            If `force_new_request' is true, the function synthesizes a new
         request if none is available, by duplicating the last request
         which was issued by a queue which would otherwise be ready to
         send one.  In practice, `force_new_request' is set to true if we
         have any missing data chunks in the CID for which negative
         acknowledgement was deemed overdue by a recent call to
         `find_gaps_to_abandon'.
            The function may return NULL if the CID already has too many
         outstanding requests in progress.  This is particularly likely for
         CID's with unreliable auxiliary return data channels, since many
         requests can be issued while missing data chunks remain outstanding.
         To avoid sudden loss of responsiveness when a hard limit is reached
         on the number of outstanding requests, the function throttles the
         rate at which requests are accepted, based on the number of
         pending requests.  In particular, noting that `request_rtt' holds
         an estimate of the typical delay between the issuing of a request and
         the receipt of the reply (HTTP-only) or both the reply and first
         JPIP message (HTTP-TCP or HTTP-UDP), and that we will start
         generating explicit abandonment messages to clear the request
         backlog once a request has been stale for more than
         `KDC_ABANDON_FACTOR' * `request_rtt' microseconds, to maintain a
         queue with W incomplete requests, we should space requests
         approximately (`KDC_ABANDON_FACTOR'+1) * request_rtt / W usecs apart.
         Based on this approach, we limit the inter-request interval to at
         least (`KDC_ABANDON_FACTOR'+1)*W*`request_rtt'/(`KDC_WINDOW_TARGET')^2
         usecs, where `KDC_WINDOW_TARGET' is a reasonable (target) limit on
         the number of outstanding requests within any given CID and W is the
         actual number of outstanding requests at the time the request is
         issued.  This way, once W reaches `KDC_WINDOW_TARGET', the window
         size should reach equilibrium.  Typical values are
         `KDC_ABANDON_FACTOR'=3 and `KDC_WINDOW_TARGET'=10.  This throttling
         strategy is applied to all CID's, regardless of whether their
         transport is reliable or unreliable, or even transported over
         auxiliary channels.
            If `last_requester' is non-NULL, the function tries to choose
         a different next requester, if there is one with a request
         available.  It is important that `last_requester' be either NULL
         or point to a valid entry in the `client's `request_queues' list
         since the function searches from the queue immediately following
         the last requester in that list.
            The function takes care of implementing the request scheduling
         rules.  Specifically, a request queue can issue a request if it has
         one available, if the primary channel's `active_requester' member is
         NULL, if all requests from other queues that are using the same
         CID have either received their replies or been issued with a
         byte limit, and either: a) the last issued request had no byte
         limit; or b) the number of outstanding bytes from byte limited
         requests is less than or equal to the threshold returned by
         `flow_regulator.get_Omax_request_threshold'.
            The function does most of the work required to manage timed
         requests.  Specifically: it identifies the point at which the CID
         must enter or leave timed request mode; it re-disributes service
         time that has not been used by idle queues to those that are able
         to use it; and it ensures that timed requests have appropriate
         `kdc_request::target_duration' and `kdc_request::nominal_start_time'
         values before the returned queue is passed to
         `kdc_request_queue::issue_request'.
      */
    void process_return_data(kdcs_message_block &block, kdc_request *req,
                             kdu_long current_time);
      /* Absorbs as much as possible of the return data contained in the
         supplied message block.  The function extracts only data which
         pertains to a single request.  It can happen that `req' is NULL if
         the auxiliary return channel is unreliable -- for example, the
         data in `block' might arise from a duplicate version of some
         data chunk delivered via the HTTP-UDP transport.  Whether `req'
         is NULL or not, the function will not parse beyond an EOR message.
         If it encounters an EOR message and `req' is non-NULL, however,
         the `req->response_terminated' flag is set, along with other flags
         which might be derived from the EOR reason code (e.g.,
         `req->window_done', `req->byte_limit_reached', ...), after which the
         function returns.  Note that there may still be `chunk_gaps'
         associated with `req', so `req->window_done' only means that the
         server has completely generated the response; it does not mean that
         the client has yet received all data chunks.
            If you are using this function to process HTTP or HTTP-UDP
         response data chunks, the message block should end after any EOR
         message is encountered.  In the case of data returned over an
         HTTP-TCP transport's auxiliary channel, each chunk of data might
         possibly contain multiple EOR messages, though.  In this case, the
         function returns after parsing the first EOR message and the caller
         may need to invoke it again on a subsequent `kdc_request' object.
            Along the way, the function updates `req->received_body_bytes' to
         hold the total number of body bytes from all JPIP messages parsed in
         response to the request (except for EOR messages).  It also updates
         `req->received_message_bytes' to hold the total number of message
         bytes (bodies + headers) from all JPIP messages parsed in response
         to the request (except for EOR messages). */
    void assign_ongoing_primary_channel();
      /* This function is called on a CID which was newly assigned by the
         server when it is first removed from its original primary channel's
         active CID list.  The function also disables flow control for
         primary channels which are used with HTTP-TCP/UDP communications.
         The function assumes that the `server_address' has already been
         resolved, if required.  The function does not itself throw
         exceptions or generate calls to `kdu_error'. */
    bool connect_aux_channel(kdu_long &current_time);
      /* Called if the auxiliary channel still needs to be connected.  May
         throw an exception.  If successful, the function sets
         `aux_channel_connected' to true and returns true; returns false if
         the connection cannot yet be completed. */
    bool read_aux_chunk(kdu_long &current_time)
      { 
        if (aux_tcp_channel != NULL)
          return read_tcp_chunk(current_time);
        else if (aux_udp_channel != NULL)
          return read_udp_chunk(current_time);
        else
          return false;
      }
      /* See `read_tcp_chunk' and `read_udp_chunk' for an explanation. */
    void update_request_rtt(kdu_long rtt)
      { /* Called once a request has received both its reply paragraph
           (all transports) and its first auxiliary data chunk (not required
           for HTTP-only transport).  `rtt' is the time since the request
           was issued. */
        request_rtt += (rtt - request_rtt) >> 3;
        if (request_rtt > KDC_MAX_REQUEST_RTT)
          request_rtt = KDC_MAX_REQUEST_RTT;
      }
    void signal_status(const char *text);
      /* Adjusts the `status_string' of all request queues which use this
         CID, then invokes the `client->notifier->notify' function. */
    void alert_app_if_new_data()
      {
        if (!have_new_data_since_last_alert) return;
        client->signal_status();
        have_new_data_since_last_alert = false;
      }
      /* Invokes `client->notifier->notify' if new data has been entered into
         the cache from this CID's `process_return_data' function, since
         this function was last called. */
    bool check_for_more_requests(const kdc_request *req);
      /* Called while `req' is still one of the CID's active receivers, this
         function checks to see if there are any later requests that have
         been issued for the CID or that are ready to be issued. */
    void initialize_request_timing(kdu_long nominal_start_time);
      /* This function is called from within `find_next_requester' if it
         finds that `last_target_end_time' is -ve and there is at least one
         request queue that is in timed request mode.  The function
         initializes the `last_target_end_time' member to `nominal_start_time'.
         It also initializes the `kdc_request_queue::next_nominal_start_time'
         values of all request uses using the CID to `nominal_start_time',
         checking along the way that any queues that already have a
         non-negative `next_nominal_request_time' agree with the value with
         which they are to be initialized.  This function also sets
         `waiting_to_sync_nominal_request_timing' to true. */
    void reset_request_timing();
      /* This function is called from within `find_next_requester' if it finds
         that `last_target_end_time' is non-negative, yet there are no queues
         still in timed request mode.  The function sets `last_target_end_time'
         to -1, resets other state variables that are associated with
         target end time disparity compensation, and also resets all
         `kdc_request_queue::next_nominal_start_time' members to -1. */
    void adjust_request_timing(kdc_request *req, kdu_long target_duration);
      /* Adjusts all the request timing members of the current object and
         its associated request queues to accommodate the issuing of a
         timed request (posted or synthesized) with the indicated target
         duration.  In simple cases `target_duration' is equal to
         `req->target_duration'.  However, the target duration may have been
         reduced to accommodate a prevailing limit on the maximum request
         size; in any event, the function returns with `req->target_duration'
         equal to `target_duration'.  This function augments the
         value of `last_target_end_time' (a.k.a. tC) by `target_duration' and
         adjusts the `kdc_request_queue::next_nominal_start_time' (a.k.a. t_q)
         values of the associated queues, q, so as to guarantee that the
         following fundamental request timing equation is satisfied:
              N*tC = sum_q t_q  -- N is the number of associated queues.
         This is done by adding N'*`target_duration' to the t_q value
         associated with `req->queue', where N' is the number of
         request queues that either have requests to send or else are in
         timed request mode, while the remaining N-N' queues (if any) have
         their t_q values incremented by exactly `target_duration'.  If
         `req->posted_service_time' < 0 or the value of N'*`target_duration'
         differs from `req->posted_service_time' by more than N,
         the request `req' is duplicated, assigning suitable values for
         the duplicate copy's timing parameters. */
    void adjust_timing_after_queue_removed();
      /* This function is called after a queue has been removed from the set
         of queues that are using the current CID.  The function
         need only be called if `last_target_end_time' >= 0 -- the CID
         is in timed request mode.  The purpose of the function is to
         adjust the `kdc_request_queue::next_nominal_start_time' members
         of all queues still associated with the CID so as to ensure that
         the constraint N*tC = sum_q t_q is satisfied, where N is the
         number of queues using the CID, tC is the `last_target_end_time'
         value and t_q are the `kdc_request_queue::next_nominal_start_time'
         values.  The function also recalculates the
         `outstanding_disparity_compensation' value by summing the
         `disparity_compensation' members of all outstanding requests
         that have non-negative `kdc_request::target_end_time' values. */
    void sync_nominal_request_timing(kdu_long delta_usecs);
      /* This function is called if `waiting_to_sync_nominal_request_timing'
         is true when the first chunk of data for a timed request (one with
         `kdc_request->nominal_start_time' >= 0) arrives.  The function
         resets `waiting_to_sync_nominal_request_timing' to false and adds
         `delta_usecs' to the nominal start times and other associated timing
         parameters of all timed requests associated with the CID and all
         queues that are using the CID; the `last_target_end_time' member
         is also adjusted by `delta_usecs'.  This behaviour is explained
         further in the notes that appear below this class declaration. */
    void wake_from_idle(kdu_long current_time);
      /* This function is called from `kdu_client::post_window' if
         a new request is added to one of the CID's queues and
         `last_idle_time' was non-negative.  If `current_time' is -ve on
         entry, it is evaluated from scratch -- the only reason for providing
         the current time is to save an unnecessary call to
         `client->timer->get_ellapsed_microseconds'.  The function resets
         `last_idle_time' to -1, but first compares the current time with
         the last idle time to estimate the amount of lost service time.
         If `last_target_end_time' is non-negative, this lost service time
         is added to `last_target_end_time' and to the
         `next_nominal_start_time' member of each of the CID's request
         queues. */
    void reconcile_timed_request(kdc_request *req, kdu_long actual_end_time);
      /* Called when the last chunk of a request (the one containing EOR) is
         received, or when the first chunk of any subsequent request is
         received, where `req->target_end_time' is positive (i.e., where
         `req' was a timed request).
            The purpose of the function is to compare `actual_end_time' with
         `req->target_end_time' and also to adjust the
         `outstanding_disparity_compensation' and `outstanding_target_duration'
         state variables to account for the fact that `req' is no longer
         considered to be an outstanding request.
            If `req' was a timed request (`req->last_target_end_time' >= 0)
         and `req->window_completed' or `req->quality_limit_reached' is true,
         the termination of response data may not have anything to
         do with a requested service time or byte limit expiring, so in this
         case the function makes compensating adustments to the state
         variables used to manage timed requests. */
  protected: // Implementation of `kdcs_channel_servicer' interface
    virtual void service_channel(kdcs_channel_monitor *monitor,
                                 kdcs_channel *channel, int cond_flags);
      /* This function is invoked by the channel monitor when the
         `aux_channel' needs to be serviced in one way or another. */
  private: // Helper functions
    bool read_tcp_chunk(kdu_long &current_time);
      /* Reads a single response data chunk from the auxiliary TCP channel,
         processes its contents and sends the corresponding acknowledgement
         message.  If all of this succeeds, the function returns true.
         Otherwise it returns false and leaves at least one network condition
         monitoring event behind which will eventually result in a call to
         `service_channel' re-invoking this function.  You should always call
         this function repeatedly until it returns false or throws an
         exception.  If an exception is thrown, the current object should
         generally be released by calling `kdu_client::release_cid'. */
    bool read_udp_chunk(kdu_long &current_time);
      /* Similar to `read_udp_chunk', except that response data chunks are
         read from the auxiliary UDP channel.  Since UDP is unreliable and
         unordered, the implementation in this case is more complex.  On
         the other hand, UDP data chunks are required to relate to exactly
         one request, which provides some simplification. */
  public: // Basic communication members
    char *channel_id; // NULL if "CID" is stateless (at least so far)
    char *resource; // Name of resource to be used in requests
    char *server; // Name or IP address of server to use for next request
    kdu_uint16 request_port; // Port associated with `server'
    kdu_uint16 return_port; // Port associated with aux return channel
    kdcs_sockaddr server_address; // Used to store any resolved address
    kdc_primary *primary_channel; // We bind each CID to a single HTTP channel
    kdcs_tcp_channel *aux_tcp_channel; // Used with HTTP-TCP transport
    kdcs_udp_channel *aux_udp_channel; // Used with HTTP-UDP transport
    bool uses_aux_channel; // True as soon as HTTP-TCP/UDP transport identified
    bool aux_channel_is_udp; // Ignored unless `uses_aux_channel' is true
    bool aux_channel_connected; // True once `aux_channel' has been connected
    kdu_long aux_connect_deadline; // 0 until 1st call to `connect_aux_channel'
    kdu_long aux_recv_gate; // Used to implement client-side rate throttling
    double aux_min_usecs_per_byte; // > 0 if there is any recv rate throttling
    double aux_per_byte_loss_probability; // > 0 if packet loss to be simulated
    bool newly_assigned_by_server; // See below
    bool channel_close_requested; // If a cclose request field has been issued
    bool is_released; // Set by `client->release_cid'
    kdu_long next_qid; // Request-id value to use with next request
    kdu_window_prefs prefs;
  public: // Request queue association members
    int num_request_queues; // Number of request queues using this CID
    kdc_request_queue *last_requester; // See below
    kdc_request *first_active_receiver; // See below
    kdc_request *last_active_receiver; // See below
  public: // Members used to buffer auxiliary channel data
    kdcs_message_block aux_recv_block; // Used to receive data chunks
    kdu_byte ack_buf[8];
    bool have_unsent_ack;
    int tcp_chunk_length; // 0 if not yet ready to read a TCP chunk
    int total_aux_chunk_bytes; // Mainly for debugging purposes
  public: // Message decoding state
    bool have_new_data_since_last_alert; // Used by `alert_app_if_new_data'
    int last_msg_class_id;
    kdu_long last_msg_stream_id;
  public: // Members used to throttle/size requests
    kdc_flow_regulator flow_regulator; // See below
    bool last_request_had_byte_limit; // See below
    kdu_long last_target_end_time; // To set request target_end_time; see below
    bool waiting_to_sync_nominal_request_timing; // See below
    kdu_long target_end_time_disparity; // Actual minus target; see below
    kdu_long outstanding_target_duration; // See below
    kdu_long outstanding_disparity_compensation; // See below
  public: // Additional channel estimation members
    kdu_long request_rtt; // Average time from request to reply & 1st JPIP msg
    kdu_long last_request_time; // Absolute time last request issued (in usecs)
    int num_active_requests; // Requests issued but not complete
    kdu_long last_idle_time; // -1 if there are active or unrequested requests
  public: // Links
    kdu_client *client;
    kdc_cid *next; // Used to build a list of JPIP channels
  };
  /* Notes:
        CID objects are used to represent JPIP channels (the name comes from
     the fact that JPIP channels are identified by a `cid' query field in
     the JPIP request syntax).  There is exactly one CID for each channel
     assigned by the server, but for stateless communications, we use a
     single `kdc_cid' object with a NULL `channel_id' member.  All JPIP
     communication starts stateless, but may become stateful if the server
     grants a `cnew' request, in which case the `channel_id' member becomes
     non-NULL and stores the server-assigned unique Channel-ID.
        All CID's are associated with a primary HTTP transport, over which
     requests are delivered and replies received.  As already discussed under
     `kdc_primary', HTTP-only CID's (those where `uses_aux_channel' is false)
     each have their own primary HTTP channel (i.e., `primary_channel' points
     to a unique channel for each such CID).  HTTP-AUX transported CID's,
     however, use the `aux_tcp_channel' or `aux_udp_channel' to receive
     response data.  These CID's share a common primary HTTP transport wherever
     they can (i.e., wherever the IP address and port assigned for the primary
     HTTP communications by the server are consistent), so as to conserve
     resources.  There are no real efficiency benefits to separating the
     HTTP channels used by HTTP-TCP/UDP transported JPIP channels.
        The `first_active_receiver' and `last_active_receiver' members
     keep track of all requests that have at least begun to be issued, but
     have not yet received all replies and response data.
        The `last_requester' member points to the request queue which most
     recently issued a request.  This member is set by the
     `kdc_request_queue::issue_request' function, which also appends the
     relevant request to the end of the active receiver list.
        The `newly_assigned_by_server' flag is true if this CID was created
     in response to a JPIP-cnew response from the server.  In this case,
     communication over the new CID proceeds using the primary channel on
     which the JPIP-cnew response was received, but only for the request.  No
     new requests are accepted over the CID until the primary channel
     connection details can be verified (possibly reassigned), which takes
     place when the CID is first removed as the active-CID on its original
     primary channel.  The `kdc_primary::remove_active_cid' watches out for
     this condition and calls `assign_ongoing_primary_channel'.
        The `flow_regulator' is used to estimate the channel/server
     behaviour and to determine appropriate byte limits and request times
     for cases in which a sequence of non-preemptive requests need to be
     issued.  The `flow_regulator' is always used for requests sent over
     the HTTP-only transport (with the sole exception of one-shot requests).
     This is necessary, because the server is not in a position to
     estimate channel conditions or regulate the flow of traffic, so
     requests posted to the client must be broken down into a sequence of
     smaller requests, each with a byte limit, that is determined in such
     a ways as to avoid clogging the channel with responses to past
     requests, which would damage responsiveness to new requests that may
     need to pre-empt existing ones.  The `flow_regulator' is also used to
     implement timed requests -- see `kdu_client::post_window'; in this case,
     the `kdc_request::target_duration' values are converted to byte limits;
     the `flow_regulator' provides part of the machinery required to do this
     well, supplying dynamic estimates of the channel data rate as well as
     information about the point at which a new request should be issued and
     the maximum number of bytes that should be requested at once (if a
     timed request would be too large, the request is automatically split
     into smaller ones).
        `last_request_had_byte_limit' is true if the most recently issued
     request had a non-zero `kdc_request::byte_limit' field.  Since byte
     limits are always synthesized internally (i.e., not part of a
     `kdu_window' that might be posted by the application), they are designed
     to keep the server responsive and are not intended to be pre-empted.
     Accordingly, the next request after one that is issued with a byte
     limit, should specify "wait=yes" even if it is preemptive, except where
     the request is not issued within a session (no channel-id).
        The role of the `last_target_end_time' member is discussed briefly
     in the notes following `kdc_request'.  This member and the
     four which follow are only used with timed requests -- either as
     posted or, if necessary, synthesized.  All times are expressed in
     microseconds, relative to the point at which the client was constructed.
     The `last_target_end_time' member holds a -ve value when not in use.
     As soon as a timed request (one with non-zero target duration) is
     encountered in a call to `kdc_request_queue::issue_request', if
     `last_target_end_time' is -ve, it is initialized to the same value as
     the request's `kdc_request::nominal_start_time' member and the
     `waiting_to_sync_nominal_request_timing' flag is set.  After such
     initialization (if necessary), `kdc_request_queue::issue_request'
     increments `last_target_end_time' by `kdc_request::target_duration'
     and the updated value is also written to `kdc_request::target_end_time'.
     When the first data chunk of a timed request is received, if
     `waiting_to_sync_nominal_request_timing' is true, the
     `sync_nominal_request_timing' function is called, whose purpose is to
     compensate for erroneously guessing the `kdc_request::nominal_start_time'
     value for the first timed request that was posted.  The compensation
     must be applied to `last_target_end_time', as well as all of the
     `kdc_request::nominal_start_time' and `kdc_request::target_end_time'
     values that have been set so far, along with the associated queue
     state variables `kdc_request_queue::next_posted_start_time' and
     `kdc_request_queue::next_attributable_start_time'.
        The `last_target_end_time' value is reset to -1 by the
     `reset_request_timing' function, which is called when
     `find_next_requester' determines that none of the request queues
     associated with this CID are in the timed request mode -- see
     `kdc_request_queue::timed_request_mode'.  The `reset_request_timing'
     function simultaneously resets the `target_end_time_disparity',
     `outstanding_target_duration', `outstanding_disparity_compensation' and
     `waiting_to_sync_nominal_request_timing' members.
        The role of the `last_idle_time' member is worth noting here.  If
     this member is non-negative, the JPIP channel is idle, meaning that
     there are no outstanding requests and no requests are available to be
     posted.  If any of the request queues is still in timed request mode
     when this happens -- i.e., a timed request sequence has not been
     cancelled (see `kdu_client::post_window') -- the time spent idle must
     eventually be attributed to these request queues as unused service time.
     This is done by incrementing their `next_nominal_start_time' member by
     an estimate of the amount of lost channel time, at the point when the
     next timed request becomes available for posting (unless the queues
     all cancel their timed request sequence first).  We estimate the
     amount of lost channel time at at time T by forming the difference
     between T and `last_idle_time' and then adding `request_rtt'.
  */

/*****************************************************************************/
/*                             kdc_request_queue                             */
/*****************************************************************************/

struct kdc_request_queue {
  public: // Member functions
    kdc_request_queue(kdu_client *client)
      { 
        queue_id = 0; cid=NULL;
        request_head = request_tail = first_incomplete = NULL;
        first_unreplied = first_unrequested = NULL;
        unreliable_transport = false;
        just_started = is_idle = true; close_when_idle = false;
        disconnect_timeout_usecs = 0;
        status_string = "Request queue created"; received_bytes = 0;
        queue_start_time_usecs = last_start_time_usecs = -1; active_usecs = 0;
        next_posted_start_time = next_nominal_start_time = -1;
        last_noted_target_duration = -1;
        cum_external_service_usecs = cum_internal_service_usecs = 0;
        sync_base_external = sync_base_internal = 0;
        sync_span_external = sync_span_internal = -1;
        this->client = client; next = NULL;
      }
    kdc_request *add_request(kdu_long current_time);
      /* Appends a new request to the internal queue, leaving the caller
         to fill out the window parameters and the `new_elements' flag.
         A side-effect of this function, is that it leaves `is_idle'=false.
         The function may need to call `cid->wake_from_idle', in which
         case it passes along the `current_time' argument to that function.
         If the current time is not known when this function is called, a
         -ve value should be supplied here, which indicates that the
         current time should be evaluated from scratch if it turns out to
         be needed. */
    kdc_request *duplicate_request(kdc_request *req,
                                   bool force_duplication=false);
      /* Makes a copy of the request on the queue.  It is the caller's
         responsibility to ensure that `req' is already on the queue in
         question.  The new request is inserted immediately after `req'.
         The function returns the duplicated request, which will be NULL
         only if `force_duplication' was false and the `close_when_idle'
         flag is asserted, or if `req' does not belong to this request
         queue (an error condition really).  Note that request duplication
         is relatively harmless, since if a requested window of interest
         comes back marked as completely service, duplicates of the request
         will be removed.
       */
    void remove_request(kdc_request *req);
      /* Can be used to remove any request in the queue and rearrange things
         as required.  By the time this function is called, the request
         being removed should have either received its response in full or
         else it should not have any other requests which are dependent on
         it, unless the entire client session is being terminated. */
    void received_first_request_chunk(kdc_request *req,
                                      kdu_long start_time, kdu_long end_time)
      { 
        if (req->copy_src != NULL)
          req->received_service_time = req->copy_src->received_service_time;
        if (cid->waiting_to_sync_nominal_request_timing &&
            (req->nominal_start_time >= 0))
          cid->sync_nominal_request_timing(start_time-req->nominal_start_time);
        for (kdc_request *scn=cid->first_active_receiver; scn != req;
             scn=scn->cid_next_receiver)
          if (scn->target_end_time > 0)
            cid->reconcile_timed_request(req,end_time);
      }
      /* Called when the first chunk of return data is received for a request.
         The `req->chunk_received' member should have been set to true
         already by the caller.  This function does the following:
         1) transfers accumulated service time from any `req->copy_src'
         request to `req'; 2) invokes `cid->sync_nominal_request_timing' if
         the CID is waiting to be synced; and 3) passes through all preceding
         incomplete requests to perform `cid->reconcile_timed_request'
         as required -- this last step is relevant only where chunks arrive
         out of order; it is normally performed by
         `kdc_request::set_response_terminated'. */
    void request_comms_completed(kdc_request *req, bool force_untrusted=false);
      /* This function is called once `req->communication_complete' returns
         true, which can happen after a reply paragraph or response
         data is received, or when missing chunks are being abandoned and a
         negative acknowledgement request is being formulated to account for
         them. In the last case, the `force_untrusted' argument is set to true
         causing the function to mark all outstanding requests issued in
         other CID's and all subsequent outstanding requests in this CID as
         `untrusted' -- once the `kdu_client::obliterating_request_issued'
         function has been called, newly issued requests will continue to
         be marked as `untrusted' until the reply to the negative
         acknowledgement request is received.  The function removes `req'
         from the `dependencies' list of any later request within the same
         queue.  It also removes `req' from the `dependencies' list of any
         request in any other queue.  Along the way, the
         `process_completed_requests' function is invoked where requests
         (including `req') are found to be fully complete. */
    void process_completed_requests();
      /* This function is called when a request on the list headed by
         `first_incomplete' might potentially now be complete.  The function
         scans all issued requests, looking for any which are now complete.
         To ensure that `kdu_client::get_window_in_progress' behaves as
         expected, the last request to which the client has received a server
         reply paragraph is always left on the queue and the order of requests
         on the queue is not modified, but intermediate requests may be
         removed.
            A request is considered to be complete once all communication
         associated with the request has been completed (reply received and
         response data fully received) and all `dependencies' have either
         been satisfied or been rendered superfluous -- dependencies are
         superfluous if the request has already become `untrusted' or an EOR
         code has been received which does not provide any useful information
         regarding the state of the service.
            The function potentially adjusts the `is_idle' and `status_string'
         members and may adjust `client->image_done' and/or
         `client->session_limit_reached' if flags of the same name are found
         to be true within completed requests which can be trusted.
            The function checks whether or not any as-yet unsent requests
         are made redundant by a new completed request, removing them
         if required -- except that the final request of a queue, whose
         `close_when_idle' flag is set, is never removed (this is the one
         on which the "cclose" message is sent).
            If this was a stateless request, the function updates the set
         of model managers, so that there is one for each code-stream for
         which we have received a main header. */
    bool have_active_requests()
      { 
        kdc_request *scan;
        for (scan=first_incomplete; scan != first_unrequested; scan=scan->next)
          if (!scan->communication_complete())
            return true;
        return false;
      }
      /* Called before deciding whether or not a request queue can be removed
         from its CID's list of active receivers.  The function returns true
         if there is any request in the queue which has been issued (or begun
         to be issued) by a call to `issue_request', but for which all
         communication is not yet complete -- meaning that
         `kdc_request::communication_complete' returns false. */
    void issue_request(kdu_long &current_time,
                       kdc_chunk_gap * &gaps_to_abandon);
      /* Called from the management thread when we are ready for a request
         to be issued.  This function should not be called until
         `cid->find_next_requester' returns the current queue.
             Moreover, the function should not be called until
         `cid->newly_assigned_by_server' is false, since we should not be
         issuing new requests over a CID until its primary
         connection details have been validated and/or reassigned.
            If `gaps_to_abandon' is non-NULL, the issued request includes an
         "abandon" request field which instructs the server to abandon
         transmission of the relevant data chunks (`gaps_to_abandon'
         points to a linked list).  This argument is passed by reference,
         because the function internally augments any `gaps_to_abandon' list
         if an unreliably transported CID is closed by sending the JPIP
         "cclose" request field -- this is necessary because the closed channel
         will not be able to receive further data, so the safest thing to do
         is to abandon all data chunks from previous requests that have not
         yet arrived.
            The function also takes responsibility for adding the queue onto
         its CID's active-receiver list, if it is not already there, since
         a queue must enter the active receiver list from the moment that
         a request is first commenced, even if it takes a while to go out
         through the relevant TCP channel.
            If this function issues a request over a CID which already has an
         incomplete request from one or more other request queues (i.e.,
         request queues other than the current one, which are on the CID's
         active receiver list) and the current request is preemptive, it is
         likely to pre-empt these other request queues.  To conceal this
         from the application, the present function synthesizes a copy of
         each such potentially pre-empted request.  A copy is made if the
         potentially pre-empted request is the final one on its queue or
         if the ensuing request is non-preemptive.  Request copies have
         the same pre-emptability as the source request from which they
         are copied.  Moreover, when a request is duplicated, it is marked
         as having been duplicated so that no attempt is made to duplicate
         it again, either here or inside the `kdc_primary::read_body_chunk'
         function.
            The function does not actually send the request, mainly because
         this might generate an exception that could be hard to back out of
         without killing the entire client connection.  Instead, the
         caller should later scan the primary request channels, looking for
         any which have `kdc_primary::active_requester' non-NULL and
         `kdc_primary::send_block' non-empty, invoking the corresponding
         `kdc_primary::send_active_request' funcion if so.  It is easier
         to catch network-generated exceptions at that point.  If the
         request needs to be re-sent due to channel queue fulness or
         anything like that, it will happen automatically from within
         the `kdc_primary::service_channel' function which is invoked by
         the client's channel monitor object. */
    kdc_request *process_reply(const char *reply, kdu_long &current_time);
      /* This function processes an HTTP reply paragraph recovered from the
         server.  If the paragraph is empty or contains a 100-series return
         code, the function returns NULL, meaning that the actual reply
         paragraph is still expected.  Otherwise, the function processes
         the reply headers, using them to adjust the state of the
         `first_unreplied' entry on the request queue and returning a
         pointer to this entry.  When the function returns non-NULL, the
         `first_unreplied' pointer is advanced and the returned request
         has its `reply_received' entry set to non-NULL.  The function
         parses and sets channel persistence information, window fields,
         new-channel (JPIP-cnew) parameters and any received information
         about the global target-ID.  However, it does not parse
         content-length or transfer encoding information; this is left to
         the caller. */
    void transfer_to_new_cid(kdc_cid *new_cid, kdc_request *req);
      /* This is a delicate function, which is used during the establishment
         of a new JPIP channel to associate the request queue with a new
         CID.  The new CID has exactly the same primary request channel as
         the old one (at least for now).  The function is called
         from within the `process_reply' function, and `req' refers to the
         request whose reply is being processed.  The function transfers
         the request queue to working with the `new_cid', updating all
         appropriate counters and pointers; it removes `req' from the
         old CID's active receiver list and adds it to that of `new_cid'.
            Once done, the queue's previous CID may possibly be left
         with no request queues.  This is possible only if all other request
         queues (there must originally have been at least one) using the CID
         were closed since the call to `kdu_client::add_queue' but before
         the new queue was established -- this is actually quite a likely
         occurrence for some navigation patterns.  In this event, the
         function assigns a temporary request queue for the old CID, which
         is marked with `close_when_inactive' and given a single empty
         request so that it will issue the JPIP `cclose' request.  Otherwise
         we would be left with an orphan request queue on the client side
         as well as the server side, consuming valuable server resources until
         the session is closed. */
    void adjust_active_usecs_on_idle();
      /* This function is called if the queue goes idle.  It may be called
         from the application thread or from the network management thread.
         The function adjusts the `last_start_time_usecs' and `active_usecs'
         members in the current object and, if all queues are idle, the
         main `client' object as well. */
    void set_idle()
      { /* Sets `is_idle' to true and performs other state machinery updates
           that are expected when entering the idle state. */
        is_idle = true;
        adjust_active_usecs_on_idle();
      }
    void signal_status(const char *text)
      {
        this->status_string = text;
        client->signal_status();
      }
    kdu_long find_initial_posted_start_time(kdu_long current_time);
      /* Finds the value that should be used to initialize
         `next_posted_start_time' when it is next needed.  The function
         ignores any existing value of `next_posted_start_time'.  If
         `next_nominal_start_time' is non-negative, that value is
         returned; otherwise, `cid->last_target_end_time' must be -ve
         (i.e., the CID is not yet in timed request mode).  In the latter
         case, the function examines all of the request queues that
         share the same CID, returning the value of any non-negative
         `kdc_request_queue::next_nominal_start_time' member that it can
         find -- any such values should be identical.  If none is found, a
         guess is formed concerning the time at which response data will
         start to come back from a request that is issued at the
         `current_time' -- the guess is, of course, `cid->request_rtt'. */
    void fix_timed_request_discrepancies();
      /* This function is called if the queue's `first_unrequested'
         request exists and has a `posted_service_time' that is positive
         (i.e., the queue is in timed request mode), but is not equal to
         the queue's `next_nominal_start_time' value.  The discrepancy is
         amortized over all timed requests that are still waiting to be sent,
         noting that no request's `posted_service_time' value may be
         reduced below 0.  If a `posted_service_time' is reduced to 0,
         the associated request is discarded.  As a result, this function
         may leave the `unrequested' pointer equal to NULL. */
  public: // Data
    int queue_id; // Identifier presented by `kdu_client' to the application
    kdu_window_prefs prefs; // Maintains service prefs for this queue
    kdc_cid *cid; // Each request queue is associated with one CID
    kdc_request *request_head; // List of all requests on the queue
    kdc_request *request_tail; // Tail of above list
    kdc_request *first_incomplete; // First request on the queue for which a
                 // complete response is not yet available.
    kdc_request *first_unreplied; // First request on the queue for which the
                 // reply has not yet been received.
    kdc_request *first_unrequested; // First request on the queue for which the
                 // process of issuing the request over the CID has not yet
                 // started; this member is advanced by `issue_request', even
                 // though it may take some time for the corresponding call to
                 // `kdc_primary::send_active_request' to completely push the
                 // request out on the relevant primary HTTP channel.
  public: // Status
    bool unreliable_transport; // See below
    bool just_started; // True until the reply to the first request is received
    bool is_idle; // See `kdu_client::is_idle'
    bool close_when_idle; // Set by `kdu_client::disconnect'
    kdu_long disconnect_timeout_usecs; // Absolute timeout, set by `disconnect'
    const char *status_string; // See `kdu_client::get_status'
    kdu_long received_bytes; // See below
    kdu_long queue_start_time_usecs; // Time first request was sent, or -1
    kdu_long last_start_time_usecs; // Time of first request since idle, or -1
    kdu_long active_usecs; // Total non-idle time, excluding any period since
                           // `last_start_time_usecs' became non-negative.
  public: // Management for timed requests
    kdu_long next_posted_start_time; // See below
    kdu_long next_nominal_start_time; // See below
    kdu_long last_noted_target_duration; // See below
  public: // Time base correction for `kdu_client::sync_timing'
    kdu_long cum_external_service_usecs; // Cumulative `service_usecs' values
    kdu_long cum_internal_service_usecs; // posted (ext) and recorded (int)
    kdu_long sync_base_external; // Re-initialized by `sync_timing' whenever
    kdu_long sync_base_internal; // `next_posted_start_time' is -ve
    kdu_long sync_span_external; // Gap between external/internal times noted
    kdu_long sync_span_internal; // by `sync_timing' and external/internal base
  public: // Links
    kdu_client *client;
    kdc_request_queue *next; // For list of all request queues
  };
  /* Notes:
        A request queue is deemed to have an `unreliable_transport' if it
     its `cid' uses an unreliable auxiliary return channel (e.g., UDP, as
     opposed to TCP) or if the request queue is `just_started' and the
     initial request requests a new JPIP channel with a potentially unreliable
     transport type (multiple transport types may be requested, of which only
     one might be unreliable, such as HTTP-UDP).  Requests issued over
     unreliable transports may be entered onto the `kdc_request::dependencies'
     list of requests in the same or other queues (including queues which
     use reliable transports), until such time as the request's response has
     been fully communicated (see `kdc_request::communication_complete').  If
     the transport type for a `just_started' request queue is found to be
     reliable upon receipt of the server's reply paragraph, any such
     dependencies must be removed at that point.
        `received_bytes' is just the queue specific version of
     `kdu_client::total_received_bytes'.  It records all received bytes,
     regardless of whether they are useful or not (e.g., duplicate transmitted
     data chunks).
        A request queue is considered to be in "timed request mode" if
     it has a non-negative `next_posted_start_time' member.  The mode is
     entered when a call to `kdu_client::window' supplies a positive
     `service_usecs' value. The mode is exited only once a request with
     `service_usecs' <= 0 is received -- the mode is not automatically
     exited when the request queue becomes idle.  The `next_posted_start_time'
     member holds the value for `kdc_request::nominal_start_time' that
     should be set the next time a timed request is posted, whereupon the
     `next_posted_start_time' value is augmented by the request's
     `kdc_request::posted_service_time' value.  However, the
     `next_nominal_start_time' member may also have an important
     influence, as described below.
        The `next_nominal_start_time' member takes a meaningful value
     if any request queue that shares the same `cid' is in timed request
     mode.  Whereas `next_posted_start_time' represents the nominal start
     time that should be assigned to the next request posted onto the queue,
     `next_nominal_start_time' represents the nominal start time that
     should be associated with the next request issued from this queue
     onto the JPIP channel.  One way or another, by the time a request
     propagates from the tail of the request queue to the point
     at which it is about to result in an issued request, its
     `kdc_request_queue::nominal_start_time' value must be reconciled with
     `next_nominal_start_time'.  Let t_q denote the value of the
     `next_nominal_start_time' member for request queue q and suppose that
     there are N queues associated with a single `cid', with indices q=1
     through q=N.  Also, for convenience, let tC denote the value of the
     `cid->last_target_end_time' member.  Whenever tC >= 0 (i.e., when
     any request queue is in timed request mode and has issued a timed
     request), the t_q values are guaranteed to satisfy
     N*tC = sum_{1<=q<=N} t_q.  Equivalently, tC is the average of the
     t_q values, for all of the CID's queues, regardless of which of them is
     in timed request mode and which of them happens to have a request that
     can be sent.  When a timed request is issued from queue j, its
     `kdc_request::target_duration' value is added to tC.  This is
     compensated by adding N' * `kdc_request::target_duration' to t_j,
     where N' is the number of request queues that are either in timed
     request mode or else at least have a request to send; the
     `kdc_request::target_duration' value is added to the t_q values of
     the other N-N' request queues (if any).  With this in mind, the
     `kdc_request::target_duration' value for an issued timed request is
     set to `kdc_request::posted_service_time' / N'.  In this way, each
     queue's t_q value should continue to line up with the nominal start
     time of its next timed request.
        The `next_nominal_start_time' member becomes particularly important
     if the application fails to post new requests to its queues that are
     in the timed request mode, by the point at which they would be ready
     to issue.  Normally, the queue that is selected to issue the next
     request is the one with the smallest `next_nominal_start_time', that
     also has a request.  To prevent a queue from getting too far behind
     the others (storing up bandwidth to steal from the other queues in
     the future), we limit the amount by which t_q can precede tC.  For
     queues that are in timed request mode, tC-t_q is limited to at most
     `cid->request_rtt' if there are no requests to send; for other queues,
     tC-t_q is constrained to be <= 0 if there are no requests to send.
     To satisfy these contraints t_q is increased as required, compensating
     for the increase by decreasing the t_j values of those queues j
     that do have requests to send.  In this process, discrepancies are
     introduced between a queue's `next_nominal_start_time' and the
     `kdc_request::nominal_start_time' value associated with its next
     timed request (if it is in timed request mode).  Discrepancies of this
     nature are also created when a timed request is completed by the
     server (window complete) before its requested service time (or byte
     count) is used up.  In that case, the unused service time is returned
     to the CID's `last_target_end_time' member, tC, and adjustments are
     made to the t_q values of all the CID's queues, so that the condition
     N*tC = sum_{1<=q<=N} t_q remains valid.  Finally, discrepancies between
     `next_nominal_start_time' and `kdc_request::nominal_start_time' are
     introduced if the physical JPIP channel goes idle, after which the
     idle time must be attributed to queues as wasted service time,
     increasing their t_q values.
        The discrepancies mentioned above, between the t_q value of a
     request queue that is in timed request mode and its
     `next_nominal_start_time' member, are handled by adjusting the
     `kdc_request::nominal_start_time' values of any requests that are
     on the queue, along with the `kdc_request::posted_service_time'
     values, so that the discrepancy is distributed amongst these requests.
     This may lead to some requests being discarded, because their
     posted service time would become non-positive; it may lead to
     other requests being granted more service time.  However, no matter
     what happens, these changes do not have any impact upon
     the queue's `next_posted_start_time' and the condition
     N*tC = sum_{1<=q<=N} is never violated, so long as the CID has
     any queues that are in timed request mode.
  */

#endif // CLIENT_LOCAL_H
