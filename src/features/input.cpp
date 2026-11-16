/*
 * OneInTwo — Keyboard and mouse control. Status: documentary structure; pending implementation.
 *
 * Responsibilities envisaged:
 * - Maintain local states, transition, remote and recovery, separated from the general state of
 *   connection.
 * - Map screens and edges, transmit normalized events and return control to source.
 * - Keep active keys/buttons, sequence and generation of control; discard obsolete events.
 * - Coordinate capture and release using the desktop contract without running network on your
 *   callbacks.
 * - Implement security timer on the receiver and local recovery at source.
 * - Release states for loss of connection, blocking, revocation or closure; define local physical
 *   input behavior.
 * - Do not convert all events into text or assume that all platforms support start capture with a
 *   shortcut.
 */

