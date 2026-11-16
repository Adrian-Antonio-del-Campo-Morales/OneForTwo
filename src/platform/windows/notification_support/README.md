# Native notifications in Windows

Private material from the notification package. The integrator incorporates these requirements into the product packaging; the application code does not need to know them except by the AppUserModelID shared in [`identity.hpp`](../../../features/notification_support/identity.hpp).

## Identidad

| Dato | Valor |
| --- | --- |
| Product AppUserModelID | `OneInTwo.Desktop` |
| Visible name | `OneInTwo` |
| Observation capacity | `userNotificationListener` |
| Minimum practical version | Windows 10 1809 (17763) or later |

The AppUserModelID is declared at `src/features/notification_support/identity.hpp` and must match
exactly that of direct access or package MSIX. A packaged application exposes its identity as
`<PackageFamilyName>!OneInTwo.Desktop`; the eco filter recognizes the family.

## Presentation

- **Packaging application (product path):** `ToastNotificationManager` uses the package identity. No
  direct access is required to display the notice, but the manifest must declare the application and
  its notification capability. Activation when pressing is not implemented: the notice lacks
  `launch`, buttons and actions, as required by the contract.
- **Unpacked compilation (development):** Windows requires direct access from the Start menu with
  `System.AppUserModel.ID = OneInTwo.Desktop`. Run, without elevation:

  ```powershell
  powershell -ExecutionPolicy Bypass -File register-unpackaged-toast.ps1 `
      -TargetPath "C:\ruta\OneInTwo.exe"
  ```

  Without that access, `CreateToastNotifier` may not release an exception and the notice will not be
  displayed. The native route must be visually confirmed.

## Observation of warnings of other applications

1. Pack with MSIX and incorporate to `AppxManifest.xml` the capacity of
   [`appxmanifest-capability.xml`](appxmanifest-capability.xml):

   ```xml
   <uap3:Capability Name="userNotificationListener" />
   ```

2. The adapter announces `observe=true` only if `GetCurrentPackageFullName` confirms package
   identity. Without it, `requestObservationPermission`, `discoverApplications` and `startObserving`
   complete with `unavailable` and the interface must show the function as unavailable.
3. To populate the list of applications, the interface calls
   `SharedNotifications::discoverApplications`. It is a standalone query: you can request consent,
   list applications with current notices and do not initiate observation or alter your baseline.
   The list can be refreshed when the user reopens the selection.
4. On the first start with an authorized forwarding and a non-empty list, the module calls
   `startObserving`. The adapter requests consent with `RequestAccessAsync` from the coordinator
   interface thread.
5. If the user withdraws consent in **Configuration → Privacy → Notifications**, the adapter detects
   it in its two-second survey, abandons subscription and delivers `state_changed(denied)`.
6. At the start of each subscription, the existing list with `GetNotificationsAsync` is read and
   used as a baseline: the notification center history is not forwarded. A reconnection or
   suspension return restarts observation and takes a new baseline.

## Integration requirements

- Call `winrt::init_apartment()` during the boot of the interface thread or ensure that Qt already
  initialized COM in that thread (Qt Widgets does so with `OleInitialize`). `RequestAccessAsync`
  must be invoked from that thread; the module already does so from the main executor.
- Connect `NotificationDependencies::report_error` to reflect the withdrawal of consent in the
  interface. Shared headers do not include an observation event to the coordinator; the adapter
  reports with `state_changed` to the module, which emits the local error `observación de
  notifications denied by desktop`. The coordinator must re-check
  `notificationCapabilities()` and update the status of the forwarding control.
- The interface gets the selection with `discoverApplications` and calls `setAllowedApplications`
  with the `NotificationApplication::app_id` chosen. The visible name is only used for attribution.
- Do not register toast enablers or actions: the receiver presents plain text and does not allow
  remote responses.

## MSIX Check Pack (CI)

The job `msix` the validation workflow compiles the native route with CRT static, pack it with [`msix/AppxManifest.xml`](msix/AppxManifest.xml) and [`msix/New-NativeCheckPackage.ps1`](msix/New-NativeCheckPackage.ps1)and publishes the artifact `oneintwo-notifications-msix` with:

- `OneInTwo-NativeCheck.msix` signed with an ephemeral test certificate.
- `OneInTwo-NativeCheck.cer` to trust him alone during the check.
- `install.txt` with installation and boot steps.
- `validation.txt`, `signature-verify.log`, `smoke.txt` and `MakeAppx`/`signtool` logs.

The script checks before signing that the manifest declares `userNotificationListener` in `uap3`, `runFullTrust` in `rescap`, full-trust application, executable and resources; then packaged with `MakeAppx` and signature with `signtool`. Then check the signature offline, without invoking `signtool verify` and the API WinTrust/Appx: extract `AppxSignature.p7x`, requires the header `PKCX`, decode the PKCS#7, validate the signature with `SignedCms.CheckSignature` and check that the signatory is the ephemeral certificate generated. `signature-verify.log` records the result and `validation.txt` includes `signature_crypto=verified`. Also check the container MSIX: packaged manifest, executable, resources and presence of `AppxSignature.p7x`. After that, the job performs a smoke test: the `.cer` in `LocalMachine\TrustedPeople`, install the package with `Add-AppxPackage`check that it is registered and remove it with `Remove-AppxPackage`. The result is saved in `smoke.txt` as `smoke=ok`, `smoke=unavailable` o `smoke=failed`; if the runner does not allow the operation or exhausts the time, the job continues. The confidence of the chain and visual check are still manual. `.pfx` private is not published.

The package uses the `OneInTwo.Notifications.NativeCheck` identity and the `NativeCheck`
`Application Id`. It is not the product package nor replaces the integrator manifest.

## Discovery of applications

### No package identity (automatic)

`notifications-windows-discovery-no-package` is a native executable that creates the adapter without
MSIX, checks that `notificationCapabilities().observe` is fake, calls `discoverApplications` and
requires `DesktopResult::unavailable` with an empty list. It does not provide notices or open any
dialogue. It runs on IC job `msvc` and can be repeated with:

```powershell
ctest --test-dir build/notifications -C Release `
      -R notifications-windows-discovery-no-package --output-on-failure
```

If the process has a package identity, the executable returns 77 and CTest marks it as omitted: that
variant corresponds to the following manual check.

### With real consent (manual, MSIX)

It requires installing the signed package of the `oneintwo-notifications-msix` device; it does not
run in IC because the runner does not install MSIX nor can it interact with the consent dialogue.
Steps:

1. Install the certificate and MSIX following `install.txt`, and open the NativeCheck application.
2. In the first call to `discoverApplications`, Windows shows the consent of **Configuration →
   Privacy → Notifications**. Accept it.
3. Confirm that the call completes once with `ready` and a list of applications with current
   notices. The list may be empty if there are no notices; it must not include OneInTwo or
   duplicates, and the discovery must not initiate observation or alter its baseline.
4. Withdraw consent in Settings, repeat the discovery and confirm that it completes with `denied` or
   `unavailable`, without list, without blocking the process and without initiating observation.
5. Register system, Windows version, result and if the dialog appeared only once.

## Native pending check

On a Windows computer with MSVC and Windows SDK 10.0.17763 or later:

```powershell
cmake -S cmake/notifications -B build/notifications `
      -DONEINTWO_BUILD_TESTS=ON -DONEINTWO_BUILD_NATIVE_NOTIFICATION_TESTS=ON
cmake --build build/notifications --config Release
ctest --test-dir build/notifications -C Release --output-on-failure
build/notifications/tests/notifications/Release/oneintwo_notifications_native_windows.exe
```

The tour prints the capabilities, consult `discoverApplications` (without package must return
`unavailable`), presents a manual warning attributed to `TEST-DEVICE`, starts the observation
if available and expects a new notice. For the packaged variant, download the artifact
`oneintwo-notifications-msix` and follow `install.txt`; thus the executable has package identity and
can request observation. It should be visually verified:

- [ ] The notice shows the title and body Unicode and line `From TEST-DEVICE`.
- [ ] There are no buttons, links or actions.
- [ ] No package or registered direct access, `presentNotice` complete with `unavailable` (exit code
      77) and nothing shown.
- [ ] With package and consent, `observe=true` and `startObserving` complete with `ready`.
- [ ] `discoverApplications` without complete package with `unavailable` (automatic test
      `notifications-windows-discovery-no-package`).
- [ ] With package and consent, `discoverApplications` complete with `ready`, lists applications
      with current notices, excludes OneInTwo and does not start observation.
- [ ] After withdrawal of consent, `discoverApplications` completes with `denied` or `unavailable`
      and does not deliver applications.
- [ ] A new notice of an authorized application arrives once with its visible name; the notices
      prior to the start are not forwarded.
- [ ] OneInTwo notices are not forwarded (without echo).
- [ ] When you withdraw consent in Settings, the observation stops in less than five seconds and the
      coordinator is informed.
- [ ] `withdrawNotices` withdraws own asset notices.
- [ ] Blocking the session withdraws effective permission and pending notices do not reappear upon
      resumption.

Register system, version, packaging method and result. The tests prior to this list do not certify
Windows.
