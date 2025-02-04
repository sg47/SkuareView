///* ========================================================================= */
///*                                   casa_in                                 */
///* ========================================================================= */
//
///*****************************************************************************/
///*                               casa_in::casa_in                            */
///*****************************************************************************/
//
//casa_in::casa_in(const char *fname, 
//                 kdu_args &args, 
//                 kdu_image_dims &dims, 
//                 int &next_comp_idx,
//                 bool &vflip,
//                 kdu_rgb8_palette *palette)
//{
//    // Initialize the state incase we need to cleanup prematurely
//    kdu_message_formatter out(&cout_message);
//    incomplete_lines = NULL;
//    free_lines = NULL;
//    num_unread_rows = 0;
//
//    // Register Open Functions ?
//
//    // Parse command line arguments
//    if (!parse_casa_parameters(args))
//        { kdu_error e; e << "Unable to parse casacore parameters"; }
//
//    // Open the file
//    lattice (ImageOpener::openImage (fname));
//    if (!lattice)
//       { kdu_error e; e << "Unable to open CASACORE file." }
//    
//    // Identify data type
//    cinfo.data_type = lattice->dataType();
//
//    // Create Image interface pointer
//    switch cinfo.data_type {
//        case TpFloat:
//            is_signed = true; // floats can only be signed
//            precision = 32;
//            sample_bytes = 4;
//            full_img = dynamic_cast<ImageInterface<float>*>(lattice.operator->());
//        case TpUInt:
//            is_signed = false;
//            precision = 32;
//            sample_bytes = 4;
//            kdu_error e; e << "TpUInt unimplemented.";
//        case TpInt:
//            is_signed = true;
//            kdu_error e; e << "TpUInt unimplemented.";
//    }
//
//    // Identify number of dimensions
//    IPosition img_shape = full_img->shape();
//    cinfo.naxis = img_shape.nelements();
//    if (cinfo.naxis == -1)
//        { kdu_error e; e << "Unable to identify number of dimensions in "
//                            "CASACORE image."; }
//    
//    // TODO: implement forced precision
//
//    AlwaysAssert (img, AipsError); // TODO: ?
//
//    // Perform cropping on img
//    IPosition start(img_shape);
//    IPosition end(img_shape);
//
//    out << "CASACORE image dimensions\n"
//        << "dims = " << img_shape.nelements() << "\n"
//        << "cols = " << end(0) << "\n"
//        << "rows = " << end(1) << "\n";
//
//    // More than 2 dimensions
//    if (cinfo.naxis > 2) {
//        out << "frames = " << end(2) << "\n";
//        // More than 3 dimensions
//        if (cinfo.naxis > 3)
//            out << "stokes = " << end(3) << "\n";
//    } 
//
//    if (dims.get_cropping(crop_y, crop_x, crop_height, crop_width, next_comp_idx)) {
//        if ((crop_x < 0) || (crop_y < 0)) 
//            { kdu_error e; e << "Requested input file cropping parameters are "
//            "illegal -- cannot have negative cropping offsets."; }
//        if ((crop_x + crop_width) > cinfo.width)
//            { kdu_error e; e << "Requested input file cropping parameters are "
//            "not compatible with actual image dimensions.  The cropping "
//            "region would cross the right hand boundary of the image."; }
//        if ((crop_y + crop_height) > cinfo.height)
//            { kdu_error e; e << "Requested input file cropping parameters are "
//            "not compatible with actual image dimensions. The cropping "
//            "region would cross the the lower hand boundary of the image."; }
//        start(0) = crop_x; // column start
//        start(1) = crop_y; // row start
//        end(0) = crop_width;
//        end(1) = crop_height;
//    }
//    else {
//        start(0) = 0;
//        start(1) = 0;
//        // ends should already be initialized to the full image's extent
//    }
//
//    // TODO: cropping on frames and stokes
//
//    Slicer slice(start, end, Slicer::endIsLast);
//    crop_img(*full_img, slice);
//
//    // Initialize offset and extent of the cropped image
//    extent(crop_img);
//    offset(crop_img);
//    for (int i = 0; i < crop_img.nelements(); ++i)
//        offset(i) = 0;
//
//    // TODO: float minvals and maxvals
//    
//    int num_colours = 1;
//    int colour_space_confidence = 0;
//    jp2_colour_space colour_space = JP2_sLUM_SPACE;
//    bool has_premultiplied_alpha = false;
//    bool has_unassociated_alpha = false;
//
//    if (next_comp_idx == 0)
//        dims.set_colour_info(num_colours,
//                             has_premultiplied_alpha,
//                             has_unassociated_alpham
//                             colour_space_confidence,
//                             colour_space);
//
//    first_comp_idx = next_comp_idx;
//
//    // Add components
//    for (int n = 0; n < num_components; ++n) {
//        dims.add_component(extent(1), extent(0), precision, is_signed,
//                next_comp_idx);
//        next_comp_idx++;
//    }
//
//    if (cinfo.naxis > 3)
//        num_unread_rows = extent(1) * extent(2) * extent(3);
//    else if (cinfo.naxis > 2)
//        num_unread_rows = extent(1) * extent(2);
//    else
//        num_unread_rows = extent(1);
//}
//
///*****************************************************************************/
///*                               casa_in::~casa_in                           */
///*****************************************************************************/
//
//casa_in::~casa_in()
//{
//    if ((num_unread_rows > 0) || (incomplete_lines != NULL))
//    {
//        kdu_warning w;
//        w << "Not all rows of image component "
//          << first_comp_idx << " were consumed!";
//    }
//    image_line_buf *tmp;
//    while ((tmp=incomplete_lines) != NULL)
//        { incomplete_lines = tmp->next; delete tmp; }
//    while ((tmp=free_lines) != NULL)
//        { free_lines = tmp->next; delete tmp; }
//
//    // TODO: free anything I've put onto the heap
//   
//    delete full_image;
//}
//
// /*****************************************************************************/
///*                                 casa_in::get                              */
///*****************************************************************************/
//
//bool
//casa_in::get(int comp_idx, // component index. We use components for frames
//             kdu_line_buf &line, 
//             int x_tnum  // tile number, starts from 0. We use tiles for stokes
//             )
//{
//    // In the context of this function the slice will just be a row
//    int width = line.get_width();
//    assert((comp_idx >= 0) && (comp_idx < num_components));
//
//    image_line_buf *scan, *prev = NULL;
//    for (scan = incomplete_lines; scan != NULL; prev = scan, scan = scan->next) {
//        assert(scan->next_x_tnum >= x_tnum);
//        if (scan->next_x_tnum == x_tnum)
//            break;
//    }
//    if (scan == NULL) {
//        // Need to read a new image line.
//        assert(x_tnum == 0);
//        if (num_unread_rows == 0)
//            return false;
//
//        if ((scan = free_lines) == NULL)
//            scan = new image_line_buf(width, sample_bytes);
//
//        // Big enough for padding and expanding bits to bytes
//        free_lines = scan->next;
//        if (prev == NULL)
//            incomplete_lines = scan;
//        else
//            prev->next = scan;
//        scan->accessed_samples = 0;
//        scan->next_x_tnum = 0;
//
//        // TODO: Casacore black magic here
//        
//        Array<Float>* buffer;
//        Slicer row_slice (start, end, Slicer::endIsLast);
//        crop_img->doGetSlice (buffer, row_slice);
//
//        // TODO: convert from Array<T>* to kdu_line_buf
//
//        // Increment position in the Casacore file
//        // Indices represent:
//        // 0 col
//        // 1 row
//        // 2 frame
//        // 3 stoke
//        
//        offset(0) = 0; // set col to the start of next line
//        if (offset(1) == extent(1)) { // just read the last row in frame
//            offset(1) = 0; // set row to the start of next frame
//            if (cinfo.naxis > 2 && cinfo.depth > 1) {
//                if (offset(2) != extent(2)) {
//                    offset(2)++;    // next fraem
//                    ++comp_idx;     // new frame - next component
//                }
//                else if (cinfo.naxis > 3 && cinfo.stokes > 1) {
//                    offset(2) = 0; // set to the start of next stoke
//                    offset(3)++; // next stoke
//                    scan->next_x_tnum++; // next tile
//                }
//            }
//        }
//        else
//            offset(1)++; // otherwise just go to next row
//
//        num_unread_rows--;
//    }
//    
//    assert((scan->width - scan->accessed_samples) >= width);
//    scan->accessed_samples += scan->width;
//    if (scan->accessed_samples == scan->width) {
//        assert(scan == incomplete_lines);
//        incomplete_lines = scan->next;
//        scan->next = free_lines;
//        free_lines = scan;
//    }
//    
//    return true;
//} 
