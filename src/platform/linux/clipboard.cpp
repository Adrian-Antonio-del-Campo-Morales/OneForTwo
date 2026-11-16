/*
 * OneInTwo — Linux clipboard ownership. Status: documentary structure; pending implementation.
 *
 * Responsibilities envisaged:
 * - Read, write and observe formats supported by the validated backend.
 * - Report whether access depends on focus, active capture, permission or session; do not simulate
 *   permanent availability.
 * - Sort text and file offers and coordinate single post with file_clipboard.cpp.
 * - Respect property and life of offers, descriptors and local transfers.
 * - The independent clipboard track is pending P2; do not implement external commands as a product
 *   engine.
 */

