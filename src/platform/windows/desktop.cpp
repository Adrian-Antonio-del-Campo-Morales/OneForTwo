/*
 * OneInTwo — Windows desktop context. Status: documentary structure; pending implementation.
 *
 * Responsibilities envisaged:
 * - Create and close native session resources and send notifications to Windows components.
 * - Observe blocking, end of session and screen changes; publish geometry and real capabilities.
 * - Coordinate auxiliary windows, message loop and thread affinity required by APIs.
 * - Connect the common contract with clipboards, files, input, visual capture and warnings.
 * - Do not run the entire application as administrator or promise secure desktop control.
 */

