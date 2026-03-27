zarray_t *apriltag_detector_detect(apriltag_detector_t *td, image_u8_t *im_orig)
{
    if (zarray_size(td->tag_families) == 0) {
        zarray_t *s = zarray_create(sizeof(apriltag_detection_t*));
        printf("apriltag.c: No tag families enabled.");
        return s;
    }

    if (td->wp == NULL || td->nthreads != workerpool_get_nthreads(td->wp)) {
        workerpool_destroy(td->wp);
        td->wp = workerpool_create(td->nthreads);
    }

    timeprofile_clear(td->tp);
    timeprofile_stamp(td->tp, "init");

    ///////////////////////////////////////////////////////////
    // Step 1. Detect quads according to requested image decimation
    // and blurring parameters.
    image_u8_t *quad_im = im_orig;
    if (td->quad_decimate > 1) {
        quad_im = image_u8_decimate(im_orig, td->quad_decimate);

        timeprofile_stamp(td->tp, "decimate");
    }

    if (td->quad_sigma != 0) {
        // compute a reasonable kernel width by figuring that the
        // kernel should go out 2 std devs.
        //
        // max sigma          ksz
        // 0.499              1  (disabled)
        // 0.999              3
        // 1.499              5
        // 1.999              7

        float sigma = fabsf((float) td->quad_sigma);

        int ksz = 4 * sigma; // 2 std devs in each direction
        if ((ksz & 1) == 0)
            ksz++;

        if (ksz > 1) {

            if (td->quad_sigma > 0) {
                // Apply a blur
                image_u8_gaussian_blur(quad_im, sigma, ksz);
            } else {
                // SHARPEN the image by subtracting the low frequency components.
                image_u8_t *orig = image_u8_copy(quad_im);
                image_u8_gaussian_blur(quad_im, sigma, ksz);

                for (int y = 0; y < orig->height; y++) {
                    for (int x = 0; x < orig->width; x++) {
                        int vorig = orig->buf[y*orig->stride + x];
                        int vblur = quad_im->buf[y*quad_im->stride + x];

                        int v = 2*vorig - vblur;
                        if (v < 0)
                            v = 0;
                        if (v > 255)
                            v = 255;

                        quad_im->buf[y*quad_im->stride + x] = (uint8_t) v;
                    }
                }
                image_u8_destroy(orig);
            }
        }
    }

    timeprofile_stamp(td->tp, "blur/sharp");

    if (td->debug)
        image_u8_write_pnm(quad_im, "debug_preprocess.pnm");

    zarray_t *quads = apriltag_quad_thresh(td, quad_im);

    // adjust centers of pixels so that they correspond to the
    // original full-resolution image.
    if (td->quad_decimate > 1) {
        for (int i = 0; i < zarray_size(quads); i++) {
            struct quad *q;
            zarray_get_volatile(quads, i, &q);

            for (int j = 0; j < 4; j++) {
                if (td->quad_decimate == 1.5) {
                    q->p[j][0] *= td->quad_decimate;
                    q->p[j][1] *= td->quad_decimate;
                } else {
                    q->p[j][0] = (q->p[j][0] - 0.5)*td->quad_decimate + 0.5;
                    q->p[j][1] = (q->p[j][1] - 0.5)*td->quad_decimate + 0.5;
                }
            }
        }
    }

    if (quad_im != im_orig)
        image_u8_destroy(quad_im);

    zarray_t *detections = zarray_create(sizeof(apriltag_detection_t*));

    td->nquads = zarray_size(quads);

    timeprofile_stamp(td->tp, "quads");

    if (td->debug) {
        image_u8_t *im_quads = image_u8_copy(im_orig);
        image_u8_darken(im_quads);
        image_u8_darken(im_quads);

        srandom(0);

        for (int i = 0; i < zarray_size(quads); i++) {
            struct quad *quad;
            zarray_get_volatile(quads, i, &quad);

            const int bias = 100;
            int color = bias + (random() % (255-bias));

            image_u8_draw_line(im_quads, quad->p[0][0], quad->p[0][1], quad->p[1][0], quad->p[1][1], color, 1);
            image_u8_draw_line(im_quads, quad->p[1][0], quad->p[1][1], quad->p[2][0], quad->p[2][1], color, 1);
            image_u8_draw_line(im_quads, quad->p[2][0], quad->p[2][1], quad->p[3][0], quad->p[3][1], color, 1);
            image_u8_draw_line(im_quads, quad->p[3][0], quad->p[3][1], quad->p[0][0], quad->p[0][1], color, 1);
        }

        image_u8_write_pnm(im_quads, "debug_quads_raw.pnm");
        image_u8_destroy(im_quads);
    }

    ////////////////////////////////////////////////////////////////
    // Step 2. Decode tags from each quad.
    if (1) {
        image_u8_t *im_samples = td->debug ? image_u8_copy(im_orig) : NULL;

        int chunksize = 1 + zarray_size(quads) / (APRILTAG_TASKS_PER_THREAD_TARGET * td->nthreads);

        struct quad_decode_task *tasks = malloc(sizeof(struct quad_decode_task)*(zarray_size(quads) / chunksize + 1));

        int ntasks = 0;
        for (int i = 0; i < zarray_size(quads); i+= chunksize) {
            tasks[ntasks].i0 = i;
            tasks[ntasks].i1 = imin(zarray_size(quads), i + chunksize);
            tasks[ntasks].quads = quads;
            tasks[ntasks].td = td;
            tasks[ntasks].im = im_orig;
            tasks[ntasks].detections = detections;

            tasks[ntasks].im_samples = im_samples;

            workerpool_add_task(td->wp, quad_decode_task, &tasks[ntasks]);
            ntasks++;
        }

        workerpool_run(td->wp);

        free(tasks);

        if (im_samples != NULL) {
            image_u8_write_pnm(im_samples, "debug_samples.pnm");
            image_u8_destroy(im_samples);
        }
    }

    if (td->debug) {
        image_u8_t *im_quads = image_u8_copy(im_orig);
        image_u8_darken(im_quads);
        image_u8_darken(im_quads);

        srandom(0);

        for (int i = 0; i < zarray_size(quads); i++) {
            struct quad *quad;
            zarray_get_volatile(quads, i, &quad);

            const int bias = 100;
            int color = bias + (random() % (255-bias));

            image_u8_draw_line(im_quads, quad->p[0][0], quad->p[0][1], quad->p[1][0], quad->p[1][1], color, 1);
            image_u8_draw_line(im_quads, quad->p[1][0], quad->p[1][1], quad->p[2][0], quad->p[2][1], color, 1);
            image_u8_draw_line(im_quads, quad->p[2][0], quad->p[2][1], quad->p[3][0], quad->p[3][1], color, 1);
            image_u8_draw_line(im_quads, quad->p[3][0], quad->p[3][1], quad->p[0][0], quad->p[0][1], color, 1);

        }

        image_u8_write_pnm(im_quads, "debug_quads_fixed.pnm");
        image_u8_destroy(im_quads);
    }

    timeprofile_stamp(td->tp, "decode+refinement");

    ////////////////////////////////////////////////////////////////
    // Step 3. Reconcile detections--- don't report the same tag more
    // than once. (Allow non-overlapping duplicate detections.)
    if (1) {
        zarray_t *poly0 = g2d_polygon_create_zeros(4);
        zarray_t *poly1 = g2d_polygon_create_zeros(4);

        for (int i0 = 0; i0 < zarray_size(detections); i0++) {

            apriltag_detection_t *det0;
            zarray_get(detections, i0, &det0);

            for (int k = 0; k < 4; k++)
                zarray_set(poly0, k, det0->p[k], NULL);

            for (int i1 = i0+1; i1 < zarray_size(detections); i1++) {

                apriltag_detection_t *det1;
                zarray_get(detections, i1, &det1);

                if (det0->id != det1->id || det0->family != det1->family)
                    continue;

                for (int k = 0; k < 4; k++)
                    zarray_set(poly1, k, det1->p[k], NULL);

                if (g2d_polygon_overlaps_polygon(poly0, poly1)) {
                    // the tags overlap. Delete one, keep the other.

                    int pref = 0; // 0 means undecided which one we'll keep.
                    pref = prefer_smaller(pref, det0->hamming, det1->hamming);     // want small hamming
                    pref = prefer_smaller(pref, -det0->decision_margin, -det1->decision_margin);      // want bigger margins

                    // if we STILL don't prefer one detection over the other, then pick
                    // any deterministic criterion.
                    for (int i = 0; i < 4; i++) {
                        pref = prefer_smaller(pref, det0->p[i][0], det1->p[i][0]);
                        pref = prefer_smaller(pref, det0->p[i][1], det1->p[i][1]);
                    }

                    if (pref == 0) {
                        // at this point, we should only be undecided if the tag detections
                        // are *exactly* the same. How would that happen?
                        printf("uh oh, no preference for overlappingdetection\n");
                    }

                    if (pref < 0) {
                        // keep det0, destroy det1
                        apriltag_detection_destroy(det1);
                        zarray_remove_index(detections, i1, 1);
                        i1--; // retry the same index
                        goto retry1;
                    } else {
                        // keep det1, destroy det0
                        apriltag_detection_destroy(det0);
                        zarray_remove_index(detections, i0, 1);
                        i0--; // retry the same index.
                        goto retry0;
                    }
                }

                retry1: ;
            }

            retry0: ;
        }

        zarray_destroy(poly0);
        zarray_destroy(poly1);
    }

    timeprofile_stamp(td->tp, "reconcile");

    ////////////////////////////////////////////////////////////////
    // Produce final debug output
    if (td->debug) {

        image_u8_t *darker = image_u8_copy(im_orig);
        image_u8_darken(darker);
        image_u8_darken(darker);

        // assume letter, which is 612x792 points.
        FILE *f = fopen("debug_output.ps", "w");
        fprintf(f, "%%!PS\n\n");
        double scale = fmin(612.0/darker->width, 792.0/darker->height);
        fprintf(f, "%f %f scale\n", scale, scale);
        fprintf(f, "0 %d translate\n", darker->height);
        fprintf(f, "1 -1 scale\n");
        postscript_image(f, darker);

        image_u8_destroy(darker);

        for (int i = 0; i < zarray_size(detections); i++) {
            apriltag_detection_t *det;
            zarray_get(detections, i, &det);

            float rgb[3];
            int bias = 100;

            for (int j = 0; j < 3; j++) {
                rgb[j] = bias + (random() % (255-bias));
            }

            fprintf(f, "%f %f %f setrgbcolor\n", rgb[0]/255.0f, rgb[1]/255.0f, rgb[2]/255.0f);
            fprintf(f, "%f %f moveto %f %f lineto %f %f lineto %f %f lineto %f %f lineto stroke\n",
                    det->p[0][0], det->p[0][1],
                    det->p[1][0], det->p[1][1],
                    det->p[2][0], det->p[2][1],
                    det->p[3][0], det->p[3][1],
                    det->p[0][0], det->p[0][1]);
        }

        fprintf(f, "showpage\n");
        fclose(f);
    }

    if (td->debug) {
        image_u8_t *darker = image_u8_copy(im_orig);
        image_u8_darken(darker);
        image_u8_darken(darker);

        image_u8x3_t *out = image_u8x3_create(darker->width, darker->height);
        for (int y = 0; y < im_orig->height; y++) {
            for (int x = 0; x < im_orig->width; x++) {
                out->buf[y*out->stride + 3*x + 0] = darker->buf[y*darker->stride + x];
                out->buf[y*out->stride + 3*x + 1] = darker->buf[y*darker->stride + x];
                out->buf[y*out->stride + 3*x + 2] = darker->buf[y*darker->stride + x];
            }
        }

        image_u8_destroy(darker);

        for (int i = 0; i < zarray_size(detections); i++) {
            apriltag_detection_t *det;
            zarray_get(detections, i, &det);

            float rgb[3];
            int bias = 100;

            for (int j = 0; j < 3; j++) {
                rgb[j] = bias + (random() % (255-bias));
            }

            for (int j = 0; j < 4; j++) {
                int k = (j + 1) & 3;
                image_u8x3_draw_line(out,
                                     det->p[j][0], det->p[j][1], det->p[k][0], det->p[k][1],
                                     (uint8_t[]) { rgb[0], rgb[1], rgb[2] },
                                     1);
            }
        }

        image_u8x3_write_pnm(out, "debug_output.pnm");
        image_u8x3_destroy(out);
    }

    // deallocate
    if (td->debug) {
        FILE *f = fopen("debug_quads.ps", "w");
        fprintf(f, "%%!PS\n\n");

        image_u8_t *darker = image_u8_copy(im_orig);
        image_u8_darken(darker);
        image_u8_darken(darker);

        // assume letter, which is 612x792 points.
        double scale = fmin(612.0/darker->width, 792.0/darker->height);
        fprintf(f, "%f %f scale\n", scale, scale);
        fprintf(f, "0 %d translate\n", darker->height);
        fprintf(f, "1 -1 scale\n");

        postscript_image(f, darker);

        image_u8_destroy(darker);

        for (int i = 0; i < zarray_size(quads); i++) {
            struct quad *q;
            zarray_get_volatile(quads, i, &q);

            float rgb[3];
            int bias = 100;

            for (int j = 0; j < 3; j++) {
                rgb[j] = bias + (random() % (255-bias));
            }

            fprintf(f, "%f %f %f setrgbcolor\n", rgb[0]/255.0f, rgb[1]/255.0f, rgb[2]/255.0f);
            fprintf(f, "%f %f moveto %f %f lineto %f %f lineto %f %f lineto %f %f lineto stroke\n",
                    q->p[0][0], q->p[0][1],
                    q->p[1][0], q->p[1][1],
                    q->p[2][0], q->p[2][1],
                    q->p[3][0], q->p[3][1],
                    q->p[0][0], q->p[0][1]);
        }

        fprintf(f, "showpage\n");
        fclose(f);
    }

    timeprofile_stamp(td->tp, "debug output");

    for (int i = 0; i < zarray_size(quads); i++) {
        struct quad *quad;
        zarray_get_volatile(quads, i, &quad);
        matd_destroy(quad->H);
        matd_destroy(quad->Hinv);
    }

    zarray_destroy(quads);

    zarray_sort(detections, detection_compare_function);
    timeprofile_stamp(td->tp, "cleanup");

    return detections;
}

