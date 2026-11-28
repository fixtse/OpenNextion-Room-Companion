# Assist: show pages and entities, switch presets

Expose three of the panel's selects to Assist (**Settings → Voice assistants →
Expose**) and a conversation agent can drive the panel without exposing each
entity:

| Select | Options | Choosing one |
| --- | --- | --- |
| **Page** | the carousel pages by name | shows that page |
| **Show entity** | every entity configured on the panel, by name | opens its detail screen |
| **Preset** | the saved presets | switches to that preset |

The options follow the panel's configuration. Home Assistant reads new **Page**
and **Show entity** options the next time it connects to the panel (saving a
slot does not drop the connection), so until then a new or renamed page or
entity may be missing from the select; the `show_entity` action accepts any
name at once. Each choice wakes the panel from
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
heard the request (`trigger.device_id`). This one understands English and
Spanish:

- "show calentador", "muestra scroll y zoom": opens an entity (Show entity).
- "show the madrid page", "muestra la página madrid": shows a page.
- "switch to the evening preset", "cambia al modo noche": switches presets.

Spoken names match the select's options ignoring case, punctuation, leading
articles, `&`/"and"/"y" and `+`/"plus", so "show the scroll and zoom" finds
`Scroll & Zoom` and "show timer 2" finds `Timer (2)`. Without a device (typed
Assist, the companion app) it uses the panel only if there is exactly one. When
nothing matches it says so instead of "Done".

```yaml
alias: "Room Companion: show / page / preset by voice"
mode: queued
triggers:
  - trigger: conversation
    command:
      - "show {item}"
      - "switch to {item}"
      - "muestra {item}"
      - "cambia (a|al) {item}"
variables:
  es: "{{ trigger.sentence | lower is match('\\s*(muestra|cambia)') }}"
  said: "{{ trigger.slots.item | default('') }}"
  kind: >-
    {% set t = said | lower %}
    {% if t is search('\\b(preset|modo)\\b') %}preset
    {%- elif t is search('\\b(page|p[aá]gina)\\b') %}page
    {%- else %}entity{% endif %}
  panel: >-
    {% set src = device_entities(trigger.device_id) if trigger.device_id
                 else integration_entities('esphome') %}
    {% set found = src | select('match', 'select\\..*_show_entity$') | list %}
    {{ found[0] if found | length == 1 else '' }}
  target: >-
    {% if panel %}{{ panel[:-12] ~ {'entity': '_show_entity', 'page': '_page',
                                    'preset': '_preset'}[kind] }}{% endif %}
  name: >-
    {{ said | regex_replace('(?i)\\b(preset|modo|page|p[aá]gina)\\b', '')
            | regex_replace('\\s+', ' ') | trim }}
  option: >-
    {% macro norm(s) -%}
      {{ s | lower | replace('&', ' and ') | replace('+', ' plus ')
           | regex_replace('\\by\\b', 'and') | regex_replace('[^\\w ]', ' ')
           | regex_replace('^\\s*(the|an|el|la|los|las|al|del)\\s+', '')
           | regex_replace('\\s+', ' ') | trim }}
    {%- endmacro %}
    {% set ns = namespace(hit='') %}
    {% for o in (state_attr(target, 'options') or []) if target %}
      {% if not ns.hit and norm(o) == norm(name) %}{% set ns.hit = o %}{% endif %}
    {% endfor %}
    {{ ns.hit }}
actions:
  - choose:
      - conditions: "{{ not panel }}"
        sequence:
          - set_conversation_response: >-
              {{ 'No sé qué panel usar.' if es else 'I could not tell which panel to use.' }}
      - conditions: "{{ not option }}"
        sequence:
          - set_conversation_response: >-
              {{ ('No encontré ' ~ name ~ ' en el panel.') if es
                 else ('I could not find ' ~ name ~ ' on the panel.') }}
    default:
      - action: select.select_option
        target: { entity_id: "{{ target }}" }
        data: { option: "{{ option }}" }
      - set_conversation_response: >-
          {% if kind == 'preset' %}{{ ('Preset ' ~ option ~ (' aplicado.' if es else ' applied.')) }}
          {%- else %}{{ ('Mostrando ' if es else 'Showing ') ~ option ~ '.' }}{% endif %}
```

Notes:

- Sentence triggers win over Home Assistant's built-in sentences, so the
  recipe avoids common command verbs: "open the garage door" still reaches the
  built-in commands. Add another verb only if no built-in sentence starts with
  it.
- Pages need the word "page"/"página" and presets "preset"/"modo"; a page and an
  entity with the same name are told apart that way.
- Speech-to-Phrase does not learn wildcard sentences, so this needs a full
  speech-to-text engine (Home Assistant Cloud, Whisper, Azure and others).
- Tested with typed text sent as if from the 3.5" panel, not by voice on the
  panels.

## Actions for automations

- `esphome.<device>_show_entity` (`entity_id`, `detail`): shows the page of a
  configured entity, or its detail screen with `detail: true`. An entity that is
  not on the panel is ignored (logged as a warning).
- `esphome.<device>_apply_preset` (`name`): switches to a saved preset.

Home Assistant lists them under **Developer tools → Actions** once the panel
connects.
