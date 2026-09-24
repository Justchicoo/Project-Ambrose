<!-- Project Ambrose by Imjustchico: German catalog for Ambrose-owned dashboard and operator text. -->

# Deutsch (`de`)

Dieser Katalog übersetzt Texte, die Project Ambrose selbst anzeigt. Er
übersetzt weder Wizard101-Clienttext noch Spieltext; diese Inhalte bleiben in
der privaten Installation des Betreibers und werden zur Laufzeit gelesen.

## Allgemeine Oberfläche

| Key | English source | Deutsch |
| --- | --- | --- |
| `common.cancel` | Cancel | Abbrechen |
| `common.close` | Close | Schließen |
| `common.copy` | Copy | Kopieren |
| `common.retry` | Retry | Erneut versuchen |
| `common.save` | Save | Speichern |
| `common.refresh` | Refresh | Aktualisieren |
| `common.loading` | Loading | Wird geladen |
| `common.unavailable` | Unavailable | Nicht verfügbar |
| `common.unknown` | Unknown | Unbekannt |
| `common.required` | Required | Erforderlich |

## Anmeldung und Sitzungen

| Key | English source | Deutsch |
| --- | --- | --- |
| `auth.sign_in` | Sign in | Anmelden |
| `auth.sign_out` | Sign out | Abmelden |
| `auth.password` | Password | Passwort |
| `auth.invalid_credentials` | The identifier or password is incorrect. | Die Kennung oder das Passwort ist falsch. |
| `auth.second_factor` | Enter your second-factor code. | Geben Sie Ihren Code für den zweiten Faktor ein. |
| `auth.session_expired` | Your session expired. Sign in again. | Ihre Sitzung ist abgelaufen. Melden Sie sich erneut an. |
| `auth.too_many_attempts` | Too many attempts. Try again later. | Zu viele Versuche. Versuchen Sie es später erneut. |

## Serverbetrieb

| Key | English source | Deutsch |
| --- | --- | --- |
| `server.ready` | Ready to accept work. | Bereit, Aufträge anzunehmen. |
| `server.starting` | Starting | Wird gestartet |
| `server.running` | Running | Läuft |
| `server.stopping` | Stopping | Wird angehalten |
| `server.stopped` | Stopped | Angehalten |
| `server.degraded` | Degraded | Eingeschränkt |
| `server.offline` | Offline | Offline |
| `server.maintenance` | Maintenance mode | Wartungsmodus |
| `server.restart` | Restart | Neustart |
| `server.shutdown` | Shut down | Herunterfahren |
| `server.reload_config` | Reload configuration | Konfiguration neu laden |

## Befehlsleiste und Tastenkürzel

| Key | English source | Deutsch |
| --- | --- | --- |
| `palette.open` | Open command palette | Befehlsleiste öffnen |
| `palette.search` | Search commands, pages and objects | Befehle, Seiten und Objekte suchen |
| `palette.no_results` | No permitted results | Keine erlaubten Ergebnisse |
| `palette.navigate` | Navigate results | Ergebnisse durchsuchen |
| `palette.execute` | Run selected action | Ausgewählte Aktion ausführen |
| `palette.close` | Close command palette | Befehlsleiste schließen |
| `palette.shortcut_sheet` | Show keyboard shortcuts | Tastenkürzel anzeigen |
| `palette.permission_filtered` | Results are limited to your permissions. | Die Ergebnisse sind auf Ihre Berechtigungen beschränkt. |
| `shortcut.command_palette` | Command palette | Befehlsleiste |
| `shortcut.search` | Global search | Globale Suche |
| `shortcut.help` | Keyboard shortcuts | Tastenkürzel |

## Zustandsseite

| Key | English source | Deutsch |
| --- | --- | --- |
| `health.title` | System health | Systemzustand |
| `health.last_updated` | Last updated {time} | Zuletzt aktualisiert: {time} |
| `health.healthy` | Healthy | Einwandfrei |
| `health.warning` | Warning | Warnung |
| `health.critical` | Critical | Kritisch |
| `health.unknown` | Unknown | Unbekannt |
| `health.no_data` | No health data is available. | Keine Zustandsdaten verfügbar. |
| `health.stale` | This health data may be out of date. | Diese Zustandsdaten sind möglicherweise veraltet. |
| `health.database` | Database | Datenbank |
| `health.storage` | Storage | Speicher |
| `health.processes` | Processes | Prozesse |
| `health.network` | Network | Netzwerk |
| `health.open_details` | Open health details | Zustandsdetails öffnen |

## Übersicht und Zusammenfassung

| Key | English source | Deutsch |
| --- | --- | --- |
| `digest.title` | Daily digest | Tageszusammenfassung |
| `digest.generated_at` | Generated {time} | Erstellt: {time} |
| `digest.no_events` | No notable events in this period. | In diesem Zeitraum gab es keine bemerkenswerten Ereignisse. |
| `digest.alerts` | Alerts | Warnungen |
| `digest.incidents` | Incidents | Vorfälle |
| `digest.backups` | Backups | Sicherungen |
| `digest.schedules` | Scheduled work | Geplante Aufgaben |
| `digest.view_all` | View all activity | Alle Aktivitäten anzeigen |
| `digest.period` | Period: {start}–{end} | Zeitraum: {start}–{end} |

## Warnmeldungen

| Key | English source | Deutsch |
| --- | --- | --- |
| `alert.title` | Alert | Warnmeldung |
| `alert.active` | Active | Aktiv |
| `alert.acknowledged` | Acknowledged | Bestätigt |
| `alert.muted` | Muted until {time} | Stummgeschaltet bis {time} |
| `alert.acknowledge` | Acknowledge alert | Warnung bestätigen |
| `alert.unacknowledge` | Remove acknowledgement | Bestätigung entfernen |
| `alert.mute` | Mute rule | Regel stummschalten |
| `alert.history` | Alert history | Warnungsverlauf |
| `alert.acknowledged_by` | Acknowledged by {operator} at {time} | Von {operator} um {time} bestätigt |
| `alert.failed_backup` | Backup failed: {name} | Sicherung fehlgeschlagen: {name} |
| `alert.failed_schedule` | Schedule failed: {name} | Zeitplan fehlgeschlagen: {name} |
| `alert.delivery_failed` | Alert delivery failed; retrying. | Zustellung der Warnung fehlgeschlagen; neuer Versuch. |
| `alert.no_secret_data` | This notice contains no secret or player personal data. | Diese Meldung enthält keine Geheimnisse oder personenbezogenen Spielerdaten. |

## Übersetzungshinweise

- Imperative Aktionen verwenden durchgehend die höfliche Anrede für die
  angemeldete Bedienperson.
- `{time}`, `{start}`, `{end}`, `{operator}` und `{name}` sind Anzeigewerte
  des Dashboards und müssen unverändert bleiben.
- Datumsangaben, Zahlen und Zeitspannen formatiert die Locale des Browsers;
  Übersetzungen dürfen ihre Darstellung nicht fest vorgeben.
- Vom Server gesendete Meldungen bleiben in der Serversprache. Dieser Katalog
  deckt nur vom Dashboard gerenderten Text ab.
- Der Katalog enthält absichtlich keine aus dem Client stammenden Namen,
  Spieldialoge, Kontokennungen, Zugangsdaten, Paketdaten oder Installationspfade.

## Prüfgrenze

Dieser Katalog ist ein Formulierungsvorschlag und bleibt zur Prüfung
zurückgestellt, bis der Übersetzungslader des Dashboards sein endgültiges
Katalogschema festlegt. Ein künftiger Lader muss Schlüssel, Platzhalter,
Berechtigungsfilterung und den Rückfall auf Englisch bei fehlender Übersetzung
beibehalten.

## Sichere Fehlermeldungen

| Key | English source | Deutsch |
| --- | --- | --- |
| `error.permission_denied` | You do not have permission for this action. | Sie haben keine Berechtigung für diese Aktion. |
| `error.not_found` | The requested resource was not found. | Die angeforderte Ressource wurde nicht gefunden. |
| `error.source_unavailable` | This data source is unavailable. | Diese Datenquelle ist nicht verfügbar. |
| `error.stale_data` | This information may be out of date. | Diese Informationen sind möglicherweise veraltet. |
| `error.operation_failed` | The operation failed. | Der Vorgang ist fehlgeschlagen. |
| `error.correlation_id` | Reference: {correlation_id} | Referenz: {correlation_id} |

`{correlation_id}` ist ein undurchsichtiger Wert und muss unverändert bleiben.
Statuswörter sind kurze Bezeichnungen; ohne die zugehörigen Zustandsdaten sind
sie kein Beleg dafür, dass eine Anwendung oder ein Realm gesund ist.
