from pathlib import Path
root = Path(__file__).resolve().parent.parent
html = (root / 'web/index.html').read_text()
assert ')PIXEL_UI"' not in html
(root / 'led_web/web_ui.h').write_text('#pragma once\n#include <Arduino.h>\nconst char WEB_UI[] PROGMEM = R"PIXEL_UI(' + html + ')PIXEL_UI";\n')
print(f'Embedded {len(html.encode())} bytes of UI')
