/*
 * OneInTwo — Pasted files into the Linux browser. Status: documentary structure; pending
 * implementation.
 *
 * Responsibilities envisaged:
 * - Interpret supported browser formats and present locally accessible files.
 * - Coordinate MIME, URI and, where appropriate, local transfer portal permissions.
 * - Ask the file module for authorized content and keep resources while the browser needs them.
 * - Post via clipboard.cpp to avoid competing for clipboard property.
 * - Solve with P3 cache against delivery on demand; a remote textual path does not constitute a
 *   local file.
 */

