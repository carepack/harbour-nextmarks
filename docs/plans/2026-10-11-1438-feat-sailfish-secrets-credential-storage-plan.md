---
title: Sailfish Secrets Credential Storage - Plan
type: feat
date: 2026-10-11
topic: sailfish-secrets-credential-storage
artifact_contract: ce-unified-plan/v1
product_contract_source: ce-brainstorm
execution: code
---

## Goal Capsule

- **Objective:** Neither harbour-nextmarks' stored login credentials exist in plaintext on disk, and no existing logged-in user is signed out by this change.
- **Means:** Port harbour-readeck's proven Sailfish Secrets C++ pattern — already shipped, reviewed, and fixed — to harbour-nextmarks' `NextcloudClient`.
- **Product authority:** Carried forward from the harbour-readeck Secrets work (same session, same user); no new product authority needed.
- **Open blockers:** None — the approach is fully settled by precedent.

## Product Contract

### Summary

Store harbour-nextmarks' login credentials (`loginName`, `appPassword`) via Sailfish Secrets instead of plaintext QSettings, reusing the exact pattern just shipped and reviewed for harbour-readeck — including the two correctness fixes that review found. Existing plaintext sessions migrate automatically on first launch after the update.

### Problem Frame

harbour-nextmarks currently writes `serverUrl`, `loginName`, and `appPassword` together into plaintext QSettings (`harbour-nextmarks.conf`) on every login. `appPassword` is a live credential — anyone with filesystem access to the device reads it directly, the same exposure harbour-readeck had before its own Secrets migration. The fix for that exposure is already designed, built, reviewed, and shipped once; this is the second of two planned applications of it (`readeck-first-then-nextmarks`, decided during the original brainstorm).

### Requirements

**Verification and preconditions**
- R1. Confirm `libsailfishsecrets` C++ devel headers and the storage/encryption plugin names are available for harbour-nextmarks' SailfishOS build targets before implementation — the same verification harbour-readeck's U1 ran, not assumed to carry over just because the SDK is shared.

**Credential storage**
- R2. `loginName` and `appPassword` persist as one combined secret via Sailfish Secrets; `serverUrl` stays in QSettings — it is not a credential.
- R3. All Secrets access goes through a dedicated C++ helper class with the same shape as harbour-readeck's `SecretsCredentialStore` (standalone device-lock secret, synchronous calls, explicit storage/encryption plugin names). Reimplemented in this project; no shared code between the two apps.

**Existing-user migration**
- R4. Existing plaintext `loginName`/`appPassword` migrates into Secrets automatically on first launch after the update; `serverUrl` is left in QSettings unchanged. If the one-time migration store fails, the plaintext is left in place and the same check retries on the next launch; regardless of that store's success or failure, the session uses the plaintext values for this run (mirrors R5's in-memory guarantee and harbour-readeck's shipped behavior — the user stays logged in either way).

**Failure handling and session integrity**
- R5. If Secrets is unreachable, the session stays usable in memory for that run only; nothing is ever written back to QSettings as a plaintext fallback.
- R6. A logout whose Secrets delete fails sets a retry flag for the next launch; that retry never falls through to loading a session. The flag clears whenever a later login in the same run successfully stores a new secret — fixing the exact gap harbour-readeck's post-implementation review found (a stale flag was deleting a freshly-created, legitimate session).
- R7. Logout unconditionally clears any leftover plaintext `loginName`/`appPassword` QSettings keys, independent of the Secrets delete outcome — fixing the exact gap harbour-readeck's review found (a user who logs out while a migration attempt is still pending must not be silently logged back in by a later migration retry).

**Packaging**
- R8. RPM packaging declares the new `libsailfishsecrets` runtime dependency; `harbour-nextmarks.desktop`'s `[X-Sailjail]` `Permissions=` gains `Secrets`.

### Key Decisions

- **C++, not QML** (session-settled: user-directed — chosen over a pure-QML `Sailfish.Secrets` approach: `Secret`/`Identifier` cannot be constructed from QML in this Qt version). Governs R3.
- **One combined secret, not two** (session-settled: user-directed — chosen over separate secrets for `loginName` and `appPassword`: simpler lifecycle, matches harbour-readeck's shipped shape). Governs R2.
- **In-memory-only fallback on Secrets failure, never plaintext** (session-settled: user-directed — chosen over writing a plaintext fallback: the whole point of this change is removing plaintext credentials from disk). Governs R5.
- **Automatic migration of existing plaintext sessions** (session-settled: user-directed — chosen over requiring a manual re-login: preserves existing users' sessions across the update). Governs R4.
- **No session timeout, no biometric re-authentication** (session-settled: user-directed — both explored and explicitly dropped during harbour-readeck's brainstorm; the same scope applies here, not reopened).
- **MyBackup integration stays out of scope** (session-settled: user-directed — the original motivation was letting users back up their account info, but Secrets alone already satisfies the real goal of not storing credentials in plaintext).
- **The two review-found correctness fixes are specified from the start** (session-settled: user-directed — harbour-readeck's post-implementation review found the pending-delete flag wasn't cleared on a successful re-login, and that logout didn't clear leftover plaintext from an interrupted migration; both fixes are written directly into R6/R7 rather than left to be rediscovered). Governs R6, R7.

<!-- ce-section: work-relationships -->
### How This Work Fits Together

This plan covers only the Sailfish Secrets credential-storage port. The invoking request also named two unrelated features — folder renaming and editable link entries (URL + title) — which the user chose to defer; they are not active scope here and nothing below should be read as committing to them.

- Rename folder — independent feature, no shared actor or data path with credential storage. Still to decide: scope and acceptance criteria (not yet brainstormed).
- Edit link entry (link + title fields) — independent feature, no shared actor or data path with credential storage. Still to decide: scope and acceptance criteria (not yet brainstormed).

A later brainstorm may plan either as its own unit and should cite this plan by path rather than duplicating its content.

### Scope Boundaries

- Deferred for later: a shared/reusable Secrets-helper library between harbour-readeck and harbour-nextmarks — each app reimplements its own copy of the pattern.
- Deferred for later: revoking the Nextcloud app password server-side on logout — no revoke call exists today in `NextcloudClient::logout()`; unrelated to the credential-storage mechanism.
- Outside this plan: MyBackup integration; session timeout; biometric re-authentication (all session-settled exclusions, see Key Decisions).
- Outside this plan: folder renaming and link-entry editing (see How This Work Fits Together).

### Dependencies / Assumptions

- Assumes harbour-nextmarks' SailfishOS build targets have the same `libsailfishsecrets` devel package availability already confirmed for harbour-readeck, since both projects build against the same shared SDK — to be verified during planning (R1), not assumed.
- Assumes harbour-nextmarks is single-account (one `serverUrl`/`loginName`/`appPassword` at a time), matching `NextcloudClient`'s current properties — no multi-account support exists or is being added by this work.

### Key Flows

- F1. Fresh login stores a new secret.
  - **Trigger:** User completes Nextcloud Login Flow v2.
  - **Steps:** `NextcloudClient` stores `{loginName, appPassword}` via Secrets; `serverUrl` persists in QSettings as it does today.
  - **Covers:** R2, R3.
- F2. App relaunch restores the session from Secrets.
  - **Trigger:** App start, no pending-delete flag, no leftover plaintext.
  - **Steps:** Secrets load returns the combined secret; the session resumes with no QSettings involvement for credentials.
  - **Covers:** R2, R3.
- F3. Existing plaintext session migrates on first launch after the update.
  - **Trigger:** App start finds leftover plaintext `loginName`/`appPassword`.
  - **Steps:** Store once into Secrets; on success remove the plaintext keys; on failure leave them in place and retry on the next launch.
  - **Covers:** R4.
- F4. Logout hits a transient Secrets failure, user re-logs in, then relaunches.
  - **Trigger:** Secrets delete fails during logout.
  - **Steps:** Retry flag is written before the blocking delete call; the user may log back in during the same run, which clears the flag on a successful store; on relaunch, a still-set flag retries the delete and never falls through to loading a session.
  - **Covers:** R6, R7.

### Acceptance Examples

- AE1. A leftover plaintext `loginName`/`appPassword`, after one launch, ends up stored in Secrets with the plaintext keys gone, and the user stays logged in. Covers R4.
- AE2. Secrets daemon unreachable during login — the session is usable for that run; `harbour-nextmarks.conf`'s `loginName`/`appPassword` keys are never written. Covers R5.
- AE3. Logout whose delete failed, followed by a successful re-login in the same run, followed by a relaunch — the new session survives instead of being silently deleted. Covers R6.
- AE4. Logout while a migration attempt is still pending, followed by a relaunch — the user stays logged out, not silently resurrected by the next migration retry. Covers R7.
- AE5. Migration store fails (Secrets daemon unreachable) with leftover plaintext present — the user stays logged in for that run from the plaintext values, and the plaintext keys remain in place for a retry on the next launch. Covers R4.

### Sources / Research

- harbour-readeck's shipped implementation: `carepack/harbour-readeck` PR #1 (merged to `main`), and its plan at `docs/plans/2026-10-10-1328-feat-sailfish-secrets-credential-storage-plan.md` in that repo — same pattern, same two review-found fixes now written directly into R6/R7 here.
- Current harbour-nextmarks credential storage: `src/nextcloudclient.cpp:69-80` (`restoreSession()`/`persistSession()`), `src/nextcloudclient.cpp:189-203` (`logout()`), `src/nextcloudclient.h:137-139` (`m_serverUrl`/`m_loginName`/`m_appPassword` fields).
- Shared Sailfish skill documenting the C++ Secrets pattern and its pitfalls: `claude-sailfish-skill/sailfishos-app/references/guidelines.md` (github.com/carepack/claude-sailfish-skill).
- Verified directly in this repo's shared SDK targets: `usr/lib*/pkgconfig/sailfishsecrets.pc` exists in all three build targets (`SailfishOS-5.1.0.11-i486.default`, `-aarch64.default`, `-armv7hl.default`) — the devel package harbour-readeck installed into the shared SDK already covers this project; `sfdk tools target package-search` reports "available" rather than "installed" for two of the three targets, which is a stale repo-metadata label, not the real file state.

---

## Planning Contract

Product Contract unchanged.

### Key Technical Decisions

- KTD1. **One combined JSON secret, `StandaloneDeviceLockSecret`, identifier `(name, "", <storage-plugin>)`.** Mirrors harbour-readeck's shipped shape 1:1 (session-settled: user-directed — chosen over two separate secrets, same reasoning as the Product Contract's Key Decision). Governs R2, R3.
- KTD2. **Explicit storage/encryption plugin names**, `org.sailfishos.secrets.plugin.storage.sqlite` / `org.sailfishos.secrets.plugin.encryption.openssl`. `StandaloneDeviceLockSecret` has no usable default plugin — already discovered the hard way in harbour-readeck's U1; not re-discovered here. Governs R1, R2.
- KTD3. **`SecretManager::PreventInteraction` on every request.** Mirrors harbour-readeck; the Secrets daemon must never surface its own UI. Governs R2, R3, R5, R6.
- KTD4. **`SecretManager::DeviceLockKeepUnlocked`.** Mirrors harbour-readeck's KTD6. Governs R2.
- KTD5. **One long-lived `SecretManager`, forward-declared and held via `QScopedPointer`.** Mirrors harbour-readeck's shipped, review-fixed shape directly (its first version leaked the Secrets headers transitively into `main.cpp`; reimplementing the already-fixed shape here avoids repeating that defect). Governs R3.
- KTD6. **Synchronous `startRequest()` + `waitForFinished()`, no event loop wiring.** Mirrors harbour-readeck's KTD4/KTD5 (session-settled: user-directed — chosen over async signal/slot wiring: keeps `NextcloudClient`'s existing synchronous session-restore timing unchanged). Governs R2, R3.
- KTD7. **No custom timeout wrapper around a genuinely-unreachable Secrets daemon.** Accept D-Bus's own activation timeout (confirmed in harbour-readeck as a bounded, if slow, tens-of-seconds wait) rather than building worker-thread abandon machinery for a rare failure mode (session-settled: user-directed — same decision carried from harbour-readeck, not re-litigated). Governs R5.
- KTD8. **`store()` deletes any existing secret at the identifier before storing.** A standalone secret has no in-place replace request type (`SecretAlreadyExistsError` on a second `store()` to the same identifier, confirmed via `secrets-tool` during harbour-readeck's U4). Governs R2, R4.
- KTD9. **Sailjail `Secrets` permission, verified through the real `invoker`-launched sandboxed path, not the raw dev binary.** An unsandboxed test run bypasses Sailjail entirely and hides the permission gap — this is exactly what happened during harbour-readeck's own verification. Governs R8.
- KTD10. **The two defects harbour-readeck's post-implementation review found are specified as day-one implementation requirements, not left to be rediscovered:** `persistCredential()`-equivalent clears the pending-delete flag on a successful store (R6); `logout()` unconditionally clears leftover plaintext credential keys regardless of the Secrets delete outcome (R7). Governs R6, R7.

### Assumptions

- `libsailfishsecrets-devel` and the matching runtime package are already present for all three build targets via the shared SDK (verified above); no fresh installation step is expected during implementation. If a target turns out to be missing it after all, that is a blocking discovery for U1, not an assumption to silently work around.
- harbour-nextmarks' `NextcloudClient` constructor runs `restoreSession()` synchronously before the QML tree loads, the same timing harbour-readeck's `ReadeckClient` already relied on — confirmed from `src/nextcloudclient.cpp:29`.

---

## Implementation Units

### U1. Add Secrets dependency wiring and the `SecretsCredentialStore` helper

- **Goal:** harbour-nextmarks can store, load, and delete one combined Secrets-backed credential, verified against the real daemon.
- **Requirements:** R1, R2, R3. KTD1, KTD2, KTD3, KTD4, KTD5, KTD6.
- **Dependencies:** none.
- **Files:** `CMakeLists.txt`, `rpm/harbour-nextmarks.spec`, `src/secretscredentialstore.h` (new), `src/secretscredentialstore.cpp` (new).
- **Approach:**
  1. `CMakeLists.txt`: add `pkg_search_module(SAILFISHSECRETS sailfishsecrets REQUIRED)`, add its include dirs and `${SAILFISHSECRETS_LDFLAGS}` to the `harbour-nextmarks` target, add `src/secretscredentialstore.cpp` to `add_executable`.
  2. `rpm/harbour-nextmarks.spec`: add `BuildRequires: pkgconfig(sailfishsecrets)`.
  3. `secretscredentialstore.h`/`.cpp`: reimplement harbour-readeck's shipped `SecretsCredentialStore` class verbatim in shape (`Outcome` enum, `store()`/`load()`/`remove()`, `removeInternal()` delete-before-store per KTD8, `SecretManager` forward-declared behind `QScopedPointer`) — same class name, no shared code between the two apps per the Product Contract.
- **Test scenarios:**
  - `secrets-tool --store-standalone-secret`/`--get-standalone-secret`/`--delete-standalone-secret` round-trip against the real daemon with the KTD2 plugin names, confirming they work unchanged for this project.
  - A throwaway diagnostic hook in `main.cpp` (removed before the unit is done) exercising `SecretsCredentialStore::store()`/`load()`/`remove()` directly, using disposable/synthetic test values (e.g. literal `"old"`/`"new"`, never a real Nextcloud app password): store then load returns the same `{loginName, appPassword}`; delete then load returns `NotFound`; a second `store()` at the same identifier succeeds (KTD8) and loads back the new value. The hook reports only a boolean match, never the raw value, so nothing resembling a credential reaches stdout/`qDebug()`/the journal.
- **Verification:** `sfdk build` succeeds for all three targets; the diagnostic hook's output confirms the round-trip; `git diff --stat src/main.cpp` is empty once the hook is removed; `journalctl` for the test run contains no printed secret value.

### U2. Wire `NextcloudClient` to `SecretsCredentialStore`

- **Goal:** `loginName`/`appPassword` persist via Secrets instead of plaintext QSettings, with both review-found defects fixed from the start.
- **Requirements:** R2, R5, R6, R7. KTD1, KTD3, KTD4, KTD6, KTD10. Covers AE2, AE3, AE4.
- **Dependencies:** U1.
- **Files:** `src/nextcloudclient.h`, `src/nextcloudclient.cpp`.
- **Approach:**
  1. Split `persistSession()` into `persistServerUrl()` (unchanged QSettings write, `serverUrl` only) and `persistCredential()` (calls `SecretsCredentialStore::store({loginName, appPassword})`; on success, clears the `pendingSecretDelete` flag — the R6 fix, built in now rather than added after a review finds it missing).
  2. `restoreSession()`: keep reading `serverUrl` from QSettings; read `loginName`/`appPassword` via `SecretsCredentialStore::load()` instead of QSettings.
  3. `logout()`: call `SecretsCredentialStore::remove()` before clearing in-memory state. Write the `pendingSecretDelete` flag *before* the blocking `remove()` call (not after) and flush it with an explicit `QSettings::sync()` immediately afterward, so a process killed mid-call still leaves a retry record on disk rather than relying on QSettings' default lazy write. Unconditionally clear any leftover plaintext `loginName`/`appPassword` QSettings keys regardless of the delete outcome — the R7 fix.
- **Test scenarios:**
  - Happy path: login stores the combined secret; relaunch restores `loginName`/`appPassword` from Secrets with no QSettings read for those two fields.
  - Covers AE2: Secrets daemon unreachable during login — session stays usable for that run; `harbour-nextmarks.conf`'s `loginName`/`appPassword` keys are never written.
  - Covers AE3: logout whose delete fails (daemon unreachable), successful re-login in the same run, then relaunch — the new session survives instead of being deleted (direct exercise of the R6 fix via the same throwaway-daemon-unreachable technique as harbour-readeck's U6).
  - Covers AE4: logout while a migration attempt is still pending, then relaunch — the user stays logged out, not silently resurrected (direct exercise of the R7 fix; this scenario needs U3's migration logic to be present first, so run it after U3 even though the fix itself lives in this unit).
  - Edge case: logging out twice in a row — the second logout has nothing to delete and behaves the same as a normal logout.
- **Verification:** the diagnostic-hook methodology from U1 (disposable test values, boolean-only reporting), extended to exercise `NextcloudClient::logout()`/`persistCredential()` directly (not only the raw `SecretsCredentialStore`), against a genuinely unreachable daemon (daemon process killed, not just `systemctl stop`, which self-heals via D-Bus activation — confirmed in harbour-readeck). Hook fully removed afterward.

### U3. Existing-user migration on launch

- **Goal:** users already running the currently-shipped plaintext version keep their session across the update; the Sailjail permission gap harbour-readeck found is addressed from the start, not rediscovered.
- **Requirements:** R4, R6, R8. KTD8, KTD9. Covers AE1, AE5.
- **Dependencies:** U1, U2.
- **Files:** `src/nextcloudclient.cpp` (constructor / `restoreSession()`), `harbour-nextmarks.desktop`.
- **Approach:**
  1. `harbour-nextmarks.desktop`: add `Secrets` to `[X-Sailjail]` `Permissions=` (currently `Permissions=Internet`).
  2. In `restoreSession()`, before the ordinary Secrets load: if `pendingSecretDelete` is set, retry the delete and return without loading regardless of outcome (same shape as harbour-readeck's U4, never falls through to a load after this branch — this is R6's second clause). Otherwise, if the plaintext `loginName`/`appPassword` QSettings keys are still non-empty, migrate them into Secrets once; on success remove the plaintext keys. Regardless of that store's success or failure, set the in-memory `loginName`/`appPassword` from the plaintext values so the session stays usable this run (R4); on failure the plaintext keys are left in place and the same check retries on the next launch.
- **Test scenarios:**
  - Covers AE1: a `harbour-nextmarks.conf` seeded with leftover plaintext `loginName`/`appPassword`, after one launch, ends with both in Secrets and the plaintext keys gone, user still logged in.
  - Covers AE5: migration store fails (daemon unreachable) with leftover plaintext present — the user stays logged in for that run, the plaintext keys remain for a retry on the next launch.
  - Exercised through the real Sailjail-sandboxed launch (`invoker`), with the one-time interactive permission consent approved on-device — not only the unsandboxed dev binary, which would hide the exact gap KTD9 names.
  - Edge case: a fresh install with no leftover plaintext key skips migration and behaves like U2's ordinary load path.
- **Verification:** `harbour-nextmarks.conf` ends with only `serverUrl` after a successful migration launch; `secrets-tool --get-standalone-secret` returns the migrated JSON; the real sandboxed launch reaches the Secrets daemon without a permission error.

### U4. RPM packaging and changelog

- **Goal:** the build declares the new runtime dependency and ships a changelog entry a real user can read.
- **Requirements:** supports R1, R8.
- **Dependencies:** U1, U2, U3 all verified working.
- **Files:** `rpm/harbour-nextmarks.spec`, `rpm/harbour-nextmarks.changes`.
- **Approach:** add `Requires: libsailfishsecrets` alongside the `BuildRequires` added in U1; bump `Version:`; add one changelog entry, written as a single physical line (openrepos.net strips newlines between wrapped lines and separate bullets alike — confirmed on harbour-readeck's own listing) and phrased as a release note rather than an implementation summary, e.g. "Your login is now kept in Sailfish's own secure storage instead of a plain settings file; existing logins switch over automatically."
- **Test expectation:** none — packaging metadata and documentation, not application behavior.
- **Verification:** `sfdk build` succeeds for all three targets with the new `Requires` resolved; `rpm -qp --changelog` on the built RPM shows the new entry as one line.

---

## Verification Contract

| Check | Applicability | Command / Method |
|---|---|---|
| Build (all targets) | U1-U4 | `sfdk build` for `SailfishOS-5.1.0.11-{i486,aarch64,armv7hl}` |
| Secrets round-trip | U1 | `secrets-tool --store-standalone-secret` / `--get-standalone-secret` / `--delete-standalone-secret`, and the U1 diagnostic hook |
| Daemon-unreachable behavior | U2 | Kill the daemon process and `chmod 000` its binary (not `systemctl stop`, which self-heals), then exercise login/logout/relaunch via the U2 diagnostic hook |
| Sandboxed launch | U3 | Real `invoker`-launched run with the Sailjail `Secrets` permission consent approved on-device |
| Changelog rendering | U4 | `rpm -qp --changelog` on the built RPM; one physical line per entry |
| No test suite exists | all | No automated tests to run — this project, like harbour-readeck, is verified manually against the real daemon and real sandboxed launch, not by a CI test command |

## Definition of Done

- All four units implemented, built, and verified against the real Secrets daemon and the real Sailjail-sandboxed launch path (not only the unsandboxed dev binary).
- `harbour-nextmarks.conf` never contains `loginName`/`appPassword` after a successful login, fresh or migrated.
- AE1-AE5 all exercised and passing, including the two review-derived scenarios (AE3, AE4) that harbour-readeck's own verification missed on the first pass.
- Every throwaway diagnostic hook used for verification is fully removed (`git diff --stat src/main.cpp` empty) before the unit it supports is considered done.
- Any test credentials or device-side log files created during verification are deleted, and `journalctl` for the test run is checked for any printed secret value, not just files.
- RPM built for all three targets with the new dependency and a single-line, human-readable changelog entry.
