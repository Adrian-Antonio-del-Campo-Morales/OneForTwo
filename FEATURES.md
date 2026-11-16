# OneInTwo — Proposed Functions

Design of November 13, 2026. The functions have initial contracts in the skeleton of the
application. The reference architecture describes the [session and its
permissions](ARCHITECTURE.md#41-session-and-messages).

| Function | User Action | Dependencia |
| --- | --- | --- |
| Direct submission of files | Choose files or folders in OneInTwo and send them to the linked computer. The recipient chooses where to save them. | Product session and file module |
| Sending links | Write or paste a URL into OneInTwo and send it. The recipient decides if to open it. | Product session and local opening of URL |
| Use profiles | Alternate between usual use, only shipments and pause from the main window. | Effective session permits and local preferences |
| [Share screen](docs/SHARE_SCREEN.md) | Show live a local monitor in a window of the other computer. | Monitor, session and viewer capture |
| [Share window](docs/SHARE_WINDOW.md) | Show only the selected window of an application. | Native window, session and viewer capture |
| [Notificaciones](docs/NOTIFICATIONS.md) | Send warnings and, when supported by the desktop, forward the selected applications. | Notification session and adapter |

## 1. Direct submission of files

**Starting range.** From OneInTwo, the source selects ordinary files and folders and clicks **Send
files**. The other computer displays the name of the linked sender, the first element name, the
total number of first-level items and the total size in bytes. The recipient can either reject the
offer or click **Save on...** and select a folder. Close the selector returns to the pending offer.
Both teams see progress and can cancel.

**Contract.** The session negotiates a specific capability and permission for direct sending. The
source creates an offer of files with "direct sending" mode, linked to the generation of session and
an offer identifier. The initial offer contains a narrowed summary. The recipient validates the
summary before displaying it. Accepting with a valid destination enables the manifest and blocks by
the common flow of files. The destination belongs to the recipient and remains local. The content
requests are limited to the elements of the accepted offer. The file module reuses its route rules,
collisions, temporal, integrity and cancellation.

**States.** Outstanding offer → accepted → transfer → verification → saved. From an active state you
can go on to reject, cancel or error as appropriate. Before accepting, a disconnection, a change of
generation or loss of permission withdraw the offer. After acceptance, those events cancel the
pending work and clean up your temporary work. The confirmed results before cancellation are
retained. A new attempt after a cut requires another offer and other acceptance.

**Criteria of acceptance**

- In both directions between Windows and Ubuntu, save a folder with Unicode names, subfolders and
  empty files. Compare structure, sizes and SHA-256.
- With the file clipboard disabled, complete a direct submission with your own permission. Refuse
  the same offer if the recipient disables the direct permission.
- Reject an offer, cancel the destination selection and cut an active transfer. Verify respectively
  that the transfer does not start, that the offer is shown again and that the temporary ones are
  cleaned without presenting incomplete results as saved.
- Repeat a shipment to the same destination and check that the common collision policy is applied
  and existing data are retained.

## 2. Sending links

**Started scope.** User enters a URL and clicks **Send link**. Recipient sees the name of linked
computer, host and full address in text, with actions **Open** and **Discard**. Open launches the
recipient computer's default browser using its local adapter. OneInTwo interface shows the offer
without loading previews or requesting content to the site.

**Contract.** The session applies a permission by specific sense of links. The offer takes a generation, a growing identifier within that generation and a URL of up to 4096 bytes in UTF-8. Both ends validate with the same analyzer of URL. Absolute addresses `http` and `https` are allowed with valid host. Embedded credentials, control characters and ill-formed addresses are rejected. The confirmation shows the standard URL and host in ASCII form when it is internationalized. When opening, exactly the validated URL shown is used. The content only lives in memory while the offer is active. Records keep the result type, without URL.

**States.** Each recipient maintains a maximum of a pending bid for 60 seconds since submitting it,
measured with monotonous clock. A second bid does not replace the slope. The receiver records the
last valid generation identifier, including offers discarded because it is occupied. Thus, a
repeated bid does not appear after releasing the previous one. Open or discard consumes the offer.
Disconnection, revocation, generation change and screen lock also remove it. Before opening,
permission, generation and validity are re-checked. If the system rejects the opening, the interface
indicates the local error. The sender's "Sending" status refers

**Criteria of acceptance**

- In both directions, present a URL HTTPS and verify that it opens only once when pressing **Open**,
  even if the action is repeated or the offer is doubled.
- Reject schemes other than HTTP and HTTPS, credentials in URL, control characters and addresses
  above limit. Check with a test adapter that the browser only receives the approved URL.
- Allow the offer to expire, block the screen and revoke the permission between presentation and
  confirmation. In each case, remove the pending action and check that the adapter does not receive
  an opening request.
- Confirm that a reconnection does not show the previous offer again and that the registration does
  not contain the URL.

## 3. Use profiles

**Starting range.** The main window offers three predefined profiles and shows what capabilities are
really available. Each team chooses its own profile for the linked pair.

| Perfil | Clipboards and entrance | Direct files, links and manual notices | Display, window and automatic forwarding |
| --- | --- | --- | --- |
| Habitual | Permitted by the profile | Permitted by the profile | Permitted by the profile |
| Only shipments | Pausados | Permitted by the profile | Pausados |
| Pausa | Pausados | Pausados | Pausados |

**Contract.** The profile limits both sending and receiving. The session calculates the effective
capabilities for each sense by intersecting the local profile, pair permissions, supported
capabilities on both computers and current restrictions on the desktop and the other end. A change
of profile never grants permissions. Direct sending permissions, links, screen, window and
notifications require explicit activation. The coordinator requests the change and modules receive
the capacity withdrawals. The session announces the result by means of the existing permission
control; the name of the profile is only saved in local preferences. The selected profile is
remembered in pairs. A pair without preference starts

**Transition.**The session closes first local access to the capabilities that come out of the
profile. The modules release keyboard and mouse, remove offers and cancel the affected jobs. Late
actions are discarded even if they belong to the same network generation. New capabilities enter
service after confirming the update of permissions in the ordered interactive channel. The
coordinator discards already glued work of the removed capabilities and modules invalidate their
identifiers. Data arriving through the secondary channel requires a transfer still active. **Pausa**
can retain the authenticated connection. Pause supports new operations and discards any attempt to
recover control, offers or content

**Criteria of acceptance**

- Switch to **Simply shipments** with an active remote control. Check the release of keys and
  buttons and check that an authorized direct shipment is still available.
- Go to **Pause** with an ongoing shipment and a pending link. Check the cancellation and cleaning,
  the withdrawal of the link offer and the rejection of late events.
- Return to **Standard** after you revoke a remote permission. Check that the capacity is still
  unavailable, that the rest is negotiated and that the selected profile is recovered when
  restarted.
- Simulate an unknown profile in preferences and check that **Pause** applies.

## Implementation points

| Function | Owner and contract | Integration |
| --- | --- | --- |
| Direct delivery | [File contract](src/features/files.hpp) and implementation in [files.cpp](src/features/files.cpp) | The window collects the selection and destination. The coordinator connects those actions with files. The session distinguishes the modality by authorizing offers, acceptance, blocks and cancellation. |
| Enlaces | [Link contract](src/features/links.hpp) and implementation in [links.cpp](src/features/links.cpp) | The protocol carries the offer. The session checks permissions and generation. The window presents the confirmation and the desktop adapter opens the approved URL. |
| Perfiles | [Profile contract](src/features/profiles.hpp) and rules in [profiles.cpp](src/features/profiles.cpp) | The coordinator applies the selection. [Session](src/core/session.cpp) calculates the actual permits and [storage](src/storage/local_store.cpp) retains the local choice. |

First you need the integrated session with permissions and revocation. From that base, profiles and
links can be validated with double desktop and transport. Direct sending also requires the common
transfer of files and can be checked on both computers by the destination selector.
