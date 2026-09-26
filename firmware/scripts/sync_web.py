"""Copy the browser app into LittleFS data before PlatformIO builds it."""

from pathlib import Path
import shutil

Import("env")

project = Path(env.subst("$PROJECT_DIR"))
data = Path(env.subst("$PROJECT_DATA_DIR"))

assets = (
    "index.html",
    "src/app.js",
    "src/config.js",
    "src/styles.css",
    "src/demo-controls.css",
    "src/api/MicrocontrollerApi.js",
)

for relative in assets:
    source = project / relative
    destination = data / relative
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
