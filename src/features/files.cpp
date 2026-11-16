#include "files.hpp"

/*
 * OneInTwo — Offers and file transfer. Status: documentary structure; pending implementation.
 *
 * Responsibilities envisaged:
 * - Create offers limited to files chosen or copied by the user; assign opaque identifiers.
 * - Manage acceptance, destination, narrowed manifest, block shipping, progress, cancellation and
 *   errors.
 * - Directly, wait for acceptance and destination selection before delivering the content by
 *   common transfer.
 * - Resolve authorized paths, collisions and file types; prevent leaks by manipulated links or
 *   components.
 * - Write temporary, check size and integrity and publish results without overwriting quietly.
 * - Coordinate the native glue adapter and the life of the cache, if that alternative is chosen.
 * - Cancel obsolete requests by changing the offer, permissions or session, defining the life of
 *   already accepted transfers.
 * - Do not delete originals, grant arbitrary access to routes or terminate native pasting with a
 *   shipping button.
 */

