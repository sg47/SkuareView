/*****************************************************************************/
// File: encoding_local.h [scope = CORESYS/CODING]
// Version: Kakadu, V7.2.1
// Author: David Taubman
// Last Revised: 28 March, 2013
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
   Defines the internal classes that are used to implement the capabilities
of `kdu_encoder'.
******************************************************************************/

#ifndef ENCODING_LOCAL_H
#define ENCODING_LOCAL_H

#include <assert.h>
#include "kdu_compressed.h"
#include "kdu_sample_processing.h"
#include "kdu_roi_processing.h"

// Defined here:
class kd_encoder_job;
struct kd_encoder_push_state;
struct kd_encoder_sync_state;
class kd_encoder;

typedef kdu_int32 (*kd_block_quant_rev16_func)
                    (kdu_int32 *dst, kdu_int16 **src_refs, int src_offset,
                     int width, int height, int K_max);
typedef kdu_int32 (*kd_block_quant_rev32_func)
                    (kdu_int32 *dst, kdu_int32 **src_refs, int src_offset,
                     int width, int height, int K_max);
typedef kdu_int32 (*kd_block_quant_irrev16_func)
                    (kdu_int32 *dst, kdu_int16 **src_refs, int src_offset,
                     int width, int height, int K_max, float delta);
typedef kdu_int32 (*kd_block_quant_irrev32_func)
                    (kdu_int32 *dst, float **src_refs, int src_offset,
                     int width, int height, int K_max, float delta);

// Configure processor-specific compilation options
#if (defined KDU_PENTIUM_MSVC)
#  undef KDU_PENTIUM_MSVC
#  ifndef KDU_X86_INTRINSICS
#    define KDU_X86_INTRINSICS // Use portable intrinsics instead
#  endif
#endif // KDU_PENTIUM_MSVC

#if defined KDU_X86_INTRINSICS
#  define KDU_SIMD_OPTIMIZATIONS
#elif defined KDU_PENTIUM_GCC
#  define KDU_SIMD_OPTIMIZATIONS
#endif

/* ========================================================================= */
/*                          Local Class Definitions                          */
/* ========================================================================= */

/*****************************************************************************/
/*                            kd_encoder_job                                 */
/*****************************************************************************/

class kd_encoder_job : public kdu_thread_job {
  public: // Memory management functions
    static size_t calculate_size(int height, int jobs_in_stripe)
      { /* This function calculates the amount of memory required for a full
           stripe of encoder jobs, sizing the shared `lines16'/`lines32'
           array for the indicated stripe height. */
        size_t len = sizeof(kd_encoder_job);
        len = (len + KDU_MAX_L2_CACHE_LINE-1) & ~(KDU_MAX_L2_CACHE_LINE-1);
        len *= jobs_in_stripe;
        len += sizeof(void **)*(size_t)height;
        len = (len + KDU_MAX_L2_CACHE_LINE-1) & ~(KDU_MAX_L2_CACHE_LINE-1);
        return len;
      }
    size_t init(int height, kd_encoder_job *prev_in_stripe)
      { /* If `prev_in_stripe' is NULL, this function initializes the first
           job in the stripe, allocating also the memory associated with the
           `lines16'/`lines32' array and returning the total amount of
           memory associated with that array and the object itself.
           Otherwise, the function copies the `lines16'/`lines32' reference
           from `prev_in_stripe' and returns just the amount of memory
           associated with the current object.  In both cases, the
           job function is installed, but the caller is left to fill out
           all the other member variables. */
        set_job_func((kdu_thread_job_func) encode_blocks);
        roi8 = NULL; // In case we forget later
        size_t len = sizeof(kd_encoder_job);
        len = (len + KDU_MAX_L2_CACHE_LINE-1) & ~(KDU_MAX_L2_CACHE_LINE-1);
        if (prev_in_stripe != NULL)
          { lines16 = prev_in_stripe->lines16; return len; }
        lines16 = (kdu_sample16 **)(((kdu_byte *) this)+len);
        len += sizeof(void **)*(size_t)height;
        len = (len + KDU_MAX_L2_CACHE_LINE-1) & ~(KDU_MAX_L2_CACHE_LINE-1);
        return len;
      }
  private: // Helper functions
    static void encode_blocks(kd_encoder_job *job, kdu_thread_env *caller);
  private: // Convenience copies of data members from the main `kd_encoder'
    friend class kd_encoder;
    kdu_subband band;
    kd_encoder *owner;
    kdu_block_encoder *block_encoder;
#ifdef KDU_SIMD_OPTIMIZATIONS
    union { // Copy of `kd_encoder::simd_block_quant...'
      kd_block_quant_rev16_func simd_block_quant_rev16;
      kd_block_quant_rev32_func simd_block_quant_rev32;
      kd_block_quant_irrev16_func simd_block_quant_irrev16;
      kd_block_quant_irrev32_func simd_block_quant_irrev32;
    };
#endif // KDU_SIMD_OPTIMIZATIONS
  private: // Parameters common to all jobs
    kdu_int16 K_max; // Max magnitude bit-planes, excluding ROI upshift
    kdu_int16 K_max_prime; // Max magnitude bit-planes, including ROI upshift.
    bool reversible;
    bool using_shorts;
    bool full_block_stripes; // If line buffers always hold multiple of 4 lines
    float delta; // For irreversible compression only
    float msb_wmse; // Normalized weighted MSE associated with first mag bit
    int num_stripes; // So we know by how much to advance vertical block index
  private: // Data members unique to this job
    int which_stripe; // Stripe to which we belong; either 0 or 1.
    int grp_offset; // See below
    int grp_width; // Number of valid samples on each row of the group buffer
    int grp_blocks; // Number of horizontally adjacent code-blocks in group
    kdu_coords first_block_idx; // Absolute index of first code-block in group
  public: // Pointer to shared synchronization variable
    kdu_interlocked_int32 *pending_stripe_jobs; // See below
    union {
      kdu_sample16 **lines16;
      kdu_sample32 **lines32;
    }; // Anonymous union; relevant member depends on `owner->using_shorts'.
  private: // Pointer to information used only for ROI masks -- right now
           // this is not managed as efficiently as it could be.
    float roi_weight; // Multiply `msb_wmse' by this for ROI foreground blocks
    kdu_byte *roi8; // Points to our location within `kd_encoder::roi_buf'
    int roi_row_gap; // Common row gap for ROI lines at the subband level
  };
  /* Notes:
        Each encoding job involves a group of one or more whole code-blocks.
     The actual range of code-blocks associated with this job is determined
     by the `first_block_idx' and `grp_blocks' members.  These blocks are all
     encoded sequentially while performing the job.
     [//]
     The subband samples consumed by block encoding are read from a
     collection of whole-line buffers.  Each stripe of code-blocks maintains
     a single array of pointers to the lines that are associated with
     the stripe.  This shared array is referenced by the `lines16'/`lines32'
     member.  The actual location of the first sample produced by this
     job, within any of these lines, is given by the `grp_offset' member.
     [//]
     The buffers referenced by `lines16'/`lines32' are aligned in such
     a way as to ensure that all code-blocks, except possibly the first one,
     commence at a sample that is at least octet-aligned (i.e., aligned on
     a multiple of 16 bytes if `using_shorts' and 32 bytes otherwise).  With
     this in mind, we can be sure that whenever a code-block's width
     is a multiple of 8, the first sample of the block is octet aligned
     within each of the `lines16'/`lines32' buffers.
     [//]
     Ideally, jobs should have a `grp_width' that corresponds to a whole
     number of assumed L2 cache lines (`KDU_MAX_L2_CACHE_LINE' bytes) and
     the `lines16'/`lines32' buffers are aligned so that the subband samples
     written by each job occupy their own disjoint collection of L2 cache
     lines within these buffers.  However, this may lead to inefficient
     use of memory for smaller images.
     [//]
     `pending_stripe_jobs' points to a synchronization variable
     that is shared by all `kd_encoder_job' objects that belong to the
     same stripe.  This variable is decremented each time a job completes;
     if this leaves the count equal to zero, the `owner->sync_state->sched'
     variable is adjusted, following the policies outlined in the notes
     that accompany `kd_encoder_sync_state' below.  The `pending_stripe_jobs'
     count is reset to `kd_encoder::jobs_per_stripe' only once the `push'
     function fills the stripe with data to be encoded.
  */

/*****************************************************************************/
/*                          kd_encoder_push_state                            */
/*****************************************************************************/

struct kd_encoder_push_state {
  public: // Memory management functions
    static size_t calculate_size(int num_stripes, int stripe_heights[],
                                 size_t job_ptr_mem)
      { /* The function includes space for the stripe job pointers that are
           required to locate `kd_encoder_job' objects, as well as the
           `kdu_thread_job_ref' pointers that are required to keep track of
           bound job references for scheduling -- this amount of
           memory is pre-determined for each stripe as `job_ptr_mem', but
           must still be multiplied by `num_stripes'.  Note that all but
           the last stripe must have exactly the same stripe height. */
        size_t len = sizeof(kd_encoder_push_state);
        int s=num_stripes-1;
        int cum_height = stripe_heights[s];
        for (s--; s >= 0; s--)
          { 
            assert(stripe_heights[s] >= stripe_heights[s+1]);
            assert((s==0) || (stripe_heights[s]==stripe_heights[s-1]));
            cum_height += stripe_heights[s];
          }
        len += sizeof(void*)*(size_t)(cum_height-1);
        len += job_ptr_mem*(size_t)num_stripes;
        len = (len + KDU_MAX_L2_CACHE_LINE-1) & ~(KDU_MAX_L2_CACHE_LINE-1);
        return len;
      }
    void init(int num_stripes, int stripe_heights[], int first_block_height,
              int subband_rows, int blocks_high, int buf_offset)
      { 
        this->num_stripes_in_subband = blocks_high;
        num_stripes_released_to_encoder = last_stripes_requested = 0;
        active_sched_stripe = partial_quanta_remaining = 0;
        active_push_stripe = active_push_line = active_lines_left = 0;
        this->next_stripe_height = first_block_height;
        this->subband_lines_left = subband_rows;
        int s = num_stripes-1;
        this->buffer_height = stripe_heights[s]; // Start with last one
        this->stripe_height = stripe_heights[0];
        assert(stripe_heights[s] <= this->stripe_height);
        for (s--; s >= 0; s--)
          { 
            assert(stripe_heights[s] == this->stripe_height);
            this->buffer_height += stripe_heights[s];
          }
        this->buffer_offset = buf_offset;
        for (int n=0; n < buffer_height; n++)
          lines16[n] = NULL;
        active_roi_line = NULL;
      }
  public: // Data members used only in multi-threaded mode
    int num_stripes_in_subband; // Total number of code-block rows in subband
    int num_stripes_released_to_encoder; // Total number of stripes which
      // have entered the PARTIALLY SCHEDULED or FULLY SCHEDULED state.
    int last_stripes_requested; // For calls to `advance_block_rows_needed'
    int active_sched_stripe; // See below
    int partial_quanta_remaining; // See below
  public: // Main state variables
    int active_push_stripe; // Stripe holding next row to push (0 or 1)
    int active_push_line; // First unpushed line in active push stripe
    int active_lines_left; // Total lines left within the active push stripe
    int next_stripe_height; // Next value to use for `active_lines_left'
    int subband_lines_left; // Lines left in subband, after `active_lines_left'
  public: // Data used for ROI processing
    kdu_byte *active_roi_line; // Updated each time an ROI line is pushed in
  public: // Data members used to manage sample data storage 
    int buffer_height;
    int stripe_height;
    int buffer_offset;
    union {
      kdu_sample16 *lines16[1];
      kdu_sample32 *lines32[1];
    };
  };
  /* Notes:
        This object manages information that is read and written exclusively
     from within calls to `kd_encoder::push' or `kd_encoder::start'.  It
     resides in a block of memory that has its own (assumed) L2 cache
     lines -- taken to have `KDU_MAX_L2_CACHE_LINE' bytes each.
        The `lines16'/`lines32' arrays are part of this object's allocated
     memory block; the actual extent of these arrays is determined by
     the `buffer_height' member.  All `buffer_height' pointers found in
     `lines16'/`lines32' have at least octet alignment (16-byte alignment
     if using short integers, else 32-byte alignment).  However, the first
     valid subband sample within each line is located `buffer_offset'
     samples in, where `buffer_offset' might not be 0.
        Calls to `kd_encoder::push' may involve the exchange of memory
     buffers, as opposed to copying; this is only feasible if `buffer_offset'
     is zero.  If this happens, the contents of the `lines16'/`lines32' array
     actually change with calls to `push', which is why we like to keep this
     array within the present object's own memory block, isolated from the
     memory blocks that are used by the `kd_encoder_job' objects.  The
     `kd_encoder_job::lines16'/`kd_encoder_job::lines32' members point to
     a separate block of memory that holds pointers to the line buffers
     used by encoder jobs.  Those arrays are updated by the present object
     when the active stripe changes.   
        Multiple stripes are used only with multi-threaded processing, in
     which case the number of stripes is often 2 or 3, but values as large
     as 4are currently supported by the definitions contained in this file.
     As explained in the notes that follow the definition of `kd_encoder'
     below, jobs are scheduled in "job quanta", where each stripe may be
     divded into multiple quanta.  The `active_sched_stripe' member
     identifies the index of the next stripe from which jobs are to be
     scheduled, while `partial_quanta_remaining' identifies the number
     of job quanta from the `active_sched_stripe' that have yet to be
     scheduled.  If this value is 0, the stripe indexed by
     `active_sched_stripe' is marked as FULLY SCHEDULED within the
     `kd_encoder_sync_state::sched' synchronization variable. */

/*****************************************************************************/
/*                          kd_encoder_sync_state                            */
/*****************************************************************************/

// The following definitions rely upon the fact that `KD_ENC_QUANTUM_BITS'
// is exactly equal to 2.
#define KD_ENC_QUANTUM_BITS 2

// The following definitions also rely upon the fact that the bit field
// defined by `KD_ENC_SYNC_SCHED_R_MASK' is able to keep a count of all
// threads that might be found inside in-flight jobs.
#if (KDU_MAX_THREADS > 127)
#  error "KDU_MAX_THREADS too big for kdu_encoder implementation!!"
#endif

// SYNC_SCHED bit fields -- see notes below `kd_encoder_sync_state' for details
#define KD_ENC_SYNC_SCHED_S_POS  0
#define KD_ENC_SYNC_SCHED_S0_BIT   ((kdu_int32)(1<<KD_ENC_SYNC_SCHED_S_POS))
#define KD_ENC_SYNC_SCHED_S_MASK   ((kdu_int32)(7<<KD_ENC_SYNC_SCHED_S_POS))
#define KD_ENC_SYNC_SCHED_W_POS  3
#define KD_ENC_SYNC_SCHED_W_BIT    ((kdu_int32)(1<<KD_ENC_SYNC_SCHED_W_POS))
#define KD_ENC_SYNC_SCHED_T_POS  4
#define KD_ENC_SYNC_SCHED_T_BIT    ((kdu_int32)(1<<KD_ENC_SYNC_SCHED_T_POS))
#define KD_ENC_SYNC_SCHED_A_POS  5
#define KD_ENC_SYNC_SCHED_A0_BIT   ((kdu_int32)(1<<KD_ENC_SYNC_SCHED_A_POS))
#define KD_ENC_SYNC_SCHED_A_MASK   ((kdu_int32)(3<<KD_ENC_SYNC_SCHED_A_POS))
#define KD_ENC_SYNC_SCHED_U_POS  7
#define KD_ENC_SYNC_SCHED_U0_BIT   ((kdu_int32)(1<<KD_ENC_SYNC_SCHED_U_POS))
#define KD_ENC_SYNC_SCHED_U_MASK   ((kdu_int32)(255<<KD_ENC_SYNC_SCHED_U_POS))
#define KD_ENC_SYNC_SCHED_Q_POS  15
#define KD_ENC_SYNC_SCHED_Q0_BIT   ((kdu_int32)(1<<KD_ENC_SYNC_SCHED_Q_POS))
#define KD_ENC_SYNC_SCHED_Q_MASK   ((kdu_int32)(3<<KD_ENC_SYNC_SCHED_Q_POS))
#define KD_ENC_SYNC_SCHED_MS_POS 17
#define KD_ENC_SYNC_SCHED_MS_BIT0  ((kdu_int32)(1<<KD_ENC_SYNC_SCHED_MS_POS))
#define KD_ENC_SYNC_SCHED_MS_MASK  ((kdu_int32)(7<<KD_ENC_SYNC_SCHED_MS_POS))
#define KD_ENC_SYNC_SCHED_P_POS  20
#define KD_ENC_SYNC_SCHED_P0_BIT   ((kdu_int32)(1<<KD_ENC_SYNC_SCHED_P_POS))
#define KD_ENC_SYNC_SCHED_P_MASK   ((kdu_int32)(31<<KD_ENC_SYNC_SCHED_P_POS))
#define KD_ENC_SYNC_SCHED_R_POS  25
#define KD_ENC_SYNC_SCHED_R_BIT0   ((kdu_int32)(1<<KD_ENC_SYNC_SCHED_R_POS))
#define KD_ENC_SYNC_SCHED_R_MASK   ((kdu_int32)(127<<KD_ENC_SYNC_SCHED_R_POS))

#define KD_ENC_SYNC_SCHED_INFLIGHT_MASK \
  ((0xAA<<KD_ENC_SYNC_SCHED_U_POS) | KD_ENC_SYNC_SCHED_R_MASK)

    // Note: considering that `rel_P' (P field above) occupies only 5 bits,
    // the least significant 2 bits of which hold the `Cp' value (partial
    // stripes), the `rel_Rp' value may not exceed 7.  The `rel_Rp' value
    // represents the number of whole stripes that are available from
    // `kdu_subband', starting from the current FIRST ACTIVE stripe.

struct kd_encoder_sync_state {
  public: // Memory management functions
    static size_t calculate_size()
      { 
        size_t len = sizeof(kd_encoder_sync_state);
        len = (len + KDU_MAX_L2_CACHE_LINE-1) & ~(KDU_MAX_L2_CACHE_LINE-1);
        return len;
      }
    void init()
      { sched.set(0); block_row_counter.set(0);
        wakeup=NULL; dependencies_closed=false; }
  public: // Data
    kdu_interlocked_int32 sched;
    kdu_interlocked_int32 block_row_counter; // See below
    kdu_thread_entity_condition *wakeup; // Works with the sched.W bit flag
    bool dependencies_closed; // If `kd_encoder::update_dependencies' has been
                              // called with non-zero `closure' argument.
  };
  /* Notes:
        This object holds the shared synchronization state variables that
     are used to manage the scheduling of encoding jobs and the availability
     of buffer space for entities that invoke `kd_encoder::push'.
        The `block_row_counter' variable is used only to check whether or
     not the `kdu_subband::block_row_generated' function has been invoked
     for the first row of code-blocks already.  If not and the the first
     row of code-blocks has a different height to the others, that height
     should be passed in the call to `kdu_subband::block_row_generated'.
        The `sched' variable has a complex structure, whose various bit
     fields are defined by the `KD_ENC_SYNC_SCHED_...' macros.  These bit
     fields have the following interpretation:
     * S [3 bits], lies in the range 0 <= S <= 4, and is interpreted as
       the number of stripes that are available for the `kd_encoder::push'
       function to store subband sample data.  When the last line of a stripe
       is filled, the value of S is decremented.  Each time S becomes 0, the
       `kd_encoder::propagate_dependencies' function is invoked with a
       `new_dependencies' value of 1, meaning that future calls to
       `kd_encoder::push' might block the caller.  Each time S transitions 
       from 0, the `kd_encoder::propagate_dependencies' function is
       invoked with a `new_dependencies' value of -1, retracting any such
       potentially blocking condition.
     * W is a 1-bit flag that indicates whether or not the `push' function
       has encountered the S=0 condition while attempting to push in a
       new line of subband samples.  This is a blocking condition.
       When a completing block encoding job increments the S field,
       if also checks for (and resets) the W flag.  If W is found, S
       should previously have been 0, and the `wakeup' member is expected
       to hold the address of the blocked thread's current
       `kdu_thread_entity_condition' object -- this object is signalled to
       wake the blocked thread.  When the `push' function needs to write the
       first line of a new stripe and finds that S=0, it sets the
       `wakeup' member to reference the thread's current
       `kdu_thread_entity_condition' object, then enters a compare-and-set
       loop that sets W to 1 and checks to see if S=0; if the W=1, S=0
       condition is left behind in `sched', the thread waits to be signalled
       within `kdu_thread_entity::wait_for_condition'.
     * U [8 bits] is organized as 4 x 2-bit fields U0, U1, U2 and U3,
       identifying the encoding status of up to four stripes.  U is required
       primarily because block encoding jobs might not complete in the same
       order as the stripes to which they belong.  The meaning of the status
       fields is as follows:
         -- Un = 0 means stripe-n is either UNUSED or is included in the
            count S, of stripes available to the `kd_encoder::push' function.
         -- Un = 1 means stripe-n has been FULLY ENCODED, but is not yet
            included in the count S, of stripes available to the
            `kd_encoder::push' function.
         -- Un = 2 means stripe-n is PARTIALLY SCHEDULABLE, where the
            number of job quanta from the sctripe that are allowed to be
            scheduled is determined by the Q field (see below).  Even if a
            job is schedulable, it is not actually scheduled unless the
            rel_P field indicates that it could proceeed without entering
            a critical section within `kdu_subband::open_block' or
            `kdu_subband::close_block' (see below).  At most one stripe
            may be partially scheduled.
         -- Un = 3 means stripe-n is FULLY SCHEDULABLE.  All of its jobs are
            available for scheduling and indeed may already have been
            scheduled, depending on the rel_P field (see below).
     * A [2 bits] lies in the range 0 <= A <= 3, and is interpreted as the
       index n of the first stripe not yet fully encoded.  This is also
       known as the first active stripe.  Once stripe-n has been fully
       encoded, the value of S can be incremented.  If Un < 2, there are
       currently no stripes with jobs being encoded.  When the last encoding
       job of stripe n=A completes, S is incremented, Un is set to 0, and A
       is incremented (modulo the number of stripes) -- it may be that later
       stripes have Un=1, in which case they are changed to Un=0 and S is
       incremented further.  When `kd_decoder::push' pushes the last line of
       a stripe p, it decreases S and sets Up to 2 or 3, setting the Q field
       to an appropriate value (see below).
     * T is a 1-bit flag that identifies whether or not the
       `kd_encoder::request_termination' has been called.  If this flag is
       present, `kd_encoder::all_done' will be called as soon as all
       current in-flight jobs complete.
     * Q [2 bits] controls the number of jobs that can be scheduled from
       PARTIALLY SCHEDULABLE stripes.  At most one stripe n can have the
       partial schedulability status value Un=2.  The jobs associated with
       any given stripe are collected into "job quanta"; these are discussed
       in the notes following `kd_encoder'.  In any event, the maximum number
       of job quanta in any given stripe is 4.  The value of Q represents
       the number of leading job quanta that can be scheduled within the
       PARTIALLY SCHEDULABLE stripe.  If there is no PARTIALLY SCHEDULABLE
       stripe, the value of Q is not important, but it should be set to 0.
       The value of Q is incremented at regular intervals from within
       `kd_encoder::push' in order to release jobs for processing at an
       approximately uniform rate.  Note that this field's length should
       be at least as large as `KD_ENC_QUANTUM_BITS'.
     * MS [3 bits] is interpreted as the minimum value for S required to
       be sure that the `kd_encoder::push' function will never again need to
       block.  The special value of 7 means that MS has not yet been set by
       the `kd_encoder::push' function because it is too far away from the
       end of the subband.  If there are sufficient stripes to accommodate
       the entire subband, right from the start, the `kd_encoder::start'
       function initializes MS and S both to `num_stripes'; otherwise,
       `kd_encoder::start' initializes MS to 7 and S to `num_stripes', then
       `kd_encoder::push' sets MS to `num_stripes' as soon as it has been
       used to push all but the last `num_stripes' stripes in the subband.
       Once MS is <= `num_stripes', the value of MS and S are both decremented
       together each time `kd_encoder::push' completes a new stripe for
       encoding.  This eventually leaves MS=0, which means that all data has
       been made available for block encoding -- this allows the
       `schedule_new_jobs' function to determine whether or not it is
       scheduling the last job for the queue.  Each time the completion of
       block encoding jobs results in an increment to S, the value of MS is
       also examined.  If S transitions from S < MS to S >= MS, the
       `kd_encoder::propagate_dependencies' function is invoked with a
       `delta_max_dependencies' argument of -1, indicating that calls to
       `kd_encoder::push' cannot possibly block the caller at any point in
       the future.  If MS = 0 when U becomes 0, all block encoding jobs in
       the subband have finished and so the `kd_encoder::all_done' function
       is invoked.
     * R [7 bits] is used to avoid any possibility that one thread invokes
       `all_done' while another is still accessing members of the object or
       making calls into a parent queue -- such things could be devastating
       because the `all_done' call may result in the object being cleaned up
       asynchronously.  To avoid such possibilities, the logic used to update
       the `sched' variable ensures that so long as a block encoding job is
       "in-flight", we either have one of the U0 through U3 fields non-zero
       or else we have a non-zero R field.  A block encoding job is considered
       to be in-flight from the time that it is scheduled until the point at
       which its job function returns.  Most of the time, the U0 through U3
       bits tell us everything we need to know about whether or not there are
       jobs in flight.  However, if at the end of processing a block encoding
       job, the job function passes into `kd_encoder::stripe_encoded', the
       compare-and-set loop which modifies the `sched' variable must be
       careful to also increment the R field, which reserves the right for
       it to continue accessing the object's member variables and call
       functions such as `propagate_dependencies' or `all_done'.
       Once all processing is complete, the function decrements the R field
       immediately before returning -- the only exception to this is if the
       job function invokes `all_done' itself, in which case it leaves the R
       field non-zero.  Calls to `all_done' are prevented from occurring
       from such a context unless the R field is equal to 1, while calls to
       `all_done' from any other context are prevented unless the R field
       is 0.  The R field needs to be large enough to count all threads
       in the multi-threaded processing system (which is currently limited
       to 32 or 64, for 32-bit or 64-bit operating systems), since it is
       conceivable that many threads find themselves in the final dependency
       propagation phase of their processing (although this is, of course,
       extraorinarily unlikely).  As job functions decrement the R field on
       their way out, immediately before returning, each one needs to check
       whether or not it is required to invoke the `all_done' function,
       which is true if the R value would be decremented to 0 and either
       all processing is complete or the T bit is set.
     * rel_P [5 bits] holds the value
              (rel_Rp << KD_ENC_QUANTUM_BITS) + (Cp >> N) =
                   p_trunc_status - (cur_Rp << KD_ENC_QUANTUM_BITS).
       Here, `cur_Rp' is the absolute index of the first stripe not yet
       fully encoded; we also refer to this as the FIRST ACTIVE stripe -- it
       is the one identified by A within the stripe buffer.  Here also,
       `p_trunc_status' is the truncated `p_status' value explained in
       the comments that accompany `kdu_subband::advance_block_rows_needed'.
       It follows that `rel_Rp' is the number of whole stripes of code-blocks,
       starting from the first active stripe, for which resources are believed
       to have been allocated already, so that `kdu_subband::open_block' and
       `kdu_subband::close_block' need not enter a critical section when
       invoked during the encoding of these stripes' code-blocks.
       Cp is the number of additional code-blocks that have this same
       property, from the next stripe beyond these `rel_Rp' stripes.
       [//]
       The value of rel_P is initialized to 0.  Each time the FIRST ACTIVE
       stripe (A) is advanced, the rel_P value is decreased by
       1<<`KD_ENC_QUANTUM_BITS'. Each call to `kd_encoder::update_dependencies'
       adds its `p_delta' argument (also explained with
       `kdu_subband::advance_block_rows_needed') to the value of rel_P.
       [//]
       These atomic operations simultaneously recover the previous values
       of rel_P, S, A, U, T and MS.  Based on these these quantities, it is
       always possible to determine exactly which (if any) new block
       encoding jobs can be scheduled.  Whenever a call to `kd_encoder::push'
       causes new jobs to become schedulable, that function is also able to
       determine the number and identity of new encoding jobs that can be
       scheduled, based on the pre-existing values of rel_P, S, A, U, T and MS.
       [//]
       In view of these considerations, we consider that all relevant
       block encoding jobs have been scheduled as soon as the value of `sched'
       changes.  It is on this basis that the number of in-flight jobs can
       always be evaluated, as required if premature termination is requested.
   */

/*****************************************************************************/
/*                                kd_encoder                                 */
/*****************************************************************************/

class kd_encoder : public kdu_push_ifc_base, public kdu_thread_queue {
  public: // Member functions
    kd_encoder()
      { 
        initialized=false; allocator=NULL;
        jobs[0]=jobs[1]=jobs[2]=jobs[3]=NULL;
        push_state=NULL; sync_state=NULL;
        roi_node=NULL; roi_context=NULL;
        roi_buf[0] = roi_buf[1] = roi_buf[2] = roi_buf[3] = NULL;
#       ifdef KDU_SIMD_OPTIMIZATIONS
        simd_block_quant_rev16=NULL;
#       endif
      }
    virtual ~kd_encoder()
      { 
        if (roi_node != NULL)
          roi_node->release();
      }
    void init(kdu_subband band, kdu_sample_allocator *allocator,
              bool use_shorts, float normalization, kdu_roi_node *roi,
              kdu_thread_env *env, kdu_thread_queue *env_queue,
              int flags);
    bool stripe_encoded(int which, kdu_thread_env *env);
      /* This function is called when all block encoding jobs in the
         indicated stripe complete and `env' is non-NULL.  The function
         atomically manipulates members of the `sync_state' object to
         robustly determine what steps need to be taken.  If `all_done' is
         called from within this function, it returns true; otherwise, it
         returns false. */
  protected: // These functions implement the `kdu_push_ifc_base' base class
    virtual void start(kdu_thread_env *env);
    virtual void push(kdu_line_buf &line, kdu_thread_env *env);
  protected: // These functions implement the `kdu_thread_queue' base class
    virtual int get_max_jobs() { return num_stripes*jobs_per_stripe; }
    virtual void request_termination(kdu_thread_entity *caller);
      /* Ensures that no more jobs will be scheduled and that the `all_done'
         function will be called as soon as all in-flight jobs have
         completed. This function calls `all_done' itself if and only if
         there are no in-flight jobs. */
    virtual bool update_dependencies(kdu_int32 p_delta, kdu_int32 closure,
                                     kdu_thread_entity *caller);
      /* This function may be called from within the code-stream machinery
         to identify the extent of progress that has been made in allocating
         containers for this subband's code-blocks.  The `p_delta' argument
         plays a completely different role to that normally played by the
         first argument of the `kdu_thread_queue::update_dependencies'
         function.  For a detailed explanation, consult the comments that
         appear with `kdu_subband::advance_block_rows_needed'.
         [//]
         Whenever calls to the `push' function result in the availability of
         a new stripe to encode, `kdu_subband::advance_block_rows_needed' is
         invoked, passing in the number of extra block rows that we would
         like the codestream machinery to allocate and initialize, so that
         the individual block coding jobs do not need to enter a critical
         section inside the codestream machinery in order to do this.  The
         background resource allocation generally happens asynchronously
         in a special background processing job.
         [//]
         It is possible that multiple threads invoke this function
         in close proximity and that calls arrive out of order.
         Nevertheless, the `p_delta' values are all guaranteed to be
         strictly positive and the degree to which they can grow our
         knowledge of code-blocks for which resources are available is
         limited by the `extra_rows_needed' values passed in successive
         calls to `kdu_subband::advance_block_rows_needed'.
         [//]
         After a call to `kdu_subband::detach_block_notifier' that returns
         false, this function will subsequently be called (usually from
         a different thread) with `p_delta'=0 and `closure' non-zero, to
         indicate that the deferred detachment of the object from the
         background processing machinery has finally been completed, so
         that the `all_done' function may be invoked.  Apart from this
         special case, the `p_delta' value will be strictly positive.
         [//]
         If `closure' is non-zero and `p_delta' is non-zero, this is the
         final regular call to this function from the background processing
         machinery -- there is no need for any further calls to
         `kdu_subband::advance_block_rows_needed', although there is no harm
         in issuing them.  Moreover, it is not advisable to invoke
         `kdu_subband::detach_block_notifier' in this case, since the call
         will not actually detach anything but will send the background
         machinery into a soul-searching exercise to deal with the possibility
         that the closure call to this function might still be in progress.
         Although nothing will break by issuing a call to
         `kdu_subband::detach_block_notifier', if the present function has
         been called with `closure' non-zero, this fact is recorded internally
         and used to avoid the necessity for any detachment call in the
         future.
      */
  private: // Helper functions
    int get_first_unscheduled_job_in_stripe(kdu_int32 sched, int which)
      { /* Returns the index of the first job in `which' stripe that has not
           yet been scheduled, or `jobs_per_stripe' if all have been
           scheduled; the return value is based on the value of the
           `sync_state->sched' atomic synchronization variable at a
           particular instant -- the sampled value is supplied as `sched'. */
        int P_rel =
          ((sched & KD_ENC_SYNC_SCHED_P_MASK) >> KD_ENC_SYNC_SCHED_P_POS);
        int R_rel = P_rel >> KD_ENC_QUANTUM_BITS;
        int active = (sched >> KD_ENC_SYNC_SCHED_A_POS) & 3;
        int status = (sched >> (KD_ENC_SYNC_SCHED_U_POS + 2*which)) & 3;
        if (status < 2)
          return 0; // Stripe not available for encoding
        int w_rel = which - active;
        R_rel -= (w_rel < 0)?(num_stripes+w_rel):w_rel;
        if (R_rel < 0)
          return 0; // Nothing ready to be scheduled
        int quanta = (1 << KD_ENC_QUANTUM_BITS);
        if (R_rel == 0)
          quanta = P_rel&(quanta-1); // `KD_ENC_QUANTUM_BITS' LSB's of P_rel
        if (status == 2)
          { // Partially schedulable stripe
            int max_quanta = ((sched & KD_ENC_SYNC_SCHED_Q_MASK) >>
                              KD_ENC_SYNC_SCHED_Q_POS);
            quanta = (max_quanta < quanta)?max_quanta:quanta;
          }
        int J = quanta * jobs_per_quantum;
        return (J >= jobs_per_stripe)?jobs_per_stripe:J;
      }
    void schedule_new_jobs(kdu_int32 old_sched, kdu_int32 new_sched,
                           kdu_thread_entity *caller);
      /* Schedules all relevant new jobs, based on the changes observed
         between the old and new value of the `sync_state->sched' atomic
         synchronization variable, as passed to the function. */
  public: // Encoding blocks using the following object does not modify its
          // state in any way.
    kdu_block_encoder block_encoder;
  private: // Data
    kdu_subband band;
    kdu_int16 K_max; // Maximum magnitude bit-planes, exclucing ROI shift
    kdu_int16 K_max_prime; // Maximum magnitude bit-planes, including ROI shift
    bool reversible;
    bool using_shorts;
    bool full_block_stripes; // If stripe line banks hold multiple of 4 lines
    bool initialized; // True once `start' has been called
    float delta; // For irreversible compression only.
    float msb_wmse; // Normalized weighted MSE associated with first mag bit
    float roi_weight; // Multiply `msb_wmse' by this for ROI foreground blocks
    int subband_cols; // Total width of the subband (or region of interest)
    int subband_rows; // Total height of the subband (or region of interest)
    kdu_int16 first_block_width;
    kdu_int16 first_block_height;
    kdu_int16 nominal_block_width;
    kdu_int16 nominal_block_height;
    kdu_dims block_indices; // Code-block indices available for encoding
    kdu_int16 num_stripes; // Anywhere from 1 to 4 stripes are supported
    kdu_int16 log2_job_blocks; // Nominal job has 2^log2_job_blocks code-blocks
    kdu_int16 quanta_per_stripe;  // See below
    kdu_int16 quantum_scheduling_offset; // See notes on job scheduling below
    kdu_int16 lines_per_scheduled_quantum; // See notes on job scheduling below
    int jobs_per_stripe; // Number of independent encoding jobs in each stripe
    int jobs_per_quantum; // Num jobs in all but last quantum of a stripe
    int raw_line_width; // `subband_cols' + (possibly) one extra sample
  private: // Members related to storage allocated by `kdu_sample_allocator' 
    kdu_sample_allocator *allocator;
    size_t allocator_offset; // Returned by `allocator->pre_alloc_block'
    size_t allocator_bytes; // Value passed to `allocator->pre_alloc_block'
    kd_encoder_job **jobs[4]; // One array of jobs per stripe
    kd_encoder_push_state *push_state; // Accessed exclusively by `push'
    kd_encoder_sync_state *sync_state; // Shared synchronization variables
  private: // Members related to ROI processing
    kdu_roi_node *roi_node;
    kdu_thread_context *roi_context; // So we can lock mutex
    int roi_row_gap;
    kdu_byte *roi_buf[4]; // One buffer for each stripe
  private: // The following function pointers may be available (non-NULL),
           // depending on the underlying architecture.
#ifdef KDU_SIMD_OPTIMIZATIONS
    union { // These functions require the block width to be a multiple of 4
      kd_block_quant_rev16_func simd_block_quant_rev16;
      kd_block_quant_rev32_func simd_block_quant_rev32;
      kd_block_quant_irrev16_func simd_block_quant_irrev16;
      kd_block_quant_irrev32_func simd_block_quant_irrev32;
    };
#endif // KDU_SIMD_OPTIMIZATIONS        
  };
  /* Notes:
        This object is designed with platform-specific optimizations and
     multi-threading firmly in mind.  The main object's members are configured
     by `init' and `start', after which they remain fixed during all actual
     data processing operations -- that way, multiple threads can cache the
     contents separately without having to read for ownership.  The state
     variables that do change during processing are stored within dedicated
     regions of the block of memory allocated using `kdu_sample_allocator'.
     [//]
     All block encoding, quantization, ROI renormalization and reorientation
     operations are performed within `kd_encoder_job' objects.
     These are organized into 1, 2, 3 or 4 stripes.  A stripe represents
     a single row of code-blocks, these code-blocks being partitioned into
     separate jobs, each of which may contain one or more code-blocks.
     Two stripes are generally preferred since they allow for double buffering
     (i.e., the `push' function may write subband lines to one stripe
     while the jobs in the other stripe are executing).  The implementation
     is designed so as to minimize any loss of efficiency incurred by
     performing each job in a separate thread.
     [//]
     The `jobs' member references up to four arrays (one per stripe), each of
     which holds `jobs_per_stripe' pointers to `kd_encoder_job' objects.
     These arrays of pointers are actually allocated from the same block
     of memory as the `push_state' object, since they normally need to
     be accessed only from within the `push' function.
     The `kd_encoder_job' objects themselves each occupy their own
     distinct blocks of memory, involving disjoint assumed L2 cache lines.
     [//]
     The `sync_state' member also points to an object that is stored within
     its own assumed L2 cache line.  This object manages all synchronization
     variables that might be accessed either from within the `push'
     function or from within block encoding jobs.  See the definition of
     that object for a comprehensive description of they way synchronization
     works.
     [//]
     Each stripe has its own synchronization variable referenced by
     `kd_encoder_job::pending_stripe_jobs', that is accessed only by block
     encoding jobs that are associated with the stripe.  This variable
     is set to `jobs_per_stripe' by the `push' function, immediately before
     making a stripe available for block encoding jobs to be scheduled.
     The only other place from which this variable may be accessed is the
     `request_termination' function -- that function's implementation
     expects that whenever `push' sets the value of `pending_stripe_jobs'
     to `jobs_per_stripe', it must do so prior to the point at which the
     relevant U status bits in the `sync_state->sched' variable are set.
     [//]
     Similarly, each stripe has its own array of line buffer pointers,
     referenced by `kd_encoder_job::lines16'/`kdu_encoder_job::lines32'.
     This array of pointers lives in its own set of (assumed) L2 cache lines
     and is not modified in any way while block encoding jobs are working
     within the stripe.  By contrast, the `kd_encoder_push_state' object's
     `lines16'/`lines32' array contains working buffer pointers that may
     be modified each time `kd_encoder::push' is called.
     [//]
     Although it is natural to schedule all jobs of a stripe for processing
     as soon as the stripe becomes available (i.e., as soon as `push' writes
     the last line of data to the stripe), this can produce non-ideal
     bubbles in the work available for processing by threads.  These bubbles
     may cause more complex jobs (typically from lower resolutions) to bunch
     up rather than getting distributed uniformly within the open work load.
     Complex jobs are processing intensive, while less complex jobs are
     more likely to be memory bandwidth intensive, so it is better to
     distribute jobs as uniformly as possible.  There are other reasons to
     strive for a more uniform distribution, but in any case the way we
     achieve this is by scheduling jobs in "quanta".  The number of jobs
     in each quantum (except possibly the last one on a stripe) is given
     by `jobs_per_quantum'.  The `jobs_per_quantum' value is chosen in
     such a way that each stripe is spanned by at most
     2^`KD_ENC_QUANTUM_BITS' quanta.
        After accumulating the `p_delta' values passed to `update_dependencies',
     the least significant `KD_ENC_QUANTUM_BITS' bits of the result
     identify the number of job quanta whose code-blocks can be considered
     to be resourced, from the the first row of code-blocks that has not yet
     been completely resourced.  When `kdu_subband::attach_block_notifier'
     is called, its `quantum_bits' argument is set to `KD_ENC_QUANTUM_BITS',
     while its `num_quantum_blocks' argument is set to the product of
     2^`log2_job_blocks' and `jobs_per_quantum'.  As explained in the
     documentation for `kdu_subband::advance_block_rows_needed', this means
     that the `p_delta' values passed to `update_dependencies' will carry
     information about the number of whole code-block rows and the number of
     additional job quanta for which resources are available.
     The Q field within `sync_state->sched' specifies the maximum number of
     job quanta which are allowed to be scheduled from any stripe that is
     marked as partially schedulable -- see below.
        The `quanta_per_stripe', `quantum_scheduling_offset' and
     `lines_per_scheduled_quantum' members are used by the `push' function
     to determine how many job quanta should be marked as schedulable within
     the stripe that has most recently been fully pushed.  Specifically, let R
     be the number of lines in the active push stripe that have yet to be
     written by the `push' function, and let Q be the number of initial quanta
     to be marked as schedulable from the most recently pushed stripe.  Then
         Q = `quanta_per_stripe'
           - (R - `quantum_scheduling_offset') / `lines_per_scheduled_quantum'
     The `quantum_scheduling_offset' is chosen in such a way as to ensure
     that Q >= `quanta_per_stripe' by the time R reaches 1, meaning that
     before we push the last line into a stripe, we must have made all
     quanta from the previous stripe schedulable.  This condition means
     that (1 - `quantum_scheduling_offset') must be strictly less than
     `lines_per_scheduled_quantum'.  Equivalently,
         `quantum_scheduling_offset' > 1 - `lines_per_scheduled_quantum'.
     If `lines_per_scheduled_quantum' is set to 0, the entire stripe is to
     be made schedulable as soon as the `push' function has finished with it.
     Otherwise, `lines_per_scheduled_quantum' is generally set to the ratio
     between `nominal_block_height' and `quanta_per_stripe' (rounded up).
     The `quantum_scheduling_offset' value is usually just set to 1, but
     can be adjusted to explore its impact on the scheduling diversity
     between image components and subband orientations.
  */

#endif // ENCODING_LOCAL_H
