/*
 * OneInTwo — Authenticated and encrypted transport. Status: documentary structure; pending
 * implementation.
 *
 * Responsibilities envisaged:
 * - Manage connections, partial readings, writing, closing, and errors using a maintained TLS
 *   library.
 * - Separate interactive channel and bulky data channel with narrowed queues and flow control.
 * - Insert blocks of files and video fragments into the data channel.
 * - Display verified identity and connection events to the session, without automatically
 *   accepting unknown pairs.
 * - Apply reception limits before booking memory and avoid blocking desktop callbacks.
 * - Reject data from secondary channels not yet associated with an authorized session.
 * - Do not implement your own cryptography or decide on functional permissions; libraries and
 *   pending linking mechanism.
 */

