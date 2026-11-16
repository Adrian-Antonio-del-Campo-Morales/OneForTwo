/*
 * OneInTwo — Text clipboard synchronization. Status: documentary structure; pending
 * implementation.
 *
 * Responsibilities envisaged:
 * - Manage local and remote changes, supported formats, limits and echo prevention.
 * - Assign operation identifiers and a deterministic logical order to resolve concurrent copies.
 * - Distinguish notifications of own scripts from new copies of the user.
 * - Apply text using the desktop contract and post changes using the authorized session.
 * - Invalid pending transactions when switching session and not forwarding old content when
 *   reconnecting.
 * - The offers and file content belong to files.cpp; do not write the clipboard by a second native
 *   way.
 */

