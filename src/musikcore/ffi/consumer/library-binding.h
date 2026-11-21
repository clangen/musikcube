#pragma once

#include <musikcore/library.h>
#include <musikcore/library/LocalLibrary.h>

namespace musik { namespace core { namespace library { class RemoteLibrary; } } }
namespace musik { namespace core { namespace api { class Library; class Context; } } }

struct LibraryIdentity;

namespace musik { namespace core { namespace ffi {

    /*
     * The app's MasterLibrary is a mutable selector. An async request may
     * start on a worker after the selection changes, and a track reference
     * may be used much later by a playback or queue action. Looking up the
     * selection at either point could run the operation against a different
     * library. Track IDs are scoped to a library, so a matching numeric ID
     * in the new selection can silently refer to a different track.
     *
     * Core binds the ABI handle to the concrete LocalLibrary selected at
     * creation. Requests, search results, and track references retain that
     * backend independently of the handle. The caller owns the handle and
     * releases it with mc_library_destroy.
     *
     * For example, a search submitted against library A still runs against A
     * if the app selects library B before the request finishes. A reference to
     * track 42 from that search still resolves in A, even if B also has a
     * track 42. The view is responsible for ignoring completions from its
     * previous selection.
     *
     * The backend also supplies the app's typed query dispatcher and enforces
     * its lifecycle. A database path identifies a file, so opening it
     * directly would bypass the app library's closed state. If A closes,
     * subsequent reads through its retained handles fail even while its
     * database file remains accessible. The context owns completion delivery
     * and cancellation for requests across all library selections.
     */
    mc_result BindLocalLibrary(const std::shared_ptr<library::LocalLibrary>& library, mc_library** out);

    /* Capture the active connection generation so reconnect cannot retarget retained handles. */
    mc_result BindRemoteLibrary(const std::shared_ptr<library::RemoteLibrary>& library, mc_library** out);

    std::shared_ptr<const LibraryIdentity> CaptureLibrary(const ILibraryPtr& library);

} } }
