# Portable curve settings

Use Import JSON or Export JSON. Imports are fully validated before active settings
change. A malformed import leaves the current response intact. Files are limited
to 64 KiB; unknown presets/fields, wrong types, invalid ranges and unordered points
are rejected.

Schema 1 has six fields in current exports. The `mode` field is optional when
reading older files; a missing mode defaults to distance from center.

| Field | Meaning |
| --- | --- |
| `schema` | `1` |
| `preset` | Preset key; see the examples or the list below |
| `mode` | `distance` (default) or `axes` for separate X/Y response |
| `sticks` | `left` and `right` booleans |
| `hid_layout` | `auto`, `xy-rxry`, or `xy-zrz` |
| `points` | Exactly ten objects with numeric X/Y **percentages** |

Point X values increase strictly between 0 and 100. Point Y values can range from
0 to 100. The fixed (0,0) and (100,100) anchors are implicit and not counted in the
ten editable points. Points remain saved when a preset is selected and become
active when Custom is selected.

Preset keys: `linear`, `soft-exponential`, `exponential`, `strong-exponential`,
`soft-inverse`, `inverse-exponential`, `strong-inverse`, `smooth`, `smoother`,
`ease-out`, `precision`, `early-saturation`, `custom`.

[custom.json](../Examples/custom.json) sets the first editable point to
**horizontal 10%, vertical 50%**. In `distance` mode this maps a 10% distance from
center to 50%, preserving direction. In `axes` mode it applies independently to
each joystick axis. [axes-separately.json](../Examples/axes-separately.json) selects
the latter explicitly. [left-stick-only.json](../Examples/left-stick-only.json) leaves the
right stick unchanged. JSON files are portable data; they do not include a device
serial number or capture-scoped token and cannot execute a script.

Automatic preferences use the same schema in the Windows registry. The application
never creates an automatic `.json` settings file.
