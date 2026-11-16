/*
 * OneInTwo — Native Linux input. Status: documentary structure; pending implementation.
 *
 * Responsibilities envisaged:
 * - Integrate capture and injection of validated Wayland backend through authorized sessions and
 *   EIS.
 * - Translate events and geometry, manage barriers, activation, deactivation and return control.
 * - Respect composer's decisions and report loss of permissions/capacitys.
 * - Release resources by closing and running recovery without waiting for the remote computer.
 * - Use portal_session.cpp for permissions; do not apply the logic of the network protocol or
 *   assume a universal activation shortcut.
 */

