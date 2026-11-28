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

## Without saying "on the panel"

Assign the panel to an area so Home Assistant knows where a request came from.

**LLM agent.** Add to the agent's instructions:

> When the user asks to show, open or display something, or to switch to a
> page or preset, and doesn't name a screen, use the Room Companion panel in
> the area the request came from: its **Show entity** select for an entity, its
> **Page** select for a page, its **Preset** select for a preset.

**Built-in agent.** A conversation-triggered automation acts on the panel that
heard the request (`trigger.device_id`):

```yaml
triggers:
  - trigger: conversation
    command: ["show {item}", "open {item}"]
actions:
  - action: select.select_option
    target:
      entity_id: >-
        {{ device_entities(trigger.device_id) | select('match', 'select\..*_show_entity$') | first }}
    data:
      option: "{{ item }}"
  - set_conversation_response: "Showing {{ item }}."
```

`{item}` is the spoken text, so it must match an option of the select exactly
(including capitalisation); an LLM agent copes with approximate names better.
Neither recipe has been tested on hardware yet.

## Actions for automations

- `esphome.<device>_show_entity` (`entity_id`, `detail`): shows the page of a
  configured entity, or its detail screen with `detail: true`. An entity that is
  not on the panel is ignored (logged as a warning).
- `esphome.<device>_apply_preset` (`name`): switches to a saved preset.

Home Assistant lists them under **Developer tools → Actions** once the panel
connects.
