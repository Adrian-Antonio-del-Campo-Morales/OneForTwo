/*
 * OneInTwo — File pasting on Windows. Status: documentary structure; pending implementation.
 *
 * Responsibilities envisaged:
 * - Interpret copied files and build representations that Explorer can consume.
 * - Research OLE objects, local files prepared and descriptors/contents on demand.
 * - Connect browser requests with authorized file module offers.
 * - Respect cancellation, asynchronous access and stream life; coordinate publication with
 *   clipboard.cpp.
 * - The selection between cache and provider is pending P3; this file does not implement network
 *   transfers.
 */

