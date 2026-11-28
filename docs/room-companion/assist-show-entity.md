# Assist: show pages and entities, switch presets

Expose three of the panel's selects to Assist (**Settings → Voice assistants →
Expose**) and a conversation agent can drive the panel without exposing each
entity:

| Select | Options | Choosing one |
| --- | --- | --- |
| **Page** | the carousel pages by name | shows that page |
| **Show entity** | every entity configured on the panel, by name | opens its detail screen |
| **Preset** | the saved presets | switches to that preset |

The options follow the panel's configuration. Each choice wakes the panel from
standby or low-power sleep, and when Assist made it the Assist overlay closes as
soon as the change is done; the spoken reply still plays.

An LLM conversation agent sets these selects directly ("show the desk lamp on
the panel", "switch the panel to the evening preset"). The default agent has no
built-in sentences for select options; add a custom sentence whose
`intent_script` calls `select.select_option` on the select.

## Actions for automations

- `esphome.<device>_show_entity` (`entity_id`, `detail`): shows the page of a
  configured entity, or its detail screen with `detail: true`. An entity that is
  not on the panel is ignored (logged as a warning).
- `esphome.<device>_apply_preset` (`name`): switches to a saved preset.

Home Assistant lists them under **Developer tools → Actions** once the panel
connects.
