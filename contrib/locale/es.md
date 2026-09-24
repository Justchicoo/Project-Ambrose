<!-- Project Ambrose by Imjustchico: Spanish catalog for Ambrose-owned dashboard and operator text. -->

# Español (`es`)

This catalog translates text owned by Project Ambrose. It does not translate
Wizard101 client or game text; that content remains in the operator's private
installation and is read at runtime.

## Common interface

| Key | English source | Spanish |
| --- | --- | --- |
| `common.cancel` | Cancel | Cancelar |
| `common.close` | Close | Cerrar |
| `common.copy` | Copy | Copiar |
| `common.retry` | Retry | Reintentar |
| `common.save` | Save | Guardar |
| `common.refresh` | Refresh | Actualizar |
| `common.loading` | Loading | Cargando |
| `common.unavailable` | Unavailable | No disponible |
| `common.unknown` | Unknown | Desconocido |
| `common.required` | Required | Obligatorio |

## Authentication and sessions

| Key | English source | Spanish |
| --- | --- | --- |
| `auth.sign_in` | Sign in | Iniciar sesión |
| `auth.sign_out` | Sign out | Cerrar sesión |
| `auth.password` | Password | Contraseña |
| `auth.invalid_credentials` | The identifier or password is incorrect. | El identificador o la contraseña no son correctos. |
| `auth.second_factor` | Enter your second-factor code. | Introduce tu código del segundo factor. |
| `auth.session_expired` | Your session expired. Sign in again. | Tu sesión ha caducado. Inicia sesión de nuevo. |
| `auth.too_many_attempts` | Too many attempts. Try again later. | Demasiados intentos. Vuelve a intentarlo más tarde. |

## Server operations

| Key | English source | Spanish |
| --- | --- | --- |
| `server.ready` | Ready to accept work. | Listo para aceptar trabajo. |
| `server.starting` | Starting | Iniciando |
| `server.running` | Running | En ejecución |
| `server.stopping` | Stopping | Deteniendo |
| `server.stopped` | Stopped | Detenido |
| `server.degraded` | Degraded | Degradado |
| `server.offline` | Offline | Desconectado |
| `server.maintenance` | Maintenance mode | Modo de mantenimiento |
| `server.restart` | Restart | Reiniciar |
| `server.shutdown` | Shut down | Apagar |
| `server.reload_config` | Reload configuration | Recargar configuración |

## Command palette and shortcuts

| Key | English source | Spanish |
| --- | --- | --- |
| `palette.open` | Open command palette | Abrir paleta de comandos |
| `palette.search` | Search commands, pages and objects | Buscar comandos, páginas y objetos |
| `palette.no_results` | No permitted results | No hay resultados permitidos |
| `palette.navigate` | Navigate results | Navegar por los resultados |
| `palette.execute` | Run selected action | Ejecutar la acción seleccionada |
| `palette.close` | Close command palette | Cerrar paleta de comandos |
| `palette.shortcut_sheet` | Show keyboard shortcuts | Mostrar atajos de teclado |
| `palette.permission_filtered` | Results are limited to your permissions. | Los resultados están limitados por tus permisos. |
| `shortcut.command_palette` | Command palette | Paleta de comandos |
| `shortcut.search` | Global search | Búsqueda global |
| `shortcut.help` | Keyboard shortcuts | Atajos de teclado |

## Health page

| Key | English source | Spanish |
| --- | --- | --- |
| `health.title` | System health | Estado del sistema |
| `health.last_updated` | Last updated {time} | Última actualización: {time} |
| `health.healthy` | Healthy | Correcto |
| `health.warning` | Warning | Advertencia |
| `health.critical` | Critical | Crítico |
| `health.unknown` | Unknown | Desconocido |
| `health.no_data` | No health data is available. | No hay datos de estado disponibles. |
| `health.stale` | This health data may be out of date. | Estos datos de estado pueden estar desactualizados. |
| `health.database` | Database | Base de datos |
| `health.storage` | Storage | Almacenamiento |
| `health.processes` | Processes | Procesos |
| `health.network` | Network | Red |
| `health.open_details` | Open health details | Abrir detalles del estado |

## Digest and overview

| Key | English source | Spanish |
| --- | --- | --- |
| `digest.title` | Daily digest | Resumen diario |
| `digest.generated_at` | Generated {time} | Generado: {time} |
| `digest.no_events` | No notable events in this period. | No hubo eventos destacables en este periodo. |
| `digest.alerts` | Alerts | Alertas |
| `digest.incidents` | Incidents | Incidentes |
| `digest.backups` | Backups | Copias de seguridad |
| `digest.schedules` | Scheduled work | Trabajo programado |
| `digest.view_all` | View all activity | Ver toda la actividad |
| `digest.period` | Period: {start}–{end} | Periodo: {start}–{end} |

## Alert notices

| Key | English source | Spanish |
| --- | --- | --- |
| `alert.title` | Alert | Alerta |
| `alert.active` | Active | Activa |
| `alert.acknowledged` | Acknowledged | Reconocida |
| `alert.muted` | Muted until {time} | Silenciada hasta {time} |
| `alert.acknowledge` | Acknowledge alert | Reconocer alerta |
| `alert.unacknowledge` | Remove acknowledgement | Quitar reconocimiento |
| `alert.mute` | Mute rule | Silenciar regla |
| `alert.history` | Alert history | Historial de alertas |
| `alert.acknowledged_by` | Acknowledged by {operator} at {time} | Reconocida por {operator} a las {time} |
| `alert.failed_backup` | Backup failed: {name} | Falló la copia de seguridad: {name} |
| `alert.failed_schedule` | Schedule failed: {name} | Falló la programación: {name} |
| `alert.delivery_failed` | Alert delivery failed; retrying. | Falló la entrega de la alerta; se reintentará. |
| `alert.no_secret_data` | This notice contains no secret or player personal data. | Este aviso no contiene secretos ni datos personales de jugadores. |

## Translation notes

- Imperative actions use the informal second-person form consistently in
  messages addressed to the signed-in operator.
- `{time}`, `{start}`, `{end}`, `{operator}`, and `{name}` are display values
  supplied by the dashboard and must remain unchanged.
- Dates, numbers, and durations should be formatted by the browser's locale
  facilities; translations must not hard-code their formatting.
- Server-sent notices remain in the server language. These keys cover only
  text rendered by the dashboard.
- The catalog intentionally contains no client-derived names, game dialogue,
  account identifiers, credentials, packet data, or installation paths.

## Review boundary

This is a wording proposal held for review until the dashboard's translation
loader defines its final catalog schema. A future loader must preserve keys,
placeholders, permission filtering, and fallback-to-English behavior when a
translation is missing.

## Safe failure messages

| Key | English source | Spanish |
| --- | --- | --- |
| `error.permission_denied` | You do not have permission for this action. | No tienes permiso para esta acciÃ³n. |
| `error.not_found` | The requested resource was not found. | No se encontrÃ³ el recurso solicitado. |
| `error.source_unavailable` | This data source is unavailable. | Esta fuente de datos no estÃ¡ disponible. |
| `error.stale_data` | This information may be out of date. | Esta informaciÃ³n puede estar desactualizada. |
| `error.operation_failed` | The operation failed. | La operaciÃ³n fallÃ³. |
| `error.correlation_id` | Reference: {correlation_id} | Referencia: {correlation_id} |

`{correlation_id}` is an opaque value and must remain unchanged. Status words are short labels; they are not evidence that an app or realm is healthy without the corresponding status data.
