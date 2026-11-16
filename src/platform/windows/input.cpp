/*
 * OneInTwo — Native Windows input. Status: documentary structure; pending implementation.
 *
 * Responsibilities envisaged:
 * - Capture keyboard/mouse events and delete their local effect when remote control is active.
 * - Inject authorized events and convert codes, buttons and coordinates from the common contract.
 * - Distinguish synthesized events to avoid loops and keep capture callbacks short.
 * - Release capture and states in the right thread; report bugs or privilege restrictions.
 * - Do not execute transfers or decide which equipment has control permission.
 */

