/*****************************************************************************/
// File: KduRender.cs [scope = MANAGED/CSHARP_SAMPLES]
// Version: Kakadu, V5.2
// Author: David Taubman
// Last Revised: 17 July, 2006
/*****************************************************************************/
// Copyright 2001, David Taubman, The University of New South Wales (UNSW)
// The copyright owner is Unisearch Ltd, Australia (commercial arm of UNSW)
// Neither this copyright statement, nor the licensing details below
// may be removed from this file or dissociated from its contents.
/*****************************************************************************/
// Licensing details:
// In order to use, modify, redistribute or profit from this software in any
// manner, you must obtain an appropriate license from the copyright owner.
// Licenses appropriate for commercial and non-commercial activities may
// be obtained by following the links available at the following URL:
// "http://maestro.ee.unsw.edu.au/~taubman/kakadu".
/******************************************************************************
Description:
   Simple example using Kakadu's managed native interfaces.  This is
intended to be an exact replica of the KduRender.java example, so as
to demonstrate Kakadu's langage inter-operability features.  This
application employs a very simple strategy for rendering a JPEG2000
compressed image to the screen.  If the image is too large to fit on
the screen, only its upper left hand corner will actually be displayed.
The intent here is not to replicate the full functionality of a viewer
such as "kdu_show" in C#, although this could certainly be done.
******************************************************************************/

using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;
using System.Data;
using kdu_mni;

namespace KduRender
{
  /* ======================================================================= */
  /*                           Ckdu_sysout_message                           */
  /* ======================================================================= */
  /// <summary>
  /// Overrides Kdu_message to implement error and warning message
  /// services.  Objects of this class can be passed to Kakadu's error
  /// and warning message customization functions, to ensure that errors
  /// in the Kakadu native code will be handled correctly in the managed
  /// environment.
  /// </summary>
  class Ckdu_sysout_message : Ckdu_message 
  {
    public Ckdu_sysout_message(bool raise_exception)
    {
      this.raise_exception_on_end_of_message = raise_exception;
    }
    public override void put_text(string text) 
    { // Implements the C++ callback function `Ckdu_message::put_text'
      Console.Out.Write(text);
    }
    public override void flush(bool end_of_message)
    { // Implements the C++ callback function `Ckdu_message::flush'.
      if (end_of_message && raise_exception_on_end_of_message)
        throw new System.Exception(); // Whatever we throw, it will be
                                      // converted to a C++ integer exception
    }
    private bool raise_exception_on_end_of_message;
  }

  /* ======================================================================= */
  /*                               ImagePanel                                */
  /* ======================================================================= */
  /// <summary>
  /// Derived Windows Form object which provides an internal `Bitmap' to
  /// manage imagery generated by the `Ckdu_region_decompressor' object
  /// and passed into the present object's `put_region' function from the
  /// main application class `KduRender'.
  /// </summary>
  class ImagePanel : System.Windows.Forms.Form 
  {
    private System.Windows.Forms.PictureBox picture_box;
    private Bitmap view_bitmap;

    /// <summary>
    /// Create a window onto which an image of the specified size
    /// (`view_size') can be painted using the `put_region' function.
    /// </summary>
    public ImagePanel(Ckdu_coords view_size)
    {
      this.ClientSize = new Size(view_size.x,view_size.y);
      picture_box = new PictureBox();
      picture_box.ClientSize = new Size(view_size.x,view_size.y);
      picture_box.Location = new Point(0,0);
      this.Controls.Add(picture_box);
      view_bitmap = new Bitmap(view_size.x,view_size.y,
                               PixelFormat.Format32bppArgb);
      picture_box.Image = view_bitmap;
    }

    /// <summary>
    /// Copies the supplied region to the `view_bitmap' and updates the
    /// corresponding region on the form window.
    /// </summary>
    public void put_region(Ckdu_dims region, Int32[] buf)
    {
      int off_x = region.access_pos().x;
      int off_y = region.access_pos().y;
      int width = region.access_size().x;
      int height = region.access_size().y;
      int x, y, buf_pos=0;
      // NOTE: This is the BOTTLENECK in the whole application, since it
      // must use highly inefficient SAFE pixel copying.  SAFE pixel copying
      // to Microsoft's Bitmap objects is almost always significantly slower
      // than the entire JPEG2000 decompression and rendering process, even
      // for complex, high bit-rate objects.  In the "KduRender2" demo,
      // I show how to avoid this bottleneck by using the more powerful
      // `Ckdu_region_compositor' object and deriving from the
      // `Ckdu_compositor_buf' object which it uses to manage custom
      // image buffers.
      for (y=0; y < height; y++)
        for (x=0; x < width; x++, buf_pos++)
          view_bitmap.SetPixel(off_x+x,off_y+y,Color.FromArgb(buf[buf_pos]));
      picture_box.Invalidate(new Rectangle(off_x,off_y,width,height));
    }

    /// <summary>
    /// We always override the `Dispose(bool)' method of derived Windows
    /// components, to clean up system resources.  Kakadu provides exactly
    /// the same framework for its native objects.  We do not need to
    /// override a Kakadu `Dispose' function in this application, but you
    /// will see a couple of examples of this in the "KduRender2" example.
    /// </summary>
    protected override void Dispose(bool in_dispose)
    {
      if (in_dispose)
      {
        view_bitmap.Dispose();
        picture_box.Dispose();
      }
      base.Dispose(in_dispose);
    }
  }

  /* ======================================================================= */
  /*                                KduRender                                */
  /* ======================================================================= */
	class KduRender
	{
    private ImagePanel panel;
    private Ckdu_region_decompressor decompressor;
    private Ckdu_dims view_dims;
    private Ckdu_dims incomplete_region;
    private int[] region_buf;
    private int region_buf_size;

    /// <summary>
    /// Constructor just stores references to the objects which are used
    /// by `process_strip' to do the processing.
    /// </summary>
    private KduRender(ImagePanel panel,
                      Ckdu_region_decompressor decompressor,
                      Ckdu_dims view_dims)
    {
      this.panel = panel;
      this.decompressor = decompressor;
      this.view_dims = view_dims;
      incomplete_region = new Ckdu_dims();
      incomplete_region.assign(view_dims);
      region_buf_size = view_dims.access_size().x * 64;
      region_buf = new int[region_buf_size];
    }

    /// <summary>
    /// Generates another strip of the image and paints it to the panel,
    /// until there is nothing more to be done.
    /// </summary>
    private void process_strip(object sender, System.EventArgs e)
    {
      if (decompressor == null)
        return; // All rendering complete
      Ckdu_dims new_region = new Ckdu_dims();
      if (!decompressor.process(region_buf,view_dims.access_pos(),
                                0,0,region_buf_size,incomplete_region,
                                new_region))
      {
        decompressor.finish();
        decompressor = null;
      }
      else
      {
        new_region.access_pos().minus(view_dims.access_pos());
                 // Make new region relative to the panel's view window
        panel.put_region(new_region,region_buf);
      }
    }

    /// <summary>
    /// This function almost invariably returns (1,1), but there can be
    /// some wacky images for which larger expansions are required.  The
    /// need for it arises from the fact that `Ckdu_region_decompressor'
    /// performs its decompressed image sizing based upon a single image
    /// component (the `image_component').  Specifically, the size of the
    /// decompressed result is obtained by expanding the dimensions of the
    /// reference component by the x-y values returned by this function.
    /// Reference expansion factors must have the property that when the
    /// first component is expanded by this much, any other components
    /// (typically colour components) are also expanded by an integral
    /// amount.  The `Ckdu_region_decompressor' actually does support
    /// rational expansion of individual image components, but we do not
    /// exploit this feature in the present coding example.
    /// </summary>
    private static Ckdu_coords
      determine_reference_expansion(int reference_component,
                                    Ckdu_channel_mapping channels,
                                    Ckdu_codestream codestream)
    {
      int c;
      Ckdu_coords ref_subs = new Ckdu_coords();
      Ckdu_coords subs = new Ckdu_coords();
      codestream.get_subsampling(reference_component,ref_subs);
      Ckdu_coords min_subs = new Ckdu_coords(); min_subs.assign(ref_subs);
      for (c=0; c < channels.num_channels; c++)
      {
        codestream.get_subsampling(channels.get_source_component(c),subs);
        if (subs.x < min_subs.x)
          min_subs.x = subs.x;
        if (subs.y < min_subs.y)
          min_subs.y = subs.y;
      }
      Ckdu_coords expansion = new Ckdu_coords();
      expansion.x = ref_subs.x / min_subs.x;
      expansion.y = ref_subs.y / min_subs.y;
      for (c=0; c < channels.num_channels; c++)
      {
        codestream.get_subsampling(channels.get_source_component(c),subs);
        if ((((subs.x * expansion.x) % ref_subs.x) != 0) ||
            (((subs.y * expansion.y) % ref_subs.y) != 0))
        {
          Ckdu_global.kdu_print_error(
            "The supplied JP2 file contains colour channels " +
            "whose sub-sampling factors are not integer " +
            "multiples of one another.");
          codestream.apply_input_restrictions(0,1,0,0,null,
            Ckdu_global.KDU_WANT_OUTPUT_COMPONENTS);
          channels.configure(codestream);
          expansion = new Ckdu_coords(1,1);
        }
      }
      return expansion;
    }

    /*************************************************************************/
    /**                                 Main                                **/
    /*************************************************************************/
		[STAThread]
		static void Main(string[] args)
    {
      // Start by customizing the error and warning services in Kakadu
      Ckdu_sysout_message sysout =
        new Ckdu_sysout_message(false); // Non-throwing message printer
      Ckdu_sysout_message syserr =
        new Ckdu_sysout_message(true); // Exception-throwing message printer
      Ckdu_message_formatter pretty_sysout =
        new Ckdu_message_formatter(sysout); // Non-throwing formatted printer
      Ckdu_message_formatter pretty_syserr =
        new Ckdu_message_formatter(syserr); // Throwing formatted printer

      Ckdu_global.kdu_customize_warnings(pretty_sysout);
      Ckdu_global.kdu_customize_errors(pretty_syserr);

      // Declare objects that we want to be able to dispose in an
      // orderly fashion -- we do this outside the try/catch statement
      // below, since otherwise, the objects will be automatically
      // finalized by the garbage collector in the event of an exception
      // and we would lose control of the order of disposal.  See END NOTE 3.
      Ckdu_simple_file_source raw_src = null; // Must be disposed last
      Cjp2_family_src family_src = new Cjp2_family_src(); // Dispose last
      Cjpx_source wrapped_src = new Cjpx_source(); // Dispose before codestream
      Ckdu_codestream codestream = new Ckdu_codestream(); // Needs `destroy'
      Ckdu_channel_mapping channels = new Ckdu_channel_mapping();
      Ckdu_region_decompressor decompressor = new Ckdu_region_decompressor();
      try 
      { // See END NOTE 1
        if (args.Length != 1)
          Ckdu_global.kdu_print_error("You must supply a file name " +
                                      "(JP2, JPX or raw codestream)");

        // Open input file as raw codestream or a JP2/JPX file
        string fname = args[0];
        family_src.open(fname); // Generates an error if file doesn't exist
        Cjpx_layer_source xlayer = null;
        Cjpx_codestream_source xstream = null;
        Ckdu_compressed_source input=null; // Allows us to refer to compressed
        // data source associated with either a raw codestream
        // file or a JP2/JPX embedded codestream.

        int success = wrapped_src.open(family_src,true);
        if (success < 0)
        { // Must open as raw file
          family_src.close();
          wrapped_src.close();
          raw_src = new Ckdu_simple_file_source(fname);
          input = raw_src;
        }
        else
        { // Succeeded in opening as wrapped JP2/JPX source
          xlayer = wrapped_src.access_layer(0);
          xstream = wrapped_src.access_codestream(xlayer.get_codestream_id(0));
          input = xstream.open_stream();
        }
        
        // Create the code-stream management machinery
        codestream.create(input);
        if (xlayer != null)
          channels.configure(xlayer.access_colour(0),xlayer.access_channels(),
                             xstream.get_codestream_id(),
                             xstream.access_palette(),
                             xstream.access_dimensions());
        else
          channels.configure(codestream);
        int ref_component = channels.get_source_component(0);
        Ckdu_coords ref_expansion =
          determine_reference_expansion(ref_component,channels,codestream);

        // Determine dimensions for the rendered result & start decompressor
        Ckdu_dims view_dims =
          decompressor.get_rendered_image_dims(codestream,channels,-1,
                                               0,ref_expansion);
        Rectangle display_rect = Screen.PrimaryScreen.Bounds;
        Ckdu_coords view_size = view_dims.access_size();
                // Note: changes in `view_size' will also affect `view_dims'
        if (view_size.x > display_rect.Width)
          view_size.x = display_rect.Width;
        if (view_size.y > display_rect.Height)
          view_size.y = display_rect.Height;
        decompressor.start(codestream,channels,-1,0,16384,
                           view_dims,ref_expansion);

        // Pop up a window and render the image to it progressively
        ImagePanel panel = new ImagePanel(view_size);
        KduRender render_app = new KduRender(panel,decompressor,view_dims);
        Application.Idle += new EventHandler(render_app.process_strip);
        Application.Run(panel);
      }
      catch (Exception)
      { // See END NOTE 2
        Console.Out.Write("[Caught exception]\n");
      }

      // Cleanup: Disposal must happen in the right order
      // See END NOTE 3 for a discussion of Kakadu object disposal
      decompressor.Dispose();
      channels.Dispose();
      if ((codestream != null) && codestream.exists())
        codestream.destroy();
      if (raw_src != null)
        raw_src.Dispose();
      wrapped_src.Dispose();
      family_src.Dispose();
    }
	}
}

/* END NOTE 1:
      Enclosing code in a try/catch clause allows you to catch any errors
      which may be generated by the Kakadu core system (or from elements
      of the C# implementation).  The way this works is worth explaining
      briefly in the sequel.
      
      If internal errors are generated through the core error
      service `kdu_error', they will be routed to the `pretty_syserr'
      object we installed using `Ckdu_global.kdu_customize_errors' above.
          Note: The simplest way to generate your own `kdu_error' errors
          from C# is to use the `Ckdu_global.kdu_print_error' function.
      When the `kdu_error' object is destroyed at the end of the
      internal error clause, it issues a call to our `pretty_syserr'
      object's `flush' function, with the `end_of_message' argument
      set to true.  The `pretty_syserr' object issues calls to
      `syserr.put_text' (for printing the message) and `sysout.flush'
      (when `pretty_syserr.flush' is called).  Our `syserr' object
      has been derived from `Ckdu_message' in such a way as to override
      the `Ckdu_message.put_text' and `Ckdu_message.flush' functions.
      The latter throws an exception when it receives the end of message
      flush call.  The exception unwinds the execution stack all the
      way back to the catch clause appearing below.  This is designed
      to be robust from the inside to the outside.
      
      When an exception is thrown, the Kakadu interop bindings
      (implemented in "kdu_mni.dll") automatically cleanup any
      temporary resources they have allocated for inter-operability.
      Also, the Kakadu native implementation's exception handling
      should destroy any resources which would otherwise be lost.
      From the catch clause, you should find yourself in a safe
      position to continue interacting with any Kakadu objects you
      have created (e.g., a `Ckdu_codestream' interface), but the
      normal response will be to destroy such objects (see END NOTE 3).
*/

/* END NOTE 2:
      Seeming as our implementation of the `Ckdu_sysout_message.flush'
      function throws exceptions, if the Kakadu native code generates
      an error through `kdu_error', we expect that exception to be caught
      by "catch (System.Exception)".
          Actually, all exceptions thrown from Kakadu callback functions
      (functions marked with the [BIND: callback] option -- designed to be
      implemented in a derived object, including a foreign language object)
      are automatically converted to C++ integer exceptions with the value 0.
      So it does not matter what type of exception you throw from the
      `Ckdu_sysout_message.flush' function after printing an error.  C++
      integer exceptions may also be thrown directly from inside Kakadu native
      objects.
          All C++ integer exceptions are automatically wrapped by the runtime
      system as objects of class `System.Runtime.InteropServices.SEHException'.
      You can use this fact to distinguish between exceptions which have
      passed through the Kakadu system (or originated from it), from those
      which have been not.
*/

/* END NOTE 3:
      By and large, the managed C# environment automatically garbage
      collects objects you have allocated, including those associated
      with the internal Kakadu system -- this causes the internal
      objects to be destroyed.  However, the garbage collector has
      two important weaknesses: 1) it gives you no control over the
      order of destruction (which is sometimes important for the
      native code); and 2) it will not destroy internal objects
      which are not owned by the managed C# environment.

      To clarify this second point, Kakadu defines a special type of
      class known as an INTERFACE class, whose content is actually a
      reference to an opaque internal entity.  Kakadu allows any
      number of copies of INTERFACE objects to be made, since they
      are all objects.  INTERFACE classes have no C++ destructor,
      because the internal object is not intended to be destroyed
      when the INTERFACE object goes out of scope.  For this reason,
      when a C# instance of an INTERFACE class is garbage collected,
      the internal object will also remain untouched (it must, for
      safety).  Although INTERFACE objects have no destructor,
      any INTERFACE object, whose internal representation can be
      explicitly destroyed, also provides an explicit `destroy'
      member function.  You must call the `destroy' function
      yourself, to destroy interfaces that you have explicitly
      created, exactly as you would from C++.
      
      Whether or not an object belongs to a Kakadu INTERFACE class
      should be very clear from the documentation generated by
      "kdu_hyperdoc", which also created the C# bindings for you.
      If in any doubt at all, look for the presence of a `destroy'
      member function.

      To address the need to control the order of destruction
      of native Kakadu objects (as opposed to just relying on C#
      garbage collection), you will find that the interop language
      bindings generated by "kdu_hyperdoc" provide a `Dispose'
      member function for you.  This destroys the native Kakadu
      object, leaving the garbage collector to cleanup the outer
      shell of the object which you see from C#.  Do not attempt to
      use the object after its `Dispose' function has been called.
      Of course, Kakadu INTERFACE classes do not provide `Dispose'.  If
      the internal resource can be explicitly destroyed, you will find
      a `Destroy' member function, as explained above.
      
      For Kakadu objects whose managed language binding provides a `Dispose'
      function, a separate `Dispose(bool)' function will also be provided,
      which you can override in a derived object to provide your own
      pre-disposal cleanup, being sure to call `base.Dispose(bool)' once
      you are done.  These functions follow exactly the same disposal
      semantics as Windows component objects.
*/

