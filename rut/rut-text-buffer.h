/* rut-text-buffer.h
 * Copyright (C) 2011 Collabora Ltd.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __RUT_TEXT_BUFFER_H__
#define __RUT_TEXT_BUFFER_H__

#include "rut-types.h"
#include "rut-closure.h"

G_BEGIN_DECLS

/**
 * SECTION:rut-text-buffer
 * @title: RutTextBuffer
 * @short_description: Text buffer for RutText
 *
 * The #RutTextBuffer class contains the actual text displayed in a
 * #RutText widget.
 *
 * A single #RutTextBuffer object can be shared by multiple #RutText
 * widgets which will then share the same text content, but not the cursor
 * position, visibility attributes, icon etc.
 *
 * #RutTextBuffer may be derived from. Such a derived class might allow
 * text to be stored in an alternate location, such as non-pageable memory,
 * useful in the case of important passwords. Or a derived class could
 * integrate with an application's concept of undo/redo.
 *
 */


#define RUT_TEXT_BUFFER_MAX_SIZE        G_MAXUSHORT

typedef struct _RutTextBuffer RutTextBuffer;
#define RUT_TEXT_BUFFER(x) ((RutTextBuffer *) x)
extern RutType rut_text_buffer_type;

/* PRIVATE */
void
_rut_text_buffer_init_type (void);

/**
 * rut_text_buffer_new:
 *
 * Create a new RutTextBuffer object.
 *
 * Return value: A new RutTextBuffer object.
 *
 */
RutTextBuffer *
rut_text_buffer_new (RutContext *ctx);

/**
 * rut_text_buffer_new_with_text:
 * @text: (allow-none): initial buffer text
 * @text_len: initial buffer text length, or -1 for null-terminated.
 *
 * Create a new RutTextBuffer object with some text.
 *
 * Return value: A new RutTextBuffer object.
 */
RutTextBuffer *
rut_text_buffer_new_with_text (RutContext *ctx,
                               const char *text,
                               int text_len);

/**
 * rut_text_buffer_get_bytes:
 * @buffer: a #RutTextBuffer
 *
 * Retrieves the length in bytes of the buffer.
 * See rut_text_buffer_get_length().
 *
 * Return value: The byte length of the buffer.
 */
int
rut_text_buffer_get_bytes (RutTextBuffer *buffer);

/**
 * rut_text_buffer_get_length:
 * @buffer: a #RutTextBuffer
 *
 * Retrieves the length in characters of the buffer.
 *
 * Return value: The number of characters in the buffer.
 */
int
rut_text_buffer_get_length (RutTextBuffer *buffer);

/**
 * rut_text_buffer_get_text:
 * @buffer: a #RutTextBuffer
 *
 * Retrieves the contents of the buffer.
 *
 * The memory pointer returned by this call will not change
 * unless this object emits a signal, or is finalized.
 *
 * Return value: a pointer to the contents of the widget as a
 *      string. This string points to internally allocated
 *      storage in the buffer and must not be freed, modified or
 *      stored.
 */
const char *
rut_text_buffer_get_text (RutObject *buffer);

/**
 * rut_text_buffer_set_text:
 * @buffer: a #RutTextBuffer
 * @chars: the new text
 *
 * Sets the text in the buffer to the null-terminated string in @chars.
 *
 * This is roughly equivalent to calling rut_text_buffer_delete_text()
 * and rut_text_buffer_insert_text().
 */
void
rut_text_buffer_set_text (RutObject *buffer,
                          const char *chars);

/**
 * rut_text_buffer_set_text_with_length:
 * @buffer: a #RutTextBuffer
 * @chars: the new text
 * @n_chars: the number of characters in @text, or -1
 *
 * Sets the text in the buffer.
 *
 * This is roughly equivalent to calling rut_text_buffer_delete_text()
 * and rut_text_buffer_insert_text().
 *
 * Note that @n_chars is in characters, not in bytes.
 */
void
rut_text_buffer_set_text_with_length (RutTextBuffer *buffer,
                                      const char *chars,
                                      int n_chars);

/**
 * rut_text_buffer_set_max_length:
 * @buffer: a #RutTextBuffer
 * @max_length: the maximum length of the entry buffer, or 0 for no maximum.
 *   (other than the maximum length of entries.) The value passed in will
 *   be clamped to the range [ 0, %RUT_TEXT_BUFFER_MAX_SIZE ].
 *
 * Sets the maximum allowed length of the contents of the buffer. If
 * the current contents are longer than the given length, then they
 * will be truncated to fit.
 */
void
rut_text_buffer_set_max_length (RutObject *buffer,
                                int max_length);

/**
 * rut_text_buffer_get_max_length:
 * @buffer: a #RutTextBuffer
 *
 * Retrieves the maximum allowed length of the text in
 * @buffer. See rut_text_buffer_set_max_length().
 *
 * Return value: the maximum allowed number of characters
 *               in #RutTextBuffer, or 0 if there is no maximum.
 */
int
rut_text_buffer_get_max_length (RutTextBuffer *buffer);

/**
 * rut_text_buffer_insert_text:
 * @buffer: a #RutTextBuffer
 * @position: the position at which to insert text.
 * @chars: the text to insert into the buffer.
 * @n_chars: the length of the text in characters, or -1
 *
 * Inserts @n_chars characters of @chars into the contents of the
 * buffer, at position @position.
 *
 * If @n_chars is negative, then characters from chars will be inserted
 * until a null-terminator is found. If @position or @n_chars are out of
 * bounds, or the maximum buffer text length is exceeded, then they are
 * coerced to sane values.
 *
 * Note that the position and length are in characters, not in bytes.
 *
 * Returns: The number of characters actually inserted.
 */
int
rut_text_buffer_insert_text (RutTextBuffer *buffer,
                             int position,
                             const char *chars,
                             int n_chars);

/**
 * rut_text_buffer_delete_text:
 * @buffer: a #RutTextBuffer
 * @position: position at which to delete text
 * @n_chars: number of characters to delete
 *
 * Deletes a sequence of characters from the buffer. @n_chars characters are
 * deleted starting at @position. If @n_chars is negative, then all characters
 * until the end of the text are deleted.
 *
 * If @position or @n_chars are out of bounds, then they are coerced to sane
 * values.
 *
 * Note that the positions are specified in characters, not bytes.
 *
 * Returns: The number of characters deleted.
 */
int
rut_text_buffer_delete_text (RutTextBuffer *buffer,
                             int position,
                             int n_chars);

typedef void (*RutTextBufferInsertCallback) (RutTextBuffer *buffer,
                                             int position,
                                             const char *chars,
                                             int n_chars,
                                             void *user_data);

RutClosure *
rut_text_buffer_add_insert_text_callback (RutTextBuffer *buffer,
                                          RutTextBufferInsertCallback callback,
                                          void *user_data,
                                          RutClosureDestroyCallback destroy_cb);

typedef void (*RutTextBufferDeleteCallback) (RutTextBuffer *buffer,
                                             int position,
                                             int n_chars,
                                             void *user_data);

RutClosure *
rut_text_buffer_add_delete_text_callback (RutTextBuffer *buffer,
                                          RutTextBufferDeleteCallback callback,
                                          void *user_data,
                                          RutClosureDestroyCallback destroy_cb);

G_END_DECLS

#endif /* __RUT_TEXT_BUFFER_H__ */
