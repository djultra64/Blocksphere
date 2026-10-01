# Controls

Keyboard: arrow keys navigate, Z confirms, X cancels, and Enter starts or pauses. F11 and Alt+Enter toggle fullscreen. F1 opens or closes the RT64 inspector. When the inspector captures keyboard or mouse input, that input does not also reach the game.

Controller actions are assigned by **physical button position**. Prompts follow the most recently active device family. The south button is A on Xbox, Cross on PlayStation, and B on Nintendo Switch. The east button is B on Xbox, Circle on PlayStation, and A on Switch. West and north also exchange X/Y labels between Xbox and Switch. A button label must not be treated as the same physical position across families.

For an ambiguous controller, use `tetrisphere-config family xbox|playstation|nintendo-switch|keyboard`; `family auto` restores automatic detection. On Windows use `tetrisphere-config.exe` instead. To remap an action, use `bind confirm|cancel|face-left|face-up south|east|west|north`. Run `show` to inspect the saved mapping. These mappings are per action and persistent; prompts reflect the active family and physical assignment.

Two-player play requires independent input devices. Reconnection, prompts with specific physical controllers, and integrated ROG Ally navigation remain part of the pending hands-on review.
