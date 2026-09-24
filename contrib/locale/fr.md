<!-- Project Ambrose by Imjustchico: French catalog for Ambrose-owned dashboard and operator text. -->

# Français (`fr`)

Ce catalogue traduit les textes affichés par Project Ambrose. Il ne traduit
pas les textes du client ni du jeu Wizard101 ; ces contenus restent dans
l'installation privée de l'opérateur et sont lus à l'exécution.

## Interface générale

| Key | English source | Français |
| --- | --- | --- |
| `common.cancel` | Cancel | Annuler |
| `common.close` | Close | Fermer |
| `common.copy` | Copy | Copier |
| `common.retry` | Retry | Réessayer |
| `common.save` | Save | Enregistrer |
| `common.refresh` | Refresh | Actualiser |
| `common.loading` | Loading | Chargement |
| `common.unavailable` | Unavailable | Indisponible |
| `common.unknown` | Unknown | Inconnu |
| `common.required` | Required | Obligatoire |

## Authentification et sessions

| Key | English source | Français |
| --- | --- | --- |
| `auth.sign_in` | Sign in | Se connecter |
| `auth.sign_out` | Sign out | Se déconnecter |
| `auth.password` | Password | Mot de passe |
| `auth.invalid_credentials` | The identifier or password is incorrect. | L'identifiant ou le mot de passe est incorrect. |
| `auth.second_factor` | Enter your second-factor code. | Saisissez votre code du second facteur. |
| `auth.session_expired` | Your session expired. Sign in again. | Votre session a expiré. Connectez-vous à nouveau. |
| `auth.too_many_attempts` | Too many attempts. Try again later. | Trop de tentatives. Réessayez plus tard. |

## Opérations du serveur

| Key | English source | Français |
| --- | --- | --- |
| `server.ready` | Ready to accept work. | Prêt à accepter des tâches. |
| `server.starting` | Starting | Démarrage |
| `server.running` | Running | En fonctionnement |
| `server.stopping` | Stopping | Arrêt en cours |
| `server.stopped` | Stopped | Arrêté |
| `server.degraded` | Degraded | Dégradé |
| `server.offline` | Offline | Hors ligne |
| `server.maintenance` | Maintenance mode | Mode maintenance |
| `server.restart` | Restart | Redémarrer |
| `server.shutdown` | Shut down | Éteindre |
| `server.reload_config` | Reload configuration | Recharger la configuration |

## Palette de commandes et raccourcis

| Key | English source | Français |
| --- | --- | --- |
| `palette.open` | Open command palette | Ouvrir la palette de commandes |
| `palette.search` | Search commands, pages and objects | Rechercher des commandes, pages et objets |
| `palette.no_results` | No permitted results | Aucun résultat autorisé |
| `palette.navigate` | Navigate results | Parcourir les résultats |
| `palette.execute` | Run selected action | Exécuter l'action sélectionnée |
| `palette.close` | Close command palette | Fermer la palette de commandes |
| `palette.shortcut_sheet` | Show keyboard shortcuts | Afficher les raccourcis clavier |
| `palette.permission_filtered` | Results are limited to your permissions. | Les résultats sont limités à vos autorisations. |
| `shortcut.command_palette` | Command palette | Palette de commandes |
| `shortcut.search` | Global search | Recherche globale |
| `shortcut.help` | Keyboard shortcuts | Raccourcis clavier |

## Page d'état

| Key | English source | Français |
| --- | --- | --- |
| `health.title` | System health | État du système |
| `health.last_updated` | Last updated {time} | Dernière mise à jour : {time} |
| `health.healthy` | Healthy | Sain |
| `health.warning` | Warning | Avertissement |
| `health.critical` | Critical | Critique |
| `health.unknown` | Unknown | Inconnu |
| `health.no_data` | No health data is available. | Aucune donnée d'état n'est disponible. |
| `health.stale` | This health data may be out of date. | Ces données d'état peuvent être obsolètes. |
| `health.database` | Database | Base de données |
| `health.storage` | Storage | Stockage |
| `health.processes` | Processes | Processus |
| `health.network` | Network | Réseau |
| `health.open_details` | Open health details | Ouvrir les détails de l'état |

## Résumé et aperçu

| Key | English source | Français |
| --- | --- | --- |
| `digest.title` | Daily digest | Résumé quotidien |
| `digest.generated_at` | Generated {time} | Généré : {time} |
| `digest.no_events` | No notable events in this period. | Aucun événement notable durant cette période. |
| `digest.alerts` | Alerts | Alertes |
| `digest.incidents` | Incidents | Incidents |
| `digest.backups` | Backups | Sauvegardes |
| `digest.schedules` | Scheduled work | Tâches planifiées |
| `digest.view_all` | View all activity | Voir toute l'activité |
| `digest.period` | Period: {start}–{end} | Période : {start}–{end} |

## Alertes

| Key | English source | Français |
| --- | --- | --- |
| `alert.title` | Alert | Alerte |
| `alert.active` | Active | Active |
| `alert.acknowledged` | Acknowledged | Reconnue |
| `alert.muted` | Muted until {time} | Désactivée jusqu'à {time} |
| `alert.acknowledge` | Acknowledge alert | Reconnaître l'alerte |
| `alert.unacknowledge` | Remove acknowledgement | Retirer la reconnaissance |
| `alert.mute` | Mute rule | Désactiver la règle |
| `alert.history` | Alert history | Historique des alertes |
| `alert.acknowledged_by` | Acknowledged by {operator} at {time} | Reconnue par {operator} à {time} |
| `alert.failed_backup` | Backup failed: {name} | Échec de la sauvegarde : {name} |
| `alert.failed_schedule` | Schedule failed: {name} | Échec de la tâche planifiée : {name} |
| `alert.delivery_failed` | Alert delivery failed; retrying. | Échec de l'envoi de l'alerte ; nouvelle tentative. |
| `alert.no_secret_data` | This notice contains no secret or player personal data. | Cet avis ne contient ni secret ni donnée personnelle de joueur. |

## Notes de traduction

- Les actions impératives utilisent systématiquement la forme de politesse
  pour l'opérateur connecté.
- `{time}`, `{start}`, `{end}`, `{operator}` et `{name}` sont des valeurs
  affichées fournies par le tableau de bord et doivent rester inchangées.
- Les dates, nombres et durées doivent être formatés par les fonctionnalités
  de langue du navigateur ; les traductions ne doivent pas imposer leur format.
- Les avis envoyés par le serveur restent dans la langue du serveur. Ces clés
  couvrent uniquement les textes rendus par le tableau de bord.
- Le catalogue ne contient volontairement aucun nom provenant du client,
  dialogue de jeu, identifiant de compte, donnée d'accès, donnée de paquet ou
  chemin d'installation.

## Limite de révision

Ce catalogue est une proposition de formulation en attente de révision,
jusqu'à ce que le chargeur de traductions du tableau de bord définisse son
schéma final. Un futur chargeur doit préserver les clés, les paramètres, le
filtrage des autorisations et le retour à l'anglais lorsqu'une traduction
manque.

## Messages d'erreur sûrs

| Key | English source | Français |
| --- | --- | --- |
| `error.permission_denied` | You do not have permission for this action. | Vous n'avez pas l'autorisation d'effectuer cette action. |
| `error.not_found` | The requested resource was not found. | La ressource demandée est introuvable. |
| `error.source_unavailable` | This data source is unavailable. | Cette source de données est indisponible. |
| `error.stale_data` | This information may be out of date. | Ces informations peuvent être obsolètes. |
| `error.operation_failed` | The operation failed. | L'opération a échoué. |
| `error.correlation_id` | Reference: {correlation_id} | Référence : {correlation_id} |

`{correlation_id}` est une valeur opaque qui doit rester inchangée. Les mots
d'état sont de courtes étiquettes ; ils ne prouvent pas qu'une application ou
un royaume est sain sans les données d'état correspondantes.
