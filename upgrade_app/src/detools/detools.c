/**
 * BSD 2-Clause License
 *
 * Copyright (c) 2019-2020, Erik Moqvist
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include "detools.h"

/* Patch types. */
#define PATCH_TYPE_SEQUENTIAL                               0

/* Compressions. */
#define COMPRESSION_HEATSHRINK                              4

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define DIV_CEIL(n, d) (((n) + (d) - 1) / (d))

/*
 * Utility functions.
 */

static size_t chunk_left(struct detools_apply_patch_chunk_t *self_p)
{
    return (self_p->size - self_p->offset);
}

static bool chunk_available(struct detools_apply_patch_chunk_t *self_p)
{
    return (chunk_left(self_p) > 0);
}

static uint8_t chunk_get_no_check(struct detools_apply_patch_chunk_t *self_p)
{
    uint8_t data;

    data = self_p->buf_p[self_p->offset];
    self_p->offset++;

    return (data);
}

static int chunk_get(struct detools_apply_patch_chunk_t *self_p,
                     uint8_t *data_p)
{
    if (!chunk_available(self_p)) {
        return (1);
    }

    *data_p = chunk_get_no_check(self_p);

    return (0);
}

static bool is_overflow(int value)
{
    return ((value + 7) > (int)(8 * sizeof(int)));
}

static int chunk_unpack_header_size(struct detools_apply_patch_chunk_t *self_p,
                                    int *size_p)
{
    uint8_t byte;
    int offset;
    int res;

    res = chunk_get(self_p, &byte);

    if (res != 0) {
        return (-DETOOLS_SHORT_HEADER);
    }

    *size_p = (byte & 0x3f);
    offset = 6;

    while ((byte & 0x80) != 0) {
        res = chunk_get(self_p, &byte);

        if (res != 0) {
            return (-DETOOLS_SHORT_HEADER);
        }

        if (is_overflow(offset)) {
            return (-DETOOLS_CORRUPT_PATCH_OVERFLOW);
        }

        *size_p |= ((byte & 0x7f) << offset);
        offset += 7;
    }

    return (0);
}

/*
 * Heatshrink patch reader.
 */

static void unpack_heatshrink_header(uint8_t byte,
                                     int8_t *window_sz2_p,
                                     int8_t *lookahead_sz2_p)
{
    *window_sz2_p = (((byte >> 4) & 0xf) + 4);
    *lookahead_sz2_p = ((byte & 0xf) + 3);
}

static int patch_reader_heatshrink_decompress(
    struct detools_apply_patch_patch_reader_t *self_p,
    uint8_t *buf_p,
    size_t *size_p)
{
    int res;
    struct detools_apply_patch_patch_reader_heatshrink_t *heatshrink_p;
    size_t size;
    size_t left;
    HSD_poll_res pres;
    HSD_sink_res sres;
    uint8_t byte;

    heatshrink_p = &self_p->compression.heatshrink;
    left = *size_p;

    if (heatshrink_p->window_sz2 == -1) {
        res = chunk_get(self_p->patch_chunk_p, &byte);

        if (res != 0) {
            return (1);
        }

        unpack_heatshrink_header(byte,
                                 &heatshrink_p->window_sz2,
                                 &heatshrink_p->lookahead_sz2);

        if ((heatshrink_p->window_sz2 != HEATSHRINK_STATIC_WINDOW_BITS)
            || (heatshrink_p->lookahead_sz2 != HEATSHRINK_STATIC_LOOKAHEAD_BITS)) {
            return (-DETOOLS_HEATSHRINK_HEADER);
        }
    }

    while (1) {
        /* Get available data. */
        pres = heatshrink_decoder_poll(&heatshrink_p->decoder,
                                       buf_p,
                                       left,
                                       &size);

        if (pres < 0) {
            return (-DETOOLS_HEATSHRINK_POLL);
        }

        buf_p += size;
        left -= size;

        if (left == 0) {
            return (0);
        }

        /* Input (sink) more data if available. */
        res = chunk_get(self_p->patch_chunk_p, &byte);

        if (res == 0) {
            sres = heatshrink_decoder_sink(&heatshrink_p->decoder,
                                           &byte,
                                           sizeof(byte),
                                           &size);

            if ((sres < 0) || (size != sizeof(byte))) {
                return (-DETOOLS_HEATSHRINK_SINK);
            }
        } else {
            if (left != *size_p) {
                *size_p -= left;

                return (0);
            } else {
                return (1);
            }
        }
    }

    return (res);
}

static int patch_reader_heatshrink_destroy(
    struct detools_apply_patch_patch_reader_t *self_p)
{
    struct detools_apply_patch_patch_reader_heatshrink_t *heatshrink_p;
    HSD_finish_res fres;

    heatshrink_p = &self_p->compression.heatshrink;

    fres = heatshrink_decoder_finish(&heatshrink_p->decoder);

    if (fres == HSDR_FINISH_DONE) {
        return (0);
    } else {
        return (-DETOOLS_CORRUPT_PATCH);
    }
}

static int patch_reader_heatshrink_init(
    struct detools_apply_patch_patch_reader_t *self_p)
{
    struct detools_apply_patch_patch_reader_heatshrink_t *heatshrink_p;

    heatshrink_p = &self_p->compression.heatshrink;
    heatshrink_p->window_sz2 = -1;
    heatshrink_p->lookahead_sz2 = -1;
    heatshrink_decoder_reset(&heatshrink_p->decoder);
    self_p->destroy = patch_reader_heatshrink_destroy;
    self_p->decompress = patch_reader_heatshrink_decompress;

    return (0);
}

/*
 * Patch reader.
 */

/**
 * Initialize given patch reader.
 */
static int patch_reader_init(struct detools_apply_patch_patch_reader_t *self_p,
                             struct detools_apply_patch_chunk_t *patch_chunk_p,
                             size_t patch_size,
                             int compression)
{
    int res;

    self_p->patch_chunk_p = patch_chunk_p;
    self_p->size.state = detools_unpack_usize_state_first_t;

    res = patch_reader_heatshrink_init(self_p);

    return (res);
}

static int patch_reader_dump(struct detools_apply_patch_patch_reader_t *self_p,
                             int compression,
                             detools_state_write_t state_write)
{
    (void)self_p;
    (void)state_write;

    int res;

    res = 0;

    return (res);
}

static int patch_reader_restore(struct detools_apply_patch_patch_reader_t *self_p,
                                struct detools_apply_patch_patch_reader_t *dumped_p,
                                struct detools_apply_patch_chunk_t *patch_chunk_p,
                                int compression,
                                detools_state_read_t state_read)
{
    (void)state_read;

    int res;

    res = 0;
    *self_p = *dumped_p;
    self_p->patch_chunk_p = patch_chunk_p;

    self_p->destroy = patch_reader_heatshrink_destroy;
    self_p->decompress = patch_reader_heatshrink_decompress;

    return (res);
}

/**
 * Try to decompress given number of bytes.
 *
 * @return zero(0) if at least one byte was decompressed, one(1) if
 *         zero bytes were decompressed and more input is needed, or
 *         negative error code.
 */
static int patch_reader_decompress(
    struct detools_apply_patch_patch_reader_t *self_p,
    uint8_t *buf_p,
    size_t *size_p)
{
    return (self_p->decompress(self_p, buf_p, size_p));
}

/**
 * Unpack a size value.
 */
static int patch_reader_unpack_size(
    struct detools_apply_patch_patch_reader_t *self_p,
    int *size_p)
{
    int res;
    uint8_t byte;
    size_t size;

    size = 1;

    do {
        switch (self_p->size.state) {

        case detools_unpack_usize_state_first_t:
            res = patch_reader_decompress(self_p, &byte, &size);

            if (res != 0) {
                return (res);
            }

            self_p->size.is_signed = ((byte & 0x40) == 0x40);
            self_p->size.value = (byte & 0x3f);
            self_p->size.offset = 6;
            self_p->size.state = detools_unpack_usize_state_consecutive_t;
            break;

        case detools_unpack_usize_state_consecutive_t:
            res = patch_reader_decompress(self_p, &byte, &size);

            if (res != 0) {
                return (res);
            }

            if (is_overflow(self_p->size.offset)) {
                return (-DETOOLS_CORRUPT_PATCH_OVERFLOW);
            }

            self_p->size.value |= ((byte & 0x7f) << self_p->size.offset);
            self_p->size.offset += 7;
            break;

        default:
            return (-DETOOLS_INTERNAL_ERROR);
        }
    } while ((byte & 0x80) != 0);

    /* Done, fix sign. */
    self_p->size.state = detools_unpack_usize_state_first_t;

    if (self_p->size.is_signed) {
        self_p->size.value *= -1;
    }

    *size_p = self_p->size.value;

    return (res);
}

static int common_process_size(
    struct detools_apply_patch_patch_reader_t *patch_reader_p,
    size_t to_pos,
    size_t to_size,
    int *size_p)
{
    int res;

    res = patch_reader_unpack_size(patch_reader_p, size_p);

    if (res != 0) {
        return (res);
    }

    if (to_pos + (size_t)*size_p > to_size) {
        return (-DETOOLS_CORRUPT_PATCH);
    }

    return (res);
}

/*
 * Low level sequential patch type functionality.
 */

static int process_init(struct detools_apply_patch_t *self_p)
{
    int patch_type;
    uint8_t byte;
    int res;
    int to_size;

    if (chunk_get(&self_p->chunk, &byte) != 0) {
        return (-DETOOLS_SHORT_HEADER);
    }

    patch_type = ((byte >> 4) & 0x7);
    self_p->compression = (byte & 0xf);

    if (patch_type != PATCH_TYPE_SEQUENTIAL) {
        return (-DETOOLS_BAD_PATCH_TYPE);
    }

    res = chunk_unpack_header_size(&self_p->chunk, &to_size);

    if (res != 0) {
        return (res);
    }

    res = patch_reader_init(&self_p->patch_reader,
                            &self_p->chunk,
                            self_p->patch_size - self_p->chunk.offset,
                            self_p->compression);

    if (res != 0) {
        return (res);
    }

    if (to_size < 0) {
        return (-DETOOLS_CORRUPT_PATCH);
    }

    self_p->to_offset = 0;
    self_p->to_size = (size_t)to_size;

    if (to_size > 0) {
        self_p->state = detools_apply_patch_state_dfpatch_size_t;
    } else {
        self_p->state = detools_apply_patch_state_done_t;
    }

    return (res);
}

static int process_dfpatch_size(struct detools_apply_patch_t *self_p)
{
    int res;
    int size;

    res = patch_reader_unpack_size(&self_p->patch_reader, &size);

    if (res != 0) {
        return (res);
    }

    if (size > 0) {
        return (-DETOOLS_NOT_IMPLEMENTED);
    }

    self_p->state = detools_apply_patch_state_diff_size_t;

    return (0);
}

static int process_size(struct detools_apply_patch_t *self_p,
                        enum detools_apply_patch_state_t next_state)
{
    int res;
    int size;

    res = common_process_size(&self_p->patch_reader,
                              self_p->to_offset,
                              self_p->to_size,
                              &size);

    if (res != 0) {
        return (res);
    }

    self_p->state = next_state;
    self_p->chunk_size = (size_t)size;

    return (res);
}

static int process_data(struct detools_apply_patch_t *self_p,
                        enum detools_apply_patch_state_t next_state)
{
    int res;
    size_t i;
    uint8_t to[128];
    size_t to_size;
    uint8_t from[128];

    to_size = MIN(sizeof(to), self_p->chunk_size);

    if (to_size == 0) {
        self_p->state = next_state;

        return (0);
    }

    res = patch_reader_decompress(&self_p->patch_reader,
                                  &to[0],
                                  &to_size);

    if (res != 0) {
        return (res);
    }

    if (next_state == detools_apply_patch_state_extra_size_t) {
        res = self_p->from_read(self_p->arg_p, &from[0], to_size);

        if (res != 0) {
            return (-DETOOLS_IO_FAILED);
        }

        self_p->from_offset += to_size;

        for (i = 0; i < to_size; i++) {
            to[i] = (uint8_t)(to[i] + from[i]);
        }
    }

    self_p->to_offset += to_size;
    self_p->chunk_size -= to_size;

    res = self_p->to_write(self_p->arg_p, &to[0], to_size);

    if (res != 0) {
        return (-DETOOLS_IO_FAILED);
    }

    return (res);
}

static int process_diff_size(struct detools_apply_patch_t *self_p)
{
    return (process_size(self_p, detools_apply_patch_state_diff_data_t));
}

static int process_diff_data(struct detools_apply_patch_t *self_p)
{
    return (process_data(self_p, detools_apply_patch_state_extra_size_t));
}

static int process_extra_size(struct detools_apply_patch_t *self_p)
{
    return (process_size(self_p, detools_apply_patch_state_extra_data_t));
}

static int process_extra_data(struct detools_apply_patch_t *self_p)
{
    return (process_data(self_p, detools_apply_patch_state_adjustment_t));
}

static int process_adjustment(struct detools_apply_patch_t *self_p)
{
    int res;
    int offset;

    res = patch_reader_unpack_size(&self_p->patch_reader, &offset);

    if (res != 0) {
        return (res);
    }

    res = self_p->from_seek(self_p->arg_p, offset);

    if (res != 0) {
        return (-DETOOLS_IO_FAILED);
    }

    self_p->from_offset += offset;

    if (self_p->to_offset == self_p->to_size) {
        self_p->state = detools_apply_patch_state_done_t;
    } else {
        self_p->state = detools_apply_patch_state_diff_size_t;
    }

    return (res);
}

static int apply_patch_process_once(struct detools_apply_patch_t *self_p)
{
    int res;

    switch (self_p->state) {

    case detools_apply_patch_state_init_t:
        res = process_init(self_p);
        break;

    case detools_apply_patch_state_dfpatch_size_t:
        res = process_dfpatch_size(self_p);
        break;

    case detools_apply_patch_state_diff_size_t:
        res = process_diff_size(self_p);
        break;

    case detools_apply_patch_state_diff_data_t:
        res = process_diff_data(self_p);
        break;

    case detools_apply_patch_state_extra_size_t:
        res = process_extra_size(self_p);
        break;

    case detools_apply_patch_state_extra_data_t:
        res = process_extra_data(self_p);
        break;

    case detools_apply_patch_state_adjustment_t:
        res = process_adjustment(self_p);
        break;

    case detools_apply_patch_state_done_t:
        res = -DETOOLS_ALREADY_DONE;
        break;

    case detools_apply_patch_state_failed_t:
        res = -DETOOLS_ALREADY_FAILED;
        break;

    default:
        res = -DETOOLS_INTERNAL_ERROR;
        break;
    }

    if (res < 0) {
        self_p->state = detools_apply_patch_state_failed_t;
    }

    return (res);
}

static int apply_patch_common_finalize(
    int res,
    struct detools_apply_patch_patch_reader_t *patch_reader_p,
    size_t to_size)
{
    if (res == 1) {
        res = -DETOOLS_NOT_ENOUGH_PATCH_DATA;
    }

    if (res == -DETOOLS_ALREADY_DONE) {
        res = 0;
    }

    if (patch_reader_p->destroy != NULL) {
        if (res == 0) {
            res = patch_reader_p->destroy(patch_reader_p);
        } else {
            (void)patch_reader_p->destroy(patch_reader_p);
        }
    }

    if (res == 0) {
        res = (int)to_size;
    }

    return (res);
}

int detools_apply_patch_init(struct detools_apply_patch_t *self_p,
                             detools_read_t from_read,
                             detools_seek_t from_seek,
                             size_t patch_size,
                             detools_write_t to_write,
                             void *arg_p)
{
    self_p->from_read = from_read;
    self_p->from_seek = from_seek;
    self_p->patch_size = patch_size;
    self_p->patch_offset = 0;
    self_p->to_write = to_write;
    self_p->from_offset = 0;
    self_p->arg_p = arg_p;
    self_p->state = detools_apply_patch_state_init_t;
    self_p->patch_reader.destroy = NULL;

    return (0);
}

int detools_apply_patch_dump(struct detools_apply_patch_t *self_p,
                             detools_state_write_t state_write)
{
    int res;

    res = state_write(self_p->arg_p, self_p, sizeof(*self_p));

    if (res != 0) {
        return (-DETOOLS_IO_FAILED);
    }

    if (self_p->state == detools_apply_patch_state_init_t) {
        return (0);
    }

    return (patch_reader_dump(&self_p->patch_reader,
                              self_p->compression,
                              state_write));
}

int detools_apply_patch_restore(struct detools_apply_patch_t *self_p,
                                detools_state_read_t state_read)
{
    int res;
    struct detools_apply_patch_t dumped;

    res = state_read(self_p->arg_p, &dumped, sizeof(dumped));

    if (res != 0) {
        return (-DETOOLS_IO_FAILED);
    }

    self_p->state = dumped.state;
    self_p->patch_size = dumped.patch_size;

    if (self_p->state == detools_apply_patch_state_init_t) {
        return (0);
    }

    self_p->compression = dumped.compression;
    self_p->patch_offset = dumped.patch_offset;
    self_p->to_offset = dumped.to_offset;
    self_p->to_size = dumped.to_size;
    self_p->from_offset = dumped.from_offset;
    self_p->chunk_size = dumped.chunk_size;

    res = self_p->from_seek(self_p->arg_p, self_p->from_offset);

    if (res != 0) {
        return (-DETOOLS_IO_FAILED);
    }

    return (patch_reader_restore(&self_p->patch_reader,
                                 &dumped.patch_reader,
                                 &self_p->chunk,
                                 self_p->compression,
                                 state_read));
}

size_t detools_apply_patch_get_patch_offset(struct detools_apply_patch_t *self_p)
{
    return (self_p->patch_offset);
}

size_t detools_apply_patch_get_to_offset(struct detools_apply_patch_t *self_p)
{
    return (self_p->to_offset);
}

int detools_apply_patch_process(struct detools_apply_patch_t *self_p,
                                const uint8_t *patch_p,
                                size_t size)
{
    int res;

    res = 0;
    self_p->patch_offset += size;
    self_p->chunk.buf_p = patch_p;
    self_p->chunk.size = size;
    self_p->chunk.offset = 0;

    while (chunk_available(&self_p->chunk) && (res >= 0)) {
        res = apply_patch_process_once(self_p);
    }

    if (res == 1) {
        res = 0;
    }

    return (res);
}

int detools_apply_patch_finalize(struct detools_apply_patch_t *self_p)
{
    int res;

    self_p->chunk.size = 0;
    self_p->chunk.offset = 0;

    do {
        res = apply_patch_process_once(self_p);
    } while (res == 0);

    return (apply_patch_common_finalize(res,
                                        &self_p->patch_reader,
                                        self_p->to_size));
}

/*
 * Callback functionality.
 */

static int callbacks_process(struct detools_apply_patch_t *apply_patch_p,
                             detools_read_t patch_read,
                             size_t patch_size,
                             void *arg_p)
{
    int res;
    size_t patch_offset;
    size_t chunk_size;
    uint8_t chunk[512];

    res = 0;
    patch_offset = 0;

    while ((patch_offset < patch_size) && (res == 0)) {
        chunk_size = MIN(patch_size - patch_offset, 512);
        res = patch_read(arg_p, &chunk[0], chunk_size);

        if (res == 0) {
            res = detools_apply_patch_process(apply_patch_p,
                                              &chunk[0],
                                              chunk_size);
            patch_offset += chunk_size;
        } else {
            res = -DETOOLS_IO_FAILED;
        }
    }

    if (res == 0) {
        res = detools_apply_patch_finalize(apply_patch_p);
    } else {
        (void)detools_apply_patch_finalize(apply_patch_p);
    }

    return (res);
}

int detools_apply_patch_callbacks(detools_read_t from_read,
                                  detools_seek_t from_seek,
                                  detools_read_t patch_read,
                                  size_t patch_size,
                                  detools_write_t to_write,
                                  void *arg_p)
{
    int res;
    struct detools_apply_patch_t apply_patch;

    res = detools_apply_patch_init(&apply_patch,
                                   from_read,
                                   from_seek,
                                   patch_size,
                                   to_write,
                                   arg_p);

    if (res != 0) {
        return (res);
    }

    return (callbacks_process(&apply_patch, patch_read, patch_size, arg_p));
}

const char *detools_error_as_string(int error)
{
    if (error < 0) {
        error *= -1;
    }

    switch (error) {

    case DETOOLS_NOT_IMPLEMENTED:
        return "Function not implemented.";

    case DETOOLS_NOT_DONE:
        return "Not done.";

    case DETOOLS_BAD_PATCH_TYPE:
        return "Bad patch type.";

    case DETOOLS_BAD_COMPRESSION:
        return "Bad compression.";

    case DETOOLS_INTERNAL_ERROR:
        return "Internal error.";

    case DETOOLS_OUT_OF_MEMORY:
        return "Out of memory.";

    case DETOOLS_CORRUPT_PATCH:
        return "Corrupt patch.";

    case DETOOLS_IO_FAILED:
        return "Input/output failed.";

    case DETOOLS_ALREADY_DONE:
        return "Already done.";

    case DETOOLS_SHORT_HEADER:
        return "Short header.";

    case DETOOLS_NOT_ENOUGH_PATCH_DATA:
        return "Not enough patch data.";

    case DETOOLS_HEATSHRINK_SINK:
        return "Heatshrink sink.";

    case DETOOLS_HEATSHRINK_POLL:
        return "Heatshrink poll.";

    case DETOOLS_STEP_SET_FAILED:
        return "Step set failed.";

    case DETOOLS_STEP_GET_FAILED:
        return "Step get failed.";

    case DETOOLS_ALREADY_FAILED:
        return "Already failed.";

    case DETOOLS_CORRUPT_PATCH_OVERFLOW:
        return "Corrupt patch, overflow.";

    case DETOOLS_HEATSHRINK_HEADER:
        return "Heatshrink header.";

    default:
        return "Unknown error.";
    }
}
