/*
 * OneInTwo — Meetings and permissions for portals. Status: documentary structure; implementation
 * pending.
 *
 * Responsibilities envisaged:
 * - Manage creation, authorization, restoration where appropriate, closing and D-Bus session
 *   signals.
 * - Share the life cycle between input and clipboards when APIs require it.
 * - Keep the technical capabilities granted and the functional permissions of OneInTwo separate.
 * - Deliver EIS connections to input.cpp and report loss of capacity to desktop.cpp.
 * - Do not request unnecessary devices to cover up the lack of independent clipboards.
 * - Minimum versions and specific strategy pending P0, P1 and P2.
 */

