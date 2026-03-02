Import("env")
import os

config = env.GetProjectConfig()
section = "env:" + env["PIOENV"]

# 1. Get the path from platformio.ini
asset_path = config.get(section, "custom_asset_path")

if asset_path:
    # 2. Force expansion of $PROJECT_DIR and convert to an absolute path
    # env.subst converts "$PROJECT_DIR" into "C:/Users/Name/Project"
    base_path = env.subst("$PROJECT_DIR")
    actual_path = os.path.abspath(os.path.join(base_path, asset_path))

    # 3. Update the environment variables used by the ESP32 build scripts
    env.Replace(PROJECT_DATA_DIR=actual_path)
    
    # This is for the 'buildfs' target specifically
    env["PROJECT_DATA_DIR"] = actual_path

    print(f"\n--- SCRIPT SUCCESS: Data Directory is now: {actual_path} ---\n")
else:
    print(f"\n--- SCRIPT WARNING: 'asset_path' not found in [{section}] ---\n")