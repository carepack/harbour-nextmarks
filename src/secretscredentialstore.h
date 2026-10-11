#pragma once

#include <QString>
#include <QVariantMap>
#include <QScopedPointer>

namespace Sailfish { namespace Secrets { class SecretManager; } }

// Stores one combined-JSON secret (loginName+appPassword) via Sailfish
// Secrets, synchronously. See docs/plans/2026-10-11-1438-feat-sailfish-
// secrets-credential-storage-plan.md (KTD1-KTD10) for why: standalone
// device-lock secret, synchronous startRequest()+waitForFinished() calls,
// explicit storage/encryption plugin names, one long-lived SecretManager.
//
// SecretManager is only forward-declared here (not included) so callers
// of this class -- nextcloudclient.h, and everything that includes it --
// don't transitively need the Sailfish Secrets headers. The destructor is
// declared (not defaulted inline) and defined in the .cpp, where
// SecretManager is complete, which QScopedPointer's own destructor needs.
class SecretsCredentialStore
{
public:
    // Locked is distinct from Failed: it means the daemon reported the
    // collection itself needs device-lock authentication we can't provide
    // with PreventInteraction. Confirmed on a real device (same issue hit
    // in harbour-readeck): this does not reliably clear on its own within
    // any bounded window post-boot, so the caller (NextcloudClient::
    // restoreSession()) reacts to it by retrying once with
    // allowInteraction=true, surfacing the platform's own unlock prompt
    // instead of waiting silently. Every other daemon-side error stays
    // Failed.
    enum class Outcome { Succeeded, NotFound, Locked, Failed };

    explicit SecretsCredentialStore(const QString &secretName);
    ~SecretsCredentialStore();

    Outcome store(const QVariantMap &fields);

    // allowInteraction lets the daemon show its own system unlock prompt
    // (SystemInteraction) instead of failing silently (PreventInteraction)
    // when the collection is still locked post-boot.
    Outcome load(QVariantMap *outFields, bool allowInteraction = false);
    Outcome remove();

private:
    Outcome removeInternal();

    QString m_secretName;
    QScopedPointer<Sailfish::Secrets::SecretManager> m_manager;
};
